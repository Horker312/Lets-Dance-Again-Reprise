#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <Windows.h>
#ifdef GetObject
#undef GetObject
#endif
#include <bcrypt.h>
#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <unordered_map>
#include "SessionCore.h"
#include "SessionInputPolicy.h"
#include "PausePolicy.h"
#include "TrackData.h"
#include "ContentSelection.h"
#include "AudioDevice.h"
#include "OARAnimations.h"
#include "Interop.h"
#include "PhotoModeBridge.h"
#include "SettingsMenu.h"
#include "CameraStage.h"
#include "SongPicker.h"
#include "PerformanceActors.h"
#include "PerformanceHeadTracking.h"
#include "ActorManager.h"
#include "WorldInteraction.h"
#include "PlaybackBinding.h"
#include "PlaybackLoadDiagnostics.h"
#include "RecoveryPolicy.h"
#include "HUDGate.h"
#include "NativePlaybackControl.h"
#include "ScopedCategoryMute.h"
#include "InteractiveCamera.h"
#include "CameraRetirement.h"
#include "FovRestoration.h"
#include "WheelFov.h"
#include <thread>
#include "Version.h"
#include "Localization.h"
#include "RuntimeSupport.h"
#include "HookSafety.h"
#include "FacialTest.h"
#include "FacialPlayback.h"
#if !defined(ENABLE_SKYRIM_SE) || !defined(ENABLE_SKYRIM_AE) || defined(ENABLE_SKYRIM_VR)
#error LDA:R requires the matching SE+AE SDK (VR is not supported)
#endif
static_assert(sizeof(RE::hkbClipGenerator)==0x110);
static_assert(offsetof(RE::hkbClipGenerator,mode)==0x72&&offsetof(RE::hkbClipGenerator,localTime)==0xF0);
static_assert(offsetof(RE::hkaAnimationBinding,animation)==0x18);
static_assert(offsetof(RE::BShkbAnimationGraph,characterInstance)==0xC0);
static_assert(sizeof(RE::FreeCameraState)==0x50);
// Actor state is accessed through version-aware AsActorState(), never a fixed Actor offset.
static_assert(offsetof(RE::bhkCharacterController,flags)==0x218&&offsetof(RE::bhkCharacterController,gravity)==0x248);

namespace DS {
using namespace DanceStudio;
Facial::Test facialTest;Facial::Playback facialPlayback;
PerformanceActors performers;ActorManager actorManager;RE::TESFaction*participantFaction{};std::uint64_t rosterRevision{};
PerformanceHeadTracking performanceHeads;
std::atomic<unsigned>actorMode{};WorldInteractionGate actorWorldGate;WorldModeMonitor actorWorldMonitor;unsigned actorModeSlot{};RE::FormID actorModeCell{};double nextActorHint{};
std::array<std::unique_ptr<Facial::Playback>,MaxPerformers> actorFaces;
Facial::SharedEvaluation facialEvaluation;bool facialMetrics{},facialEvaluationWarned{};std::uint64_t facialApplications{},facialApplyNs{};unsigned facialMaxActors{};
constexpr char Forms[]="DanceStudioData.esl";
constexpr wchar_t Ini[]=L"Data\\SKSE\\Plugins\\DanceStudio.ini";
constexpr auto SongFolderRoot=L"Data\\meshes\\actors\\character\\animations\\OpenAnimationReplacer\\Dance Studio";
constexpr auto SongsRoot=L"Data\\SKSE\\Plugins\\DanceStudio\\Songs";
struct Config {double volume{.7},stallSeconds{5};unsigned focusPollMs{100};bool combatStop{true};}cfg;
HUDGate hud;ProgressWatch progressWatch;ScopedCategoryMute<RE::BGSSoundCategory> backgroundMute;std::vector<RE::BGSSoundCategory*> backgroundCategories;
bool Focused(){DWORD process{};auto window=GetForegroundWindow();if(!window)return false;GetWindowThreadProcessId(window,&process);return process==GetCurrentProcessId();}
Hotkeys::Store hotkeys;CameraFramingStore cameraFraming;
SongPicker picker(&cameraFraming);WheelFov wheelFov;WheelFovQueue wheelQueue;std::uint64_t catalogGeneration{};std::shared_ptr<const PickerChoice> pendingPick;StableStage stableStage;std::atomic_bool recordedCamera{true};double nextCollisionLog{},nextHelperRepark{},nextParkWarning{},maxPlayerStepSquared{};Vec previousPlayerPosition;unsigned protectionResets{};
PriorityRecovery recovery;bool recoveringGraph{};double nextPriorityLog{};bool sessionNative{};std::atomic<double> nextClipTrace{};
struct CatalogResult{std::vector<Song> songs;std::string summary;};
SessionCore core;std::string catalogSummary;std::vector<Song> songs;std::string selectedDanceId,selectedCameraId;std::future<CatalogResult> catalogFuture;std::future<Prepared> loadFuture;
std::unordered_map<std::string,bool> facialWarnings;
std::uint64_t loadEpoch{},catalogEpoch{};bool catalogPending{},available{},wasPaused{},installed{},controlsGated{},cameraOwned{},cameraCalibrated{},announced{},retryIdle{};
std::atomic_bool world{},inputGate{},running{},watchClips{},menuSuspended{};std::atomic<unsigned> commands{};std::atomic<SessionState> inputState{SessionState::Idle};std::atomic<DWORD> mainThread{};
bool developerTools{};
bool SettingsIdle(){return inputState.load()==SessionState::Idle;}
Hotkeys::SettingsMenu settings(hotkeys,SettingsIdle,[]{return PhotoMode::available.load();},&cameraFraming,[]{return developerTools;});
std::atomic<bool> engineFrozen{},photoActive{},userPaused{},graphPlayable{};
std::atomic<std::uint32_t> pauseReasons{PauseNoSession};std::uint32_t loggedPauseReasons{~0u};
double photoRequestUntil{};bool cameraReturnPending{};
RE::TESGlobal* selector{};RE::TESNPC* cameraBase{};RE::ActorHandle cameraActor;RE::TESObjectCELL* stageCell{};
std::array<RE::NiPointer<RE::NiAVObject>,2> hiddenRoots;double nextMeshHide{};
RE::NiPointer<RE::NiNode> cameraAnchor;RE::BSTSmartPointer<RE::TESCameraState> savedState;RE::ActorHandle savedTarget;
std::uint64_t completedEngineUpdates{};std::atomic<std::uint64_t> menuEpoch{};std::uint64_t observedMenuEpoch{};
struct RetiringCamera {
 RE::NiPointer<RE::Actor> actor;RE::ActorHandle target;bool firstPerson{},failed{};
 std::array<RE::NiPointer<RE::NiAVObject>,2> roots;RE::NiPointer<RE::NiNode> anchor;
 double began{},lastDiagnostic{};CameraRetirementGate gate;
 RetiringCamera(RE::NiPointer<RE::Actor> a,RE::ActorHandle t,bool first):actor(std::move(a)),target(t),firstPerson(first),roots(hiddenRoots),anchor(cameraAnchor),gate(completedEngineUpdates){}
};
// A single pending helper is retained; a new recorded-camera performance waits
// rather than accumulating helpers while another camera owner prevents return.
std::optional<RetiringCamera> retiringCamera;
RE::BSTSmartPointer<RE::BSAnimationGraphManager> graphManager;RE::BSTSmartPointer<RE::BShkbAnimationGraph> playerGraph;
std::optional<Prepared> prepared;AudioDevice audio;Vec stage;Quat stageRotation,calibration;float savedFov{},ownedFov{};
using DeferredFov=FovRestoration;std::optional<DeferredFov> deferredFov;struct DeferredInteractive {RE::ActorHandle target;bool first;};std::optional<DeferredInteractive> deferredInteractive;double nextFovTrace{};unsigned calibrationTicks{};
std::string probedLoadPath;
double preparedAt{},lastGraphSeen{},resumeGraceUntil{},nextDiagnostic{};RE::NiPoint3 originalPosition;
struct PlayerHeightScale {
 RE::NiPointer<RE::NiAVObject> root,bone;
 double nextSearch{};HeightScale selected{1.,"default"};double reference{1},base{1},scene{},skeleton{};
 void Reset(){root.reset();bone.reset();nextSearch=0;selected={1.,"default"};}
 double Read(RE::Actor*player,double now){
  if(!player){Reset();return 1.;}
  reference=player->GetScale();base=player->GetBaseHeight();auto*current=player->Get3D(false);
  if(root.get()!=current){root.reset(current);bone.reset();nextSearch=0;}
  if(bone){auto*ancestor=bone.get();unsigned depth=0;while(ancestor&&ancestor!=root.get()&&depth++<64)ancestor=ancestor->parent;if(ancestor!=root.get()){bone.reset();nextSearch=0;}}
  if(root&&!bone&&now>=nextSearch){nextSearch=now+.5;bone.reset(root->GetObjectByName("NPC Root [Root]"));}
  // GetObjectByName points to a descendant held with the current model. Reset
  // at session cleanup/world changes; never measure animated bone positions.
  scene=root?root->world.scale:0;skeleton=bone?bone->world.scale:0;
  selected=ResolveHeightScale(skeleton,scene,base,reference);return selected.value;
 }
}playerHeightScale;SessionCameraScale sessionScale;
OAR_API::Animations::IAnimationsInterface* oar{};
struct ActorWatch {RE::ActorHandle actor;unsigned slot{};RE::BSTSmartPointer<RE::BSAnimationGraphManager>manager;RE::BSTSmartPointer<RE::BShkbAnimationGraph>graph;bool reconnectPending{};std::atomic<unsigned>generated{};std::atomic<double>generatedAt{};};
std::vector<std::shared_ptr<ActorWatch>> actorWatches;std::atomic<std::uint64_t>activePlaybackEpoch{};
struct Playback {std::shared_ptr<ActorWatch> watch;std::uint64_t epoch{};RE::hkbCharacter* character{};std::string submod;double duration{},seconds{};bool native{},advance{};};
using PlaybackSet=std::vector<Playback>;
std::atomic<std::shared_ptr<const PlaybackSet>> playback;
std::shared_ptr<const Playback> PlaybackFor(const RE::hkbContext&ctx){auto list=playback.load();if(list)for(auto&p:*list)if(p.character==ctx.character)return std::shared_ptr<const Playback>(list,&p);return {};}
struct View {RE::ActorHandle subject;RE::TESCameraState* state{};Vec position;Quat rotation;Vec origin;Quat basis;};std::atomic<std::shared_ptr<const View>> view;
struct Observation {std::string mod,submod,path;float duration{},generatorDuration{};bool hasControl{},hasControlBinding{};ClipLoadObservation load;};std::atomic<std::shared_ptr<const Observation>> observation;std::atomic<double> observationAt{};
std::atomic<std::uint64_t> observedEpoch{};std::atomic<unsigned> generated{};std::atomic<double> generatedAt{};
struct ClipSaved {std::uint64_t epoch{};RE::hkaAnimationBinding* binding{};decltype(RE::hkbClipGenerator::mode) mode;float speed{},fraction{};bool nativeStarted{};float local{},time{},controlTime{};decltype(RE::hkbClipGenerator::animationControl) control{};};
std::atomic<unsigned> clipCount{};std::mutex clipsLock;std::unordered_map<RE::hkbClipGenerator*,ClipSaved> clips;
double Now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void Notify(std::string text){SKSE::log::info("{}",text);RE::SendHUDMessage::ShowHUDMessage((std::string(ProductName)+": "+text).c_str());}
std::uint32_t ExternalPauseReasons(){auto*ui=RE::UI::GetSingleton();bool frozen=PhotoMode::Frozen();engineFrozen=frozen;return PauseMask({userPaused.load(),frozen,!ui||ui->GameIsPaused(),menuSuspended.load(),settings.Open(),!Focused(),true,true});}
void ApplyPlaybackPause(bool allowResume){auto reasons=ExternalPauseReasons()|(!graphPlayable.load()?PauseGraph:0)|(!running.load()?PauseNoSession:0);pauseReasons=reasons;audio.SyncPause(pauseReasons,allowResume);if(running.load()&&loggedPauseReasons!=reasons){loggedPauseReasons=reasons;SKSE::log::info("Playback pause reasons={} user={} engineFreeze={} timeline={}",reasons,userPaused.load(),engineFrozen.load(),core.Seconds());}}
// Stop-only fast path: animation callbacks can observe Photo Mode's direct
// freeze flag before the next published Playback snapshot. Never unfreeze it.
bool ObserveFreezeNow(){bool frozen=PhotoMode::Frozen();if(frozen){engineFrozen=true;auto old=pauseReasons.fetch_or(PauseFreeze);if(!(old&PauseFreeze))if(!BestEffort([]{audio.Pause(true);}))commands.fetch_or(4);}return frozen;}
void ToggleUserPauseNow(){bool previous=userPaused.load();while(!userPaused.compare_exchange_weak(previous,!previous)){}if(!previous){pauseReasons.fetch_or(PauseUser);if(!BestEffort([]{audio.Pause(true);}))commands.fetch_or(4);}commands.fetch_or(64);}
bool Executable(std::uintptr_t p){MEMORY_BASIC_INFORMATION m{};return p&&VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m))&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));}
RE::NiPoint3 Point(Vec p){return{static_cast<float>(p.x),static_cast<float>(p.y),static_cast<float>(p.z)};}
Vec Vector(RE::NiPoint3 p){return{p.x,p.y,p.z};}
Quat Quaternion(RE::NiQuaternion q){return Unit(Quat{q.x,q.y,q.z,q.w});}
RE::NiQuaternion Quaternion(Quat q){return{static_cast<float>(q.w),static_cast<float>(q.x),static_cast<float>(q.y),static_cast<float>(q.z)};}
std::string Hash(const std::vector<char>& bytes){BCRYPT_ALG_HANDLE alg{};BCRYPT_HASH_HANDLE hash{};auto check=[](NTSTATUS n){if(n<0)throw std::runtime_error(L10n::T("r0087"));};std::array<unsigned char,32> out{};try{check(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0));check(BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0));check(BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),static_cast<ULONG>(bytes.size()),0));check(BCryptFinishHash(hash,out.data(),32,0));}catch(...){if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);throw;}BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);std::string result;const char* h="0123456789abcdef";for(auto v:out){result+=h[v>>4];result+=h[v&15];}return result;}
Prepared Load(Song master,std::optional<Song> dance,std::optional<Song> camera,double volume){Prepared p;p.song=std::move(master);p.hasDance=dance.has_value();
 auto track=[](const Song&s,const char*key,std::uintmax_t max){auto name=s.manifest.at(key).get<std::string>();auto bytes=ReadBytes(RelativeFile(s.root,name),max);if(!SameDigest(Hash(bytes),s.manifest.at("hashes").at(name).get<std::string>()))throw std::runtime_error(L10n::T("r0088")+name);return bytes;};
 if(dance){for(const char*key:{"danceFemale","danceMale"})if(dance->manifest.contains(key))track(*dance,key,512ull*1024*1024);p.song.token=dance->token;p.song.submod=dance->submod;}else{p.song.token=0;p.song.submod.clear();}
 if(camera){auto bytes=track(*camera,"cameraSamples",384ull*1024*1024);p.camera=CameraTrack::Read(Json::parse(bytes),camera->duration);}auto music=track(p.song,"music",384ull*1024*1024);p.wave=ReadWave(music,p.song.duration);ApplyVolume(p.wave,volume);
 std::string facialPhase="invalid";if(p.song.manifest.contains("facialTimeline"))try{auto name=p.song.manifest.at("facialTimeline").get<std::string>();if(name!="facial.timeline.json"){facialPhase="unsupported";throw std::runtime_error("unsupported facial path");}if(!std::filesystem::exists(p.song.root/name)){facialPhase="missing";throw std::runtime_error("facial file missing");}facialPhase="integrity";auto bytes=track(p.song,"facialTimeline",2*1024*1024);auto facialJson=Facial::ParseFacial(bytes);facialPhase="unsupported";auto schema=facialJson.at("schemaVersion").get<int>();if(schema<1||schema>3)throw std::runtime_error("facial schema unsupported");facialPhase="invalid";p.facial=Facial::Timeline::Read(facialJson,p.song.duration);if(p.facial.legacySkipped)SKSE::log::warn("{} unsupported MFG modifier entries skipped (valid modifier range 0..13). Other morphs retained",p.facial.legacySkipped);SKSE::log::info("Facial timeline loaded: {} segments; legacy profiles are metadata; actual morph support depends on face resources",p.facial.segments.size());}catch(const std::exception&e){p.facial={};p.facialIssue=facialPhase;SKSE::log::warn("Facial file rejected; music/dance/camera retained: {}",e.what());}return p;}


CatalogResult Catalog(){std::vector<Song> found;size_t scanned=0,bad=0;std::uintmax_t metadata=0;bool limited=false;auto began=Now();
 for(auto root:{SongsRoot,SongFolderRoot}){std::error_code ec;if(!std::filesystem::exists(root,ec))continue;for(std::filesystem::directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)){if(ec){++bad;SKSE::log::warn("Catalog directory read failed: {}",ec.message());ec.clear();break;}const auto&e=*it;if(++scanned>4096||Now()-began>8||metadata>64*1024*1024){limited=true;break;}if(!e.is_directory(ec)||e.is_symlink(ec)||e.path().filename().wstring().starts_with(L".")){ec.clear();continue;}auto path=e.path()/"song.json";if(!std::filesystem::exists(path,ec)){ec.clear();continue;}try{metadata+=std::filesystem::file_size(path);auto song=ReadSong(path);if(root==SongFolderRoot&&!song.embedded)throw std::runtime_error(L10n::T("r0090"));for(const char*key:{"music","danceFemale","danceMale","cameraSamples"})if(song.manifest.contains(key)&&!std::filesystem::is_regular_file(RelativeFile(song.root,song.manifest.at(key).get<std::string>())))throw std::runtime_error(std::string("missing track: ")+key);
 if(song.HasDance()&&(root==SongFolderRoot||std::filesystem::exists(song.root/"config.json"))){auto config=ReadJson(song.root/"config.json",1024*1024);bool selector=false;if(config.value("name",std::string{})!=song.submod)throw std::runtime_error("OAR submod name mismatch");for(auto&condition:config.at("conditions"))if(condition.value("condition",std::string{})=="CompareValues"&&condition.contains("Value B")&&condition.at("Value B").value("value",0u)==song.token&&condition.at("Value A").at("form").value("pluginName",std::string{})=="DanceStudioData.esl"&&condition.at("Value A").at("form").value("formID",std::string{})=="800"&&condition.value("Comparison",std::string{})=="==")selector=true;if(!selector)throw std::runtime_error("OAR selector mismatch");}found.push_back(std::move(song));}catch(const std::exception&x){++bad;SKSE::log::warn("Rejected song {}: {}",PathText(e.path()),x.what());}}if(limited)break;}
 auto selected=SelectSongs(std::move(found));for(auto&entry:selected.collisions)SKSE::log::error("Excluded unsafe song selector collision; remove conflicting copies or re-export: {}",entry);for(auto&path:selected.ignored)SKSE::log::warn("Older/legacy copy of the same song ignored: {}",path);
 auto count=selected.songs.size();if(count>512){selected.songs.resize(512);limited=true;}std::string summary=std::string(L10n::T("qCatalog"))+std::to_string(selected.songs.size())+" / "+L10n::T("qRejected")+std::to_string(bad)+" / "+L10n::T("qOlder")+std::to_string(selected.ignored.size())+" / "+L10n::T("qCollisions")+std::to_string(selected.collisions.size());if(limited)summary+=L10n::T("qScanLimit");if(bad||!selected.collisions.empty())summary+=L10n::T("qSeeLog");SKSE::log::info("Catalog scan: folders={} metadataBytes={} validBeforeSelection={} unique={} old={} invalid={} collisions={} limit={}",scanned,metadata,count+selected.ignored.size()+selected.collisions.size(),count,selected.ignored.size(),bad,selected.collisions.size(),limited);return{std::move(selected.songs),summary};}

void ReadConfig(){facialMetrics=GetPrivateProfileIntW(L"Diagnostics",L"EnableFacialMetrics",0,Ini)!=0;developerTools=GetPrivateProfileIntW(L"Diagnostics",L"EnableDeveloperTools",0,Ini)!=0;auto read=[](const wchar_t* k,unsigned d){return GetPrivateProfileIntW(L"Keys",k,d,Ini);};
 auto error=hotkeys.Load({read(L"PrepareStart",68),read(L"Stop",0),read(L"ToggleHUD",87)},Hotkeys::MenuKey());
 if(!error.empty())SKSE::log::warn("{}",error);
 auto keys=hotkeys.Current();SKSE::log::info("Hotkeys DIK: play={}, stop={}",keys.start,keys.stop);
 cfg.volume=std::clamp(double(GetPrivateProfileIntW(L"Playback",L"VolumePercent",70,Ini))/100.,0.,1.);cfg.combatStop=GetPrivateProfileIntW(L"Playback",L"StopOnCombat",1,Ini)!=0;cfg.stallSeconds=std::clamp(double(GetPrivateProfileIntW(L"Recovery",L"AudioStallSeconds",5,Ini)),1.,120.);cfg.focusPollMs=std::clamp(GetPrivateProfileIntW(L"Recovery",L"FocusPollMilliseconds",100,Ini),20u,1000u);
 HUDPolicy policy;const auto path=std::filesystem::path(L"Data\\SKSE\\Plugins\\DanceStudio\\HUD.json");if(std::filesystem::exists(path)){try{auto j=ReadJson(path,16384);if(j.at("schemaVersion")!=1||!j.at("movies").is_array()||j.at("movies").size()>128)throw std::runtime_error(L10n::T("r0091"));std::vector<std::string> names;for(auto&v:j.at("movies")){auto name=v.get<std::string>();if(name.empty()||name.size()>128||HUDPolicy::FileName(name)!=name)throw std::runtime_error(L10n::T("r0092"));names.push_back(std::move(name));}if(j.value("includeDefaults",true)){for(auto&name:names)if(std::find(policy.movies.begin(),policy.movies.end(),name)==policy.movies.end())policy.movies.push_back(std::move(name));}else policy.movies=std::move(names);}catch(const std::exception&e){SKSE::log::warn("HUD configuration rejected; defaults used: {}",e.what());}}hud.Configure(std::move(policy));}


using ClipUpdateFn=void(*)(RE::hkbClipGenerator*,const RE::hkbContext&,float);
using ClipDeactivateFn=void(*)(RE::hkbClipGenerator*,const RE::hkbContext&);
// CommonLib's Generate declaration omits active children/output/timeOffset. This is OAR's five-argument ABI.
using ClipGenerateFn=void(*)(RE::hkbClipGenerator*,const RE::hkbContext&,const RE::hkbGeneratorOutput**,RE::hkbGeneratorOutput&,float);
ClipUpdateFn originalClipUpdate{};ClipDeactivateFn originalClipDeactivate{};ClipGenerateFn originalClipGenerate{};
void RestoreClip(RE::hkbClipGenerator* c,const ClipSaved&s){if(PlaybackBinding(c)!=s.binding)return;c->mode=s.mode;c->playbackSpeed=s.speed;c->userControlledTimeFraction=s.fraction;c->localTime=s.local;c->time=s.time;if(c->animationControl&&c->animationControl==s.control)c->animationControl->localTime=s.controlTime;}
bool ClockClip(RE::hkbClipGenerator*c,const RE::hkbContext&ctx,const std::shared_ptr<const Playback>& provided){
 auto p=provided;
 if(!p&&clipCount.load()==0)return false;
 std::scoped_lock lock(clipsLock);if(p&&p->epoch!=activePlaybackEpoch.load())p.reset();auto*activeBinding=PlaybackBinding(c);auto it=clips.find(c);
 if(it!=clips.end()&&(!p||it->second.epoch!=p->epoch||activeBinding!=it->second.binding)){RestoreClip(c,it->second);clips.erase(it);clipCount=static_cast<unsigned>(clips.size());it=clips.end();}
 if(!p||ctx.character!=p->character||!oar)return false;
 auto info=oar->GetCurrentReplacementAnimationInfo(c);
 double last=observationAt.load(),now=Now();if(now-last>1&&observationAt.compare_exchange_strong(last,now))observation.store(std::make_shared<Observation>(Observation{info.modName.c_str(),info.subModName.c_str(),info.animationPath.c_str(),activeBinding&&activeBinding->animation?activeBinding->animation->duration:0,c->binding&&c->binding->animation?c->binding->animation->duration:0,!!c->animationControl,!!activeBinding,ObserveClipLoad(c,ctx)}));
 const bool match=IsDanceMod(info.modName.c_str())&&std::string_view(info.subModName.c_str())==p->submod;
 if(!match||!PlaybackBindingMatches(activeBinding,p->duration)){if(it!=clips.end()){RestoreClip(c,it->second);clips.erase(it);clipCount=static_cast<unsigned>(clips.size());}return false;}
 if(it==clips.end()){if(clips.size()>=32)return false;it=clips.emplace(c,ClipSaved{p->epoch,activeBinding,c->mode,c->playbackSpeed,c->userControlledTimeFraction,false,c->localTime,c->time,c->animationControl?c->animationControl->localTime:0,c->animationControl}).first;clipCount=static_cast<unsigned>(clips.size());SKSE::log::info("Playback binding acquired: submod={} duration={} controlBinding={} generatorBinding={} native={}",p->submod,activeBinding->animation->duration,reinterpret_cast<std::uintptr_t>(activeBinding),reinterpret_cast<std::uintptr_t>(c->binding),p->native);}
 bool freezeNow=ObserveFreezeNow();
 if(p->native&&running.load()){
  ApplyNativePlayback(*c,it->second.nativeStarted,NativeMayAdvance(p->advance,freezeNow,pauseReasons.load()),RE::hkbClipGenerator::PlaybackMode::kModeLooping);return true;
 }
 float time=static_cast<float>(std::clamp(p->seconds,0.,std::max(0.,p->duration-.00001)));
 c->mode=RE::hkbClipGenerator::PlaybackMode::kModeUserControlled;c->playbackSpeed=0;c->userControlledTimeFraction=time/static_cast<float>(p->duration);
 c->localTime=time;c->time=time;if(c->animationControl)c->animationControl->localTime=time;return true;
}
template<class F>bool HookStep(const char*where,F&& f)noexcept{try{f();return true;}catch(const std::exception&e){commands.fetch_or(4);watchClips=false;try{SKSE::log::error("LDA hook {} failed; stop requested: {}",where,e.what());}catch(const std::exception&){}return false;}}
void ClipUpdate(RE::hkbClipGenerator*c,const RE::hkbContext&ctx,float dt){if((!watchClips.load()&&clipCount.load()==0)){originalClipUpdate(c,ctx,dt);return;}auto p=PlaybackFor(ctx);HookStep("ClipUpdate pre",[&]{ClockClip(c,ctx,p);});originalClipUpdate(c,ctx,dt);HookStep("ClipUpdate post",[&]{ClockClip(c,ctx,p);});}
void ClipDeactivate(RE::hkbClipGenerator*c,const RE::hkbContext&ctx){if(clipCount.load()==0){originalClipDeactivate(c,ctx);return;}HookStep("ClipDeactivate",[&]{std::scoped_lock lock(clipsLock);auto i=clips.find(c);if(i!=clips.end()){RestoreClip(c,i->second);clips.erase(i);clipCount=static_cast<unsigned>(clips.size());}});originalClipDeactivate(c,ctx);}
void ClipGenerate(RE::hkbClipGenerator*c,const RE::hkbContext&ctx,const RE::hkbGeneratorOutput** children,RE::hkbGeneratorOutput& output,float offset){if((!watchClips.load()&&clipCount.load()==0)){originalClipGenerate(c,ctx,children,output,offset);return;}auto p=PlaybackFor(ctx);auto*before=PlaybackBinding(c);bool match=false;HookStep("ClipGenerate pre",[&]{match=ClockClip(c,ctx,p);});originalClipGenerate(c,ctx,children,output,offset);HookStep("ClipGenerate post",[&]{if(match&&p&&PlaybackBinding(c)==before&&ClockClip(c,ctx,p)){p->watch->generated.fetch_add(1);p->watch->generatedAt=Now();if(observedEpoch.exchange(p->epoch)!=p->epoch)generated=0;generated.fetch_add(1);generatedAt=Now();double now=Now(),due=nextClipTrace.load();if(now>=due&&nextClipTrace.compare_exchange_strong(due,now+1.))SKSE::log::info("Playback comparison: native={} audioTime={} clipTime={} drift={} advance={}",p->native,p->seconds,c->localTime,c->localTime-p->seconds,p->advance);}});}
using CameraRotationFn=void(*)(RE::TESCameraState*,RE::NiQuaternion&);using CameraTranslationFn=void(*)(RE::TESCameraState*,RE::NiPoint3&);
CameraRotationFn originalRotation{};CameraTranslationFn originalTranslation{};
void CameraRotation(RE::TESCameraState*s,RE::NiQuaternion&q){bool applied=false;HookStep("CameraRotation",[&]{auto v=view.load();if(v&&!photoActive.load()&&!PhotoMode::Active()&&s==v->state){auto subject=v->subject.get();auto basis=subject?Yaw(-subject->GetAngleZ()):v->basis;auto next=Quaternion(FollowCameraRotation(v->rotation,v->basis,basis));q=next;applied=true;}});if(!applied)originalRotation(s,q);}
void CameraTranslation(RE::TESCameraState*s,RE::NiPoint3&p){bool applied=false;HookStep("CameraTranslation",[&]{auto v=view.load();if(v&&!photoActive.load()&&!PhotoMode::Active()&&s==v->state){auto subject=v->subject.get();Vec current=subject?Vector(subject->GetPosition()):v->origin;auto basis=subject?Yaw(-subject->GetAngleZ()):v->basis;auto next=std::isfinite(Dot(current,current))?Point(FollowCameraPosition(v->position,v->origin,v->basis,current,basis)):Point(v->position);p=next;applied=true;}});if(!applied)originalTranslation(s,p);}
using InputFn=RE::BSEventNotifyControl(*)(RE::PlayerControls*,RE::InputEvent*const*,RE::BSTEventSource<RE::InputEvent*>*);
InputFn originalPlayerInput{};
RE::BSEventNotifyControl PlayerInput(RE::PlayerControls*p,RE::InputEvent*const* e,RE::BSTEventSource<RE::InputEvent*>*s){if(!inputGate.load())return originalPlayerInput(p,e,s);if(!recordedCamera.load()||photoActive.load())return originalPlayerInput(p,e,s);p->data.moveInputVec={0,0};p->data.lookInputVec={0,0};p->data.prevMoveVec={0,0};p->data.prevLookVec={0,0};p->data.autoMove=false;
 return RE::BSEventNotifyControl::kContinue;}


bool InstallRuntimeHooks(){static bool failed=false;if(failed)return false;if(installed)return true;auto fail=[&](){failed=true;return false;};
 REL::Relocation<std::uintptr_t> clip{RE::VTABLE_hkbClipGenerator[0]},cam{RE::VTABLE_FreeCameraState[0]},pc{RE::VTABLE_PlayerControls[0]};
 for(auto [table,slot]:{std::pair{clip.address(),5u},{clip.address(),7u},{clip.address(),0x17u},{cam.address(),4u},{cam.address(),5u},{pc.address(),1u}})if(!ReadableRange(table+slot*8,8)||!Executable(reinterpret_cast<std::uintptr_t*>(table)[slot]))return fail();
 originalClipUpdate=reinterpret_cast<ClipUpdateFn>(reinterpret_cast<std::uintptr_t*>(clip.address())[5]);originalClipDeactivate=reinterpret_cast<ClipDeactivateFn>(reinterpret_cast<std::uintptr_t*>(clip.address())[7]);originalClipGenerate=reinterpret_cast<ClipGenerateFn>(reinterpret_cast<std::uintptr_t*>(clip.address())[0x17]);originalRotation=reinterpret_cast<CameraRotationFn>(reinterpret_cast<std::uintptr_t*>(cam.address())[4]);originalTranslation=reinterpret_cast<CameraTranslationFn>(reinterpret_cast<std::uintptr_t*>(cam.address())[5]);originalPlayerInput=reinterpret_cast<InputFn>(reinterpret_cast<std::uintptr_t*>(pc.address())[1]);
 auto hookOwner=[](const char* name,std::uintptr_t address){HMODULE module{};wchar_t filename[MAX_PATH]{};if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(address),&module))GetModuleFileNameW(module,filename,MAX_PATH);SKSE::log::info("Loader hook chain: {} previousModule={} address={:X}",name,PathText(std::filesystem::path(filename).filename()),address);};
 hookOwner("Update",reinterpret_cast<std::uintptr_t>(originalClipUpdate));hookOwner("Deactivate",reinterpret_cast<std::uintptr_t>(originalClipDeactivate));hookOwner("Generate",reinterpret_cast<std::uintptr_t>(originalClipGenerate));
 // Publish only after every chained function is initialized: NPC jobs may run concurrently.
 std::array<std::pair<std::uintptr_t,std::uintptr_t>,6> patches{{{clip.address()+5*8,reinterpret_cast<std::uintptr_t>(ClipUpdate)},{clip.address()+7*8,reinterpret_cast<std::uintptr_t>(ClipDeactivate)},{clip.address()+0x17*8,reinterpret_cast<std::uintptr_t>(ClipGenerate)},{cam.address()+4*8,reinterpret_cast<std::uintptr_t>(CameraRotation)},{cam.address()+5*8,reinterpret_cast<std::uintptr_t>(CameraTranslation)},{pc.address()+8,reinterpret_cast<std::uintptr_t>(PlayerInput)}}};std::array<std::uintptr_t,6> previous{};for(size_t i=0;i<patches.size();++i){if(!ReadableRange(patches[i].first,8))return fail();std::memcpy(&previous[i],reinterpret_cast<void*>(patches[i].first),8);}for(size_t i=0;i<patches.size();++i)if(!PatchMemory(patches[i].first,&patches[i].second,8)){for(size_t j=0;j<=i;++j)PatchMemory(patches[j].first,&previous[j],8);SKSE::log::error("Runtime vtable hook write failed; rollback attempted");return fail();}installed=true;SKSE::log::info("Chained clip, free-camera and player-only input hooks installed");return true;
}
RE::NiCamera* FindCamera(RE::NiAVObject* node,unsigned depth=0){if(!node||depth>16)return nullptr;if(auto*c=netimmerse_cast<RE::NiCamera*>(node))return c;if(auto*n=node->AsNode())for(auto&child:n->GetChildren())if(auto*c=FindCamera(child.get(),depth+1))return c;return nullptr;}
bool Calibrate(){if(!interop::CameraConfirmed())return false;auto* pc=RE::PlayerCamera::GetSingleton();auto* cam=FindCamera(pc->cameraRoot.get());if(!cam)return false;RE::NiPoint3 origin,direction;if(!cam->WindowPointToRay(500,500,origin,direction,1000,1000))return false;auto f=Vector(direction);if(Dot(f,f)<.01||std::abs(Unit(f).z)>.995)return false;
 // Built-in FreeCameraState has yaw/pitch and no roll. World up avoids screen-Y convention assumptions.
 SKSE::log::info("Camera calibration after free-state updates: SmoothCam={} owned={} ticks={} renderPosition=({},{},{})",interop::cameraPresent,interop::ownCamera,calibrationTicks,cam->world.translate.x,cam->world.translate.y,cam->world.translate.z);auto basis=Look(f,{0,0,1});RE::NiQuaternion q;originalRotation(pc->currentState.get(),q);calibration=Unit(basis.Inv()*Quaternion(q));return true;}

bool SafeWorld(){auto*p=RE::PlayerCharacter::GetSingleton();auto*ui=RE::UI::GetSingleton();return world&&p&&p->GetParentCell()&&p->Get3D(false)&&ui&&!ui->IsMenuOpen(RE::MainMenu::MENU_NAME)&&!ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);}
bool SafePlayer(){auto*p=RE::PlayerCharacter::GetSingleton();return SafeWorld()&&!p->IsDead()&&!p->AsActorState()->IsBleedingOut()&&!p->IsOnMount()&&!p->AsActorState()->IsSwimming()&&!p->AsActorState()->IsSneaking()&&static_cast<int>(p->AsActorState()->GetSitSleepState())==0&&static_cast<int>(p->AsActorState()->GetKnockState())==0&&(!cfg.combatStop||!p->IsInCombat());}
const char*participantIssue="aActorMissing";bool participantsWaiting{},participantsBound{};double participantWaitSince{-1};std::optional<Prepared> attachmentPending;double attachmentDeadline{};bool restageStartRequested{};std::atomic_bool beginPending{};double beginDeadline{};
void ValidateParticipantsForStart();void PublishActors();void RestoreParticipants(bool valid,bool clear);void BindParticipants();bool ParticipantsValid();void StartParticipants();
void Stop(const char* reason=nullptr,bool failed=false){{std::scoped_lock lock(clipsLock);activePlaybackEpoch=0;playback.store(nullptr);}facialPlayback.Stop();for(auto&f:actorFaces)if(f)f->Stop();facialTest.Close(true,"dance stop");wheelQueue.Disable();if(core.Stop(failed)&&reason)Notify(reason);running=false;watchClips=false;playback.store(nullptr);view.store(nullptr);}
void Cleanup(bool touchingWorld=true)noexcept{if(core.State()!=SessionState::Stopping)return;
 {std::scoped_lock lock(clipsLock);activePlaybackEpoch=0;playback.store(nullptr);}
 facialPlayback.Stop(touchingWorld);facialTest.Close(touchingWorld,"dance cleanup");
 // Release our input/render gates BEFORE optional engine restoration.
 wheelQueue.Disable();inputGate=false;running=false;watchClips=false;playback.store(nullptr);view.store(nullptr);controlsGated=false;graphPlayable=false;pauseReasons=PauseNoSession;loggedPauseReasons=~0u;progressWatch.Reset();
 auto attempt=[](auto&&action){if(!BestEffort(action))BestEffort([]{SKSE::log::error("A cleanup step failed; continuing independent recovery steps");});};
 attempt([]{picker.Cancel();});pendingPick.reset();recovery.Reset();recoveringGraph=false;nextPriorityLog=0;
 attempt([]{if(!backgroundMute.Restore())SKSE::log::error("Background volume restoration pending");if(auto*manager=RE::BSAudioManager::GetSingleton())manager->ComposeMessage(RE::SOUND_MSG::VolumeChanged);});
 attempt([]{audio.Close();});if(selector)selector->value=0;
 if(participantsBound)attempt([&]{RestoreParticipants(touchingWorld,false);});participantsBound=false;participantsWaiting=false;participantWaitSince=-1;attachmentPending.reset();beginPending=false;restageStartRequested=false;actorWatches.clear();
 attempt([]{if(facialMetrics){Facial::BackendMetrics counts;for(auto&f:actorFaces)if(f){auto c=f->Metrics();counts.fullReads+=c.fullReads;counts.mfgWrites+=c.mfgWrites;counts.mfeeWrites+=c.mfeeWrites;}SKSE::log::info("Facial metrics: actors={} evaluations={} applications={} evalUs={} applyUs={} GetAllMorph={} MFGWrites={} MFEEWrites={} (API cost measured in-game; no per-frame logs)",facialMaxActors,facialEvaluation.Evaluations(),facialApplications,facialEvaluation.Nanoseconds()/1000.,facialApplyNs/1000.,counts.fullReads,counts.mfgWrites,counts.mfeeWrites);}});
 facialEvaluation.Reset();
 // Retain the actor and its scene nodes before returning the camera. A failed
 // return must not fall through to same-frame Disable/SetDelete.
 attempt([&]{if(touchingWorld)if(auto a=cameraActor.get()){
  if(retiringCamera){SKSE::log::error("Camera retirement already pending; helper retained without deletion");return;}
  auto target=savedTarget;if(!target.get())if(auto*p=RE::PlayerCharacter::GetSingleton())target=p->GetHandle();savedTarget=target;
  retiringCamera.emplace(std::move(a),target,savedState&&savedState->id==RE::CameraState::kFirstPerson);
  SKSE::log::info("Camera helper retirement queued; waiting for confirmed camera return after engine updates");
 }});
 attempt([&]{auto*pc=RE::PlayerCamera::GetSingleton();if(touchingWorld&&pc&&cameraOwned){if(PhotoMode::Active()){if(recordedCamera.load()){deferredFov=DeferredFov{savedFov,ownedFov};deferredInteractive=DeferredInteractive{savedTarget,savedState&&savedState->id==RE::CameraState::kFirstPerson};}else deferredInteractive=DeferredInteractive{savedTarget,savedState&&savedState->id==RE::CameraState::kFirstPerson};if(pc->cameraTarget==cameraActor)pc->cameraTarget=savedTarget;return;}if(!recordedCamera.load()){ExitInteractiveCamera(*pc,savedTarget,savedState&&savedState->id==RE::CameraState::kFirstPerson);return;}auto free=pc->GetRuntimeData().cameraStates[RE::CameraState::kFree];if(pc->currentState==free&&pc->cameraTarget==cameraActor){pc->cameraTarget=savedTarget;if(savedState&&savedState->id==RE::CameraState::kFirstPerson)pc->ForceFirstPerson();else if(savedState&&savedState->id==RE::CameraState::kThirdPerson)pc->ForceThirdPerson();else if(savedState)pc->SetState(savedState.get());else pc->ForceThirdPerson();}else if(pc->cameraTarget==cameraActor)pc->cameraTarget=savedTarget;if(auto value=DeferredFov{savedFov,ownedFov}.Value(pc->GetRuntimeData2().worldFOV))pc->GetRuntimeData2().worldFOV=*value;}});
 attempt([&]{if(touchingWorld)if(auto*p=RE::PlayerCharacter::GetSingleton();p&&p->Get3D(false)&&oar)oar->ClearConditionStateData(p);});
 attempt([]{interop::Release();});
 sessionScale.Reset();wheelFov.Reset();hiddenRoots={};playerHeightScale.Reset();nextMeshHide=0;cameraOwned=false;cameraCalibrated=false;cameraAnchor.reset();cameraActor.reset();savedState.reset();savedTarget.reset();playerGraph.reset();graphManager.reset();stageCell=nullptr;prepared.reset();core.Cleaned();announced=false;wasPaused=false;userPaused=false;cameraReturnPending=false;photoRequestUntil=0;resumeGraceUntil=0;
 BestEffort([&]{SKSE::log::info("Session cleanup: touchingWorld={} NPCPending={} NPCFailures={} NPCAbandoned={} cameraPending={} audioPending={} audioReset={} audioUnprepare={} audioClose={}",touchingWorld,performers.Pending(),performers.releaseFailures,performers.abandoned,retiringCamera.has_value(),audio.Faulted(),audio.lastReset,audio.lastUnprepare,audio.lastClose);});
}
void PumpCameraRetirement(){
 if(!retiringCamera)return;
 auto&r=*retiringCamera;double now=Now();if(!r.began)r.began=now;if(now-r.lastDiagnostic>=10){r.lastDiagnostic=now;SKSE::log::warn("Camera retirement pending {} seconds; failed={} (helper retained, never forced deleted)",now-r.began,r.failed);}
 if(r.failed)return;auto*pc=RE::PlayerCamera::GetSingleton();auto*ui=RE::UI::GetSingleton();
 const bool observable=world.load()&&SafeWorld()&&pc&&ui&&!ui->GameIsPaused()&&!PhotoMode::Active()&&!PhotoMode::Frozen()&&!photoActive.load()&&photoRequestUntil==0;
 // Check target AND normal camera state after the engine update, not merely
 // the handle we assigned during cleanup. Do not take control from other mods.
 if(observable&&!r.target.get())if(auto*player=RE::PlayerCharacter::GetSingleton();player&&pc->cameraTarget==player->GetHandle())r.target=player->GetHandle();
 const bool returned=observable&&r.target.get()&&pc->cameraTarget==r.target&&(pc->currentState==pc->GetRuntimeData().cameraStates[RE::CameraState::kFirstPerson]||pc->currentState==pc->GetRuntimeData().cameraStates[RE::CameraState::kThirdPerson]);
 auto action=r.gate.Observe(returned,completedEngineUpdates,Now());
 if(action==CameraRetirementGate::Action::None)return;
 try{
  if(action==CameraRetirementGate::Action::Disable){r.actor->Disable();r.gate.Completed();SKSE::log::info("Camera return confirmed across engine updates; helper disabled, retaining actor and scene references");}
  else if(action==CameraRetirementGate::Action::Delete){r.actor->SetDelete(true);r.gate.Completed();SKSE::log::info("Camera return remains confirmed after helper disable; deletion requested, retaining references through further updates");}
  else {retiringCamera.reset();SKSE::log::info("Camera helper retirement completed after confirmed return and deferred deletion");}
 }catch(const std::exception&e){SKSE::log::error("Camera retirement C++ exception: {}",e.what());if(retiringCamera)retiringCamera->failed=true;SKSE::log::error("Camera helper retirement failed; retained without further engine cleanup attempts until world reset");BestEffort([]{Notify(L10n::T("qCameraRetirementFailed"));});}
}
// SetCollision only sets a reference flag in CommonLib. Harden the actual
// controller as soon as it exists, including asynchronously replaced controllers.
void ProtectCameraActor(RE::Actor* actor){
 // Only this session's helper: never cull the player's or another actor's model.
 // Alpha can be reset while a camera target's biped is refreshed. Hide the
 // scene root and meshes independently, including late/replaced 3D roots.
 bool recull=Now()>=nextMeshHide;for(unsigned i=0;i<2;++i){auto*root=actor->Get3D(i!=0);if(root){bool replaced=hiddenRoots[i].get()!=root;root->SetAppCulled(true);if(replaced||recull){root->CullGeometry(true);root->SetCollisionLayer(RE::COL_LAYER::kNonCollidable);}hiddenRoots[i].reset(root);}else hiddenRoots[i].reset();}if(recull)nextMeshHide=Now()+.5;
 actor->SetCollision(false);if(auto*cc=actor->GetCharController()){cc->flags.set(RE::CHARACTER_FLAGS::kNoSim,RE::CHARACTER_FLAGS::kNoCharacterCollisions,RE::CHARACTER_FLAGS::kNotPushablePermanent);cc->gravity=0;}}
void Publish(double seconds){if(!prepared)return;if(prepared->hasDance){auto set=std::make_shared<PlaybackSet>();set->reserve(actorWatches.size());for(auto&w:actorWatches)set->push_back(Playback{w,core.Generation(),&w->graph->characterInstance,prepared->song.submod,prepared->song.duration,seconds,sessionNative,core.State()==SessionState::Playing&&pauseReasons.load()==0});watchClips=true;playback.store(std::move(set));}else{watchClips=false;playback.store(nullptr);}if(!recordedCamera.load()||photoActive.load()){view.store(nullptr);return;}if(!cameraCalibrated)return;auto sample=prepared->camera.At(seconds);auto*activeCamera=RE::PlayerCamera::GetSingleton();if(activeCamera&&activeCamera->currentState==activeCamera->GetRuntimeData().cameraStates[RE::CameraState::kFree]&&activeCamera->cameraTarget==cameraActor){ownedFov=static_cast<float>(wheelFov.Value(sample.fov));activeCamera->GetRuntimeData2().worldFOV=ownedFov;}
 auto actor=cameraActor.get();auto subjectForView=performers.CameraActor();if(!actor||!subjectForView)return;
 // The helper never overlaps the dancer; its physics/position is not a camera origin.
 auto*player=RE::PlayerCharacter::GetSingleton();double scale=sessionScale.Ratio(prepared->camera.referenceScale);Vec pos=FrameCameraPosition(stage,stageRotation,prepared->camera.RelativePosition(sample),prepared->camera.movementPivot,scale,prepared->camera.heightOffset,cameraFraming.Current());Quat direction=Unit(stageRotation*prepared->camera.Direction(sample));Quat rot=Unit(direction*calibration);auto*pc=RE::PlayerCamera::GetSingleton();auto*state=pc->GetRuntimeData().cameraStates[RE::CameraState::kFree].get();view.store(std::make_shared<View>(View{subjectForView->GetHandle(),state,pos,rot,stage,stageRotation}));if(cameraAnchor){auto actorRotation=Yaw(-actor->GetAngleZ());cameraAnchor->local.translate=Point(CameraAnchorPosition(pos,Vector(actor->GetPosition()),actorRotation));Quaternion(Unit(actorRotation.Inv()*direction)).ToRotation(cameraAnchor->local.rotate);}}

void AttachPrepared(Prepared&& p){ValidateParticipantsForStart();if(!SafePlayer())throw std::runtime_error(L10n::T("r0093"));auto*player=RE::PlayerCharacter::GetSingleton();auto*pc=RE::PlayerCamera::GetSingleton();auto*controls=RE::PlayerControls::GetSingleton();if(!pc||!controls||controls->blockPlayerInput)throw std::runtime_error(L10n::T("r0094"));if(!pc->IsInFirstPerson()&&!pc->IsInThirdPerson())throw std::runtime_error(L10n::T("r0095"));if(auto target=pc->cameraTarget.get();target&&target.get()!=player)throw std::runtime_error(L10n::T("r0096"));float speed=0;if(!player->GetGraphVariableFloat("Speed",speed))throw std::runtime_error(L10n::T("r0097"));if(speed>1||player->AsActorState()->IsWeaponDrawn())throw std::runtime_error(L10n::T("r0098"));
 if(!InstallRuntimeHooks()||!oar)throw std::runtime_error(L10n::T("r0099"));
 const bool withCamera=recordedCamera.load();if(retiringCamera&&withCamera)throw std::runtime_error(L10n::T("qCameraReturnPending"));if(!interop::Acquire(true)||!interop::CameraConfirmed())throw std::runtime_error(L10n::T("r0100"));
 graphPlayable=false;pauseReasons=PauseNoSession|PauseGraph;facialTest.Close(true,"performance preparation completed");facialPlayback.Stop();prepared=std::move(p);for(auto&f:actorFaces)if(f)f->ResetMetrics();facialEvaluation.Bind(prepared->facial,facialMetrics);facialApplications=facialApplyNs=0;facialMaxActors=0;facialEvaluationWarned=false;audio.Open(std::move(prepared->wave));core.Ready(core.Generation(),ReadyPart::AudioBuffered,true,PerformanceActors::ControlTime());core.Ready(core.Generation(),ReadyPart::CameraTrack,true,PerformanceActors::ControlTime());
 activePlaybackEpoch=core.Generation();BindParticipants();

 auto subject=performers.CameraActor();if(!subject)throw std::runtime_error(L10n::T("aLost"));stage=Vector(subject->GetPosition());stageRotation=Yaw(-subject->GetAngleZ());originalPosition=player->GetPosition();stageCell=player->GetParentCell();stableStage.Reset(stage,Now());sessionScale.Capture(playerHeightScale.Read(subject.get(),Now()));
 SKSE::log::info("Camera state snapshot after ownership confirmation; recorded={}",withCamera);savedState=pc->currentState;savedTarget=pc->cameraTarget;savedFov=pc->GetRuntimeData2().worldFOV;
 if(withCamera){
 // Same engine API used by CommonLib's PlaceObjectAtMe, but spawn away from
 // the player from the very first instruction (no overlapping placement/move).
 auto parked=Point(CameraParking(stage));auto spawnHandle=RE::TESDataHandler::GetSingleton()->CreateReferenceAtLocation(cameraBase,parked,player->GetAngle(),stageCell,player->GetWorldspace(),nullptr,nullptr,RE::ObjectRefHandle(),false,true);
 auto spawned=spawnHandle.get();auto*actor=spawned?spawned->As<RE::Actor>():nullptr;if(!actor){if(spawned){spawned->Disable();spawned->SetDelete(true);}throw std::runtime_error(L10n::T("r0104"));}cameraActor=actor->GetHandle();if(!cameraActor.get()){actor->Disable();actor->SetDelete(true);throw std::runtime_error(L10n::T("r0105"));}
 actor->EnableAI(false);actor->SetAlpha(0);ProtectCameraActor(actor);auto*ghost=actor->extraList.GetByType<RE::ExtraGhost>();if(!ghost){ghost=RE::BSExtraData::Create<RE::ExtraGhost>();if(ghost)actor->extraList.Add(ghost);}if(ghost)ghost->ghost=true;
 SKSE::log::info("Camera helper parked away from dancer: stage=({},{},{}), helper=({},{},{}), controllerReady={}",stage.x,stage.y,stage.z,parked.x,parked.y,parked.z,actor->GetCharController()!=nullptr);

 ownedFov=static_cast<float>(wheelFov.Value(prepared->camera.At(0).fov));
 pc->ForceThirdPerson();pc->cameraTarget=cameraActor;cameraOwned=true;pc->SetState(pc->GetRuntimeData().cameraStates[RE::CameraState::kFree].get());pc->GetRuntimeData2().worldFOV=ownedFov;
 }else{if(!EnterInteractiveCamera(*pc))throw std::runtime_error(L10n::T("r0168"));cameraOwned=true;cameraCalibrated=true;core.Ready(core.Generation(),ReadyPart::CameraActor3D,true,PerformanceActors::ControlTime());SKSE::log::info("Interactive free camera entered: freeze=false, no player-relative camera path");}
 inputGate=true;controlsGated=true;controls->data.moveInputVec={0,0};controls->data.lookInputVec={0,0};controls->data.autoMove=false;
 selector->value=prepared->hasDance?static_cast<float>(prepared->song.token):0;if(prepared->hasDance)for(auto&w:actorWatches)if(auto a=w->actor.get()){oar->ClearConditionStateData(a.get());a->NotifyAnimationGraph("IdleForceDefaultState");}
 generated=0;observedEpoch=0;observation.store(nullptr);observationAt=0;probedLoadPath.clear();preparedAt=Now();nextDiagnostic=preparedAt+5;lastGraphSeen=Now();calibrationTicks=0;if(withCamera)cameraCalibrated=false;retryIdle=false;nextCollisionLog=nextHelperRepark=nextParkWarning=0;maxPlayerStepSquared=0;protectionResets=0;previousPlayerPosition=stage;Publish(0);Notify(L10n::T("r0106"));
}

void BeginPrepare(){facialTest.Close(true,"dance preparation");for(auto&f:actorFaces)if(f&&f->Releasing()){Notify(L10n::T("aFacialPending"));return;}if(!SafePlayer()){if(beginPending){beginPending=false;restageStartRequested=false;performers.CancelRestage(participantFaction);}Notify(L10n::T("r0107"));return;}if(RE::UI::GetSingleton()->GameIsPaused()||loadFuture.valid())return;
 auto restage=performers.RestageSaved(participantFaction);
 if(restage==PerformanceActors::RestageResult::Pending){if(!beginPending){beginPending=true;beginDeadline=PerformanceActors::ControlTime()+30;Notify(L10n::T("aRestaging"));}return;}
 if(restage==PerformanceActors::RestageResult::Failed){beginPending=false;restageStartRequested=false;std::string reason=L10n::T("aRestageFailed");auto i=performers.lastRestageSlot;if(i<MaxPerformers){auto&s=performers.slots[i];auto a=s.actor.get();const char*detail=performers.lastForceIssue==PerformanceActors::ForceIssue::RestorePending?"aRestoreWait":!PerformanceActors::Registrable(a.get())?"aActorMissing":!s.savedPlacement?"aRepositionHint":!PerformanceActors::SameSpace(RE::PlayerCharacter::GetSingleton()->GetParentCell(),PerformanceActors::Cell(s.savedPlacement->cell))?"aCellHint":"aActorControl";reason+=" · "+std::to_string(i+1)+": "+L10n::T(detail);}Notify(reason);PublishActors();return;}
 // Restaging restores positions only; it never requests playback.
 auto*checkPlayer=RE::PlayerCharacter::GetSingleton();auto initialIssue=performers.ValidateStart(checkPlayer?checkPlayer->GetParentCell():nullptr,!cfg.combatStop);if(initialIssue==PerformanceActors::StartIssue::Loading||initialIssue==PerformanceActors::StartIssue::Recovering){if(!beginPending){beginPending=true;beginDeadline=PerformanceActors::ControlTime()+10;Notify(L10n::T("aActorLoading"));}return;}try{ValidateParticipantsForStart();}catch(const std::exception&e){beginPending=false;restageStartRequested=false;RestoreParticipants(world.load(),false);Notify(e.what());return;}bool automaticStart=restageStartRequested;beginPending=false;participantsBound=true;auto selection=ResolveContent(songs,selectedDanceId,selectedCameraId);if(!selection){restageStartRequested=false;RestoreParticipants(world.load(),false);Notify(L10n::T("r0108"));return;}auto dance=selection->dance,camera=selection->camera;auto epoch=core.Prepare(PerformanceActors::ControlTime(),45);if(!epoch){restageStartRequested=false;RestoreParticipants(world.load(),false);return;}if(automaticStart)core.RequestStart();restageStartRequested=false;loadEpoch=*epoch;wheelQueue.Disable();wheelFov.Reset();recordedCamera=camera.has_value();sessionNative=developerTools&&hotkeys.NativePlayback();nextClipTrace=0;Song master=dance?*dance:*camera;SKSE::log::info("Selection: dance={} camera={} audioMaster={} duration={}",selectedDanceId,selectedCameraId,master.id,master.duration);loadFuture=std::async(std::launch::async,[master=std::move(master),dance=std::move(dance),camera=std::move(camera),volume=cfg.volume]()mutable{return Load(std::move(master),std::move(dance),std::move(camera),volume);});Notify(L10n::T("r0109"));}
void PublishSongList(std::string error={}){auto data=std::make_shared<PickerList>();data->generation=catalogGeneration;data->loading=catalogPending;data->error=std::move(error);data->summary=catalogSummary;FillPickerTracks(*data,songs);picker.Publish(std::move(data));}

void Select(){if(core.State()!=SessionState::Idle){Notify(L10n::T("r0110"));return;}if(catalogPending||catalogFuture.valid())return;
 const auto generation=++catalogGeneration;
 auto loading=std::make_shared<PickerList>();loading->generation=generation;loading->loading=true;picker.Publish(std::move(loading));
 try{auto task=std::async(std::launch::async,[generation]{try{auto result=Catalog();auto data=std::make_shared<PickerList>();data->generation=generation;FillPickerTracks(*data,result.songs);data->summary=result.summary;picker.Publish(std::move(data));return result;}catch(const std::exception&e){auto data=std::make_shared<PickerList>();data->generation=generation;data->error=e.what();picker.Publish(std::move(data));throw;}});catalogFuture=std::move(task);catalogEpoch=generation;catalogPending=true;}
 catch(const std::exception&e){catalogPending=false;PublishSongList(std::string(L10n::T("r0111"))+e.what());}
}
void OpenPicker(){pendingPick.reset();if(songs.empty()&&!catalogPending)Select();if(!picker.Open(selectedDanceId)){Notify(L10n::T("r0112"));}}

std::string HotkeysKeyLabel(unsigned code){wchar_t name[128]{};GetKeyNameTextW(static_cast<LONG>((code&127)<<16),name,128);char out[512]{};WideCharToMultiByte(CP_UTF8,0,name,-1,out,512,nullptr,nullptr);return out;}
RE::FormID rosterCell{};
void PublishActors(){
 auto*p=RE::PlayerCharacter::GetSingleton();rosterCell=p&&p->GetParentCell()?p->GetParentCell()->GetFormID():0;
 auto list=std::make_shared<ActorList>();list->hasPlayer=performers.HasPlayer();list->revision=rosterRevision;list->camera=performers.camera.slot;
 for(unsigned i=0;i<MaxPerformers;++i){auto&s=performers.slots[i];auto a=s.actor.get();list->assigned[i]=bool(s.actor);list->labels[i]=std::to_string(i+1)+". "+(a?std::string(a->GetName()):L10n::T(s.actor?"aActorMissing":"aEmpty"));if(a){const char*key="aUnplaced";
 if(performers.Restoring(a.get()))key="aRestorePending";else if(s.moving)key="aMoving";else if(s.placement){auto state=performers.StateOf(i,p?p->GetParentCell():nullptr,false);key=state==PerformanceActors::ParticipantState::RecoveringControl?"aControlRecovering":state==PerformanceActors::ParticipantState::Loading?"aActorLoading":state==PerformanceActors::ParticipantState::InvalidActor?"aActorMissing":s.placementValid?(core.State()==SessionState::Preparing||core.State()==SessionState::Armed?"aPreparing":"aPlaced"):"aReposition";}else if(a.get()==p)key="aAuto";list->labels[i]+=" — "+std::string(L10n::T(key));}}

 actorManager.Publish(std::move(list));
}
void EndActorMode(bool reopen){if(actorMode==2&&actorModeSlot<MaxPerformers&&performers.slots[actorModeSlot].moving)performers.Release(actorModeSlot,world.load(),participantFaction);actorMode=0;actorModeCell=0;actorWorldGate={};actorWorldMonitor={};commands.fetch_and(~(256u|512u));actorManager.Close();if(reopen){PublishActors();actorManager.Open();}}
void RestoreParticipants(bool valid,bool clear){
 performanceHeads.Stop(valid);
 for(auto&f:actorFaces)if(f)f->Stop(valid);
 
 auto watches=actorWatches;performers.Reset(valid,participantFaction,clear);
 if(valid)for(auto&w:watches)BestEffort([&]{if(auto a=w->actor.get();a&&a->Get3D(false)){if(oar)oar->ClearConditionStateData(a.get());a->NotifyAnimationGraph("IdleForceDefaultState");}});
 rosterRevision++;PublishActors();
}
void ProcessActorActions(unsigned bits){
 if(picker.TakeActors()){if(core.State()!=SessionState::Idle){Notify(L10n::T("aBusy"));return;}PublishActors();actorManager.Open();}
 if(auto request=actorManager.Take()){
  if(core.State()!=SessionState::Idle||request->revision!=rosterRevision||request->slot>=MaxPerformers){Notify(L10n::T("aBusy"));PublishActors();return;}
  auto slot=request->slot;if(beginPending&&request->action!=ActorAction::Back&&request->action!=ActorAction::Camera){beginPending=false;restageStartRequested=false;performers.CancelRestage(participantFaction);}
  if(request->action==ActorAction::Back){actorManager.Close();OpenPicker();return;}
  if(request->action==ActorAction::Remove){performers.Release(slot,true,participantFaction);performers.slots[slot]={};if(performers.FixCamera())Notify(L10n::T("aAnchorChanged"));rosterRevision++;PublishActors();}
  else if(request->action==ActorAction::Player){if(!performers.AssignPlayer(slot))Notify(L10n::T("aPlayerRejected"));rosterRevision++;PublishActors();}
  else if(request->action==ActorAction::Camera){if(performers.slots[slot].actor.get()){performers.camera.slot=slot;rosterRevision++;PublishActors();}}
  else if(request->action==ActorAction::Assign||request->action==ActorAction::Place){
   if(!SafeWorld()||PhotoMode::Active()||!hotkeys.Current().start){Notify(L10n::T("aRejected"));return;}picker.Cancel();actorManager.Close();settings.CloseMainForWorld();actorWorldGate.Begin(Now());actorWorldMonitor.Begin(PerformanceActors::ControlTime());bits&=~(256u|512u);commands.fetch_and(~(256u|512u));actorModeSlot=slot;actorMode=request->action==ActorAction::Assign?1:2;actorModeCell=RE::PlayerCharacter::GetSingleton()->GetParentCell()->GetFormID();nextActorHint=0;
  }
 }
 performers.Separation();
 // A registered actor is not necessarily placed; retained coordinates are not a live lease.
 if(core.State()==SessionState::Idle){auto*p=RE::PlayerCharacter::GetSingleton();bool changed=performers.RefreshPlacements(world.load(),participantFaction);changed|=(p&&p->GetParentCell()?p->GetParentCell()->GetFormID():0)!=rosterCell;for(unsigned i=0;i<MaxPerformers;++i)if(PerformanceActors::Excluded(performers.slots[i].actor.get().get())){performers.Release(i,world.load(),participantFaction);performers.slots[i]={};performers.FixCamera();changed=true;}if(changed){rosterRevision++;PublishActors();}}
 if(!actorMode.load())return;
 auto*player=RE::PlayerCharacter::GetSingleton();auto*ui=RE::UI::GetSingleton();auto selectedActor=performers.slots[actorModeSlot].actor.get();
 const bool sameCell=player&&player->GetParentCell()&&PerformanceActors::SameSpace(player->GetParentCell(),PerformanceActors::Cell(actorModeCell));
 const bool canReopen=SafeWorld()&&sameCell&&player&&!player->IsDead()&&ui&&!ui->GameIsPaused()&&!settings.Open()&&!PhotoMode::Active()&&!ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)&&!ui->IsMenuOpen(RE::Console::MENU_NAME);
 if(bits&512){beginPending=false;EndActorMode(canReopen);Notify(L10n::T("aCancelled"));return;}
 const bool placing=actorMode.load()==2;
 const bool terminal=!world.load()||(player&&player->IsDead())||(player&&player->GetParentCell()&&!sameCell)||(placing&&!selectedActor)||(placing&&selectedActor&&(selectedActor->IsDead()||selectedActor->IsDisabled()||selectedActor->IsDeleted()));
 const bool transferPending=placing&&performers.slots[actorModeSlot].moving;
 const bool available=player&&player->GetParentCell()&&player->Get3D(false)&&ui&&(!placing||transferPending||(selectedActor&&selectedActor->GetParentCell()));
 const bool visiblePause=ui&&std::any_of(ui->menuStack.begin(),ui->menuStack.end(),[](const auto&m){return m&&m->PausesGame();});
 const bool foreignMenu=ui&&(settings.Open()||visiblePause||PhotoMode::Active()||ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)||ui->IsMenuOpen(RE::Console::MENU_NAME));
 const bool orphanPause=ui&&ui->GameIsPaused()&&!foreignMenu;
 const bool usable=(!placing||transferPending||PerformanceActors::Registrable(selectedActor.get()))&&!orphanPause;
 if(observedMenuEpoch!=menuEpoch.load()){observedMenuEpoch=menuEpoch.load();actorWorldMonitor.UITransition();}
  auto decision=actorWorldMonitor.Update(PerformanceActors::ControlTime(),terminal,available,foreignMenu,usable);
 if(decision==WorldModeDecision::Cancel){SKSE::log::info("World interaction safe exit: terminal={} available={} usable={} sameCell={} mode={}",terminal,available,usable,sameCell,actorMode.load());auto reason=!world.load()?"aCancelWorld":!sameCell?"aCancelCell":player&&player->IsDead()?"aCancelDead":placing&&selectedActor&&selectedActor->IsDead()?"aCancelTargetDead":!available?"aCancelMissing":"aCancelTimeout";EndActorMode(canReopen);Notify(L10n::T(reason));return;}
 auto handoff=actorWorldGate.Update(Now(),decision==WorldModeDecision::Suspend,false);
 if(decision!=WorldModeDecision::Continue||handoff!=WorldInteractionGate::Result::Ready)return;

 if((bits&256)||(placing&&performers.slots[actorModeSlot].moving)){bool success=false;bool refused=false;if(actorMode==1){auto*pick=RE::CrosshairPickData::GetSingleton();auto ref=pick?pick->GetActiveTarget().get():RE::NiPointer<RE::TESObjectREFR>{};auto*target=ref?ref->As<RE::Actor>():nullptr;refused=PerformanceActors::Excluded(target);if(!refused)success=performers.Assign(actorModeSlot,target,participantFaction);}else success=performers.Place(actorModeSlot,participantFaction);
  if(success){Notify(L10n::T(actorMode==1?"aAssigned":"aPlacedDone"));rosterRevision++;EndActorMode(true);}else{auto issue=performers.lastForceIssue;if(issue==PerformanceActors::ForceIssue::Moving){if(bits&256){Notify(L10n::T("aMoving"));PublishActors();}return;}if(!refused)SKSE::log::warn("Actor assignment/placement refused: slot={} forceIssue={}",actorModeSlot+1,static_cast<unsigned>(issue));Notify(L10n::T(refused?"aRefused":issue==PerformanceActors::ForceIssue::RestorePending?"aRestoreWait":issue==PerformanceActors::ForceIssue::Movement?"aPlaceMoveFailed":"aRejected"));}
 }else if(Now()>=nextActorHint){nextActorHint=Now()+3.;if(actorMode==1)Notify(L10n::T("aAim"));else{auto a=performers.slots[actorModeSlot].actor.get();auto key=HotkeysKeyLabel(hotkeys.Current().start);Notify(std::string(L10n::T("aPlaceHint"))+(a?a->GetName():"?")+" · "+key+L10n::T("aConfirm"));}}
}
void ValidateParticipantsForStart(){performers.HoldPlaced(true,PerformanceActors::ControlTime(),participantFaction,true);auto*p=RE::PlayerCharacter::GetSingleton();auto issue=performers.ValidateStart(p?p->GetParentCell():nullptr,!cfg.combatStop);if(issue!=PerformanceActors::StartIssue::None)throw std::runtime_error(L10n::T(issue==PerformanceActors::StartIssue::Loading?"aActorLoading":issue==PerformanceActors::StartIssue::Recovering?"aControlRecovering":issue==PerformanceActors::StartIssue::Control?"aActorControl":issue==PerformanceActors::StartIssue::InvalidActor?"aActorMissing":issue==PerformanceActors::StartIssue::Excluded?"aRefused":issue==PerformanceActors::StartIssue::Restoring?"aRestoreWait":issue==PerformanceActors::StartIssue::NeedsPlacement?"aRepositionHint":issue==PerformanceActors::StartIssue::WrongCell?"aCellHint":"aLost"));}
void BindParticipants(){
 ValidateParticipantsForStart();participantsBound=true;
 if(!performers.Count())throw std::runtime_error(L10n::T("aNoActors"));
 const bool routing=prepared->song.manifest.value("participantRouting",0)==1;
 if(prepared->hasDance&&!routing&&(!performers.HasPlayer()||performers.Count()!=1))throw std::runtime_error(L10n::T("aNewPackage"));
 if(prepared->hasDance&&routing&&!participantFaction)throw std::runtime_error("DanceStudioData.esl participant faction missing (update full package)");
 for(unsigned i=0;i<MaxPerformers;++i){auto&s=performers.slots[i];auto a=s.actor.get();if(!s.actor)continue;
  if(!PerformanceActors::Usable(a.get(),a.get()==RE::PlayerCharacter::GetSingleton()&&!cfg.combatStop))throw std::runtime_error(std::string(L10n::T("aGraph"))+std::to_string(i+1));
  if(a.get()!=RE::PlayerCharacter::GetSingleton()){
   if(!s.placement||!s.placementValid||performers.Restoring(a.get()))throw std::runtime_error(std::string(L10n::T("aNeedPlace"))+a->GetName());
   if(!PerformanceActors::SamePlace(a.get(),*s.placement,256)||!PerformanceActors::Hold(s,a.get()))throw std::runtime_error(L10n::T("aLost"));
  }
  if(!PerformanceActors::SameSpace(a->GetParentCell(),RE::PlayerCharacter::GetSingleton()->GetParentCell()))throw std::runtime_error(L10n::T("aLost"));
  s.danceId=selectedDanceId;s.danceToken=prepared->song.token;
  if(!actorFaces[i])actorFaces[i]=std::make_unique<Facial::Playback>();actorFaces[i]->SetActor(s.actor);
  if(!prepared->hasDance)continue;
  if(!a->GetActorBase())throw std::runtime_error(L10n::T("aLost"));
  if(a->GetActorBase()->GetSex()==RE::SEX::kMale&&!prepared->song.manifest.contains("danceMale"))throw std::runtime_error(std::string(L10n::T("aSex"))+a->GetName());
  if(routing&&!a->IsInFaction(participantFaction)){a->AddToFaction(participantFaction,0);s.membership=true;}
  auto watch=std::make_shared<ActorWatch>();watch->actor=s.actor;watch->slot=i;
  if(!a->GetAnimationGraphManager(watch->manager)||!watch->manager)throw std::runtime_error(std::string(L10n::T("aGraph"))+a->GetName());
  {RE::BSSpinLockGuard lock(watch->manager->GetRuntimeData().updateLock);for(auto&g:watch->manager->graphs)if(g&&g->holder==a.get()&&(std::string_view(g->projectName.c_str())=="DefaultFemale"||std::string_view(g->projectName.c_str())=="DefaultMale")){watch->graph=g;break;}}
  if(!watch->graph)throw std::runtime_error(std::string(L10n::T("aGraph"))+a->GetName());
  actorWatches.push_back(std::move(watch));
  if(a.get()==RE::PlayerCharacter::GetSingleton())playerGraph=actorWatches.back()->graph;
  
  SKSE::log::info("Participant graph slot={} ref={:08X} project={} token={}",i+1,a->GetFormID(),actorWatches.back()->graph->projectName.c_str(),s.danceToken);
 }
}
bool ReconnectWatch(std::shared_ptr<ActorWatch>&watch,RE::Actor*a,RE::BSTSmartPointer<RE::BSAnimationGraphManager>manager){
 RE::BSTSmartPointer<RE::BShkbAnimationGraph>graph;{RE::BSSpinLockGuard lock(manager->GetRuntimeData().updateLock);for(auto&g:manager->graphs)if(g&&g->holder==a&&(std::string_view(g->projectName.c_str())=="DefaultFemale"||std::string_view(g->projectName.c_str())=="DefaultMale")){graph=g;break;}}
 if(!graph)return false;if(manager.get()==watch->manager.get()&&graph.get()==watch->graph.get())return true;
 auto fresh=std::make_shared<ActorWatch>();fresh->actor=watch->actor;fresh->slot=watch->slot;fresh->manager=manager;fresh->graph=graph;fresh->reconnectPending=true;watch=std::move(fresh);if(a==RE::PlayerCharacter::GetSingleton())playerGraph=graph;
 graphPlayable=false;if(running.load())ApplyPlaybackPause(false);
 oar->ClearConditionStateData(a);a->NotifyAnimationGraph("IdleForceDefaultState");
 Publish(core.State()==SessionState::Playing?core.Seconds():0);SKSE::log::info("Participant graph rebound at music time={} slot={}",core.Seconds(),watch->slot+1);return true;
}
bool ParticipantsValid(){participantIssue="aActorMissing";bool waiting=false,recovering=false;participantsWaiting=false;auto*player=RE::PlayerCharacter::GetSingleton();if(!player||!stageCell)return false;
 for(unsigned i=0;i<MaxPerformers;++i)if(performers.slots[i].actor){auto state=performers.StateOf(i,stageCell,core.State()!=SessionState::Playing);
  switch(state){case PerformanceActors::ParticipantState::InvalidActor:return false;case PerformanceActors::ParticipantState::Fatal:participantIssue="aCellHint";return false;case PerformanceActors::ParticipantState::NeedsPlacement:participantIssue="aRepositionHint";return false;case PerformanceActors::ParticipantState::Loading:waiting=true;break;case PerformanceActors::ParticipantState::RecoveringControl:recovering=waiting=true;break;default:break;}
 }
 for(auto&w:actorWatches){auto a=w->actor.get();if(!a)return false;RE::BSTSmartPointer<RE::BSAnimationGraphManager>manager;if(!a->Get3D(false)||!a->GetAnimationGraphManager(manager)||!manager||!ReconnectWatch(w,a.get(),manager)){waiting=true;continue;}if(w->reconnectPending){if(w->generated.load()<3)waiting=true;else w->reconnectPending=false;}}
 if(waiting){participantIssue=recovering?"aActorControl":"aActorLoadTimeout";if(participantWaitSince<0)participantWaitSince=PerformanceActors::ControlTime();participantsWaiting=true;return PerformanceActors::ControlTime()-participantWaitSince<10;}
 participantWaitSince=-1;return performers.Count()>0;
}
void StartParticipants(){
 ValidateParticipantsForStart();
 if(!performers.MovePlayerToStart(stageCell))throw std::runtime_error(L10n::T("aMoveFailed"));
 auto anchor=performers.CameraActor();if(!anchor)throw std::runtime_error(L10n::T("aLost"));stage=Vector(anchor->GetPosition());stageRotation=Yaw(-anchor->GetAngleZ());stableStage.Reset(stage,Now());performers.stage={stage,stageRotation};
}

void ProcessCommands(){unsigned bits=commands.exchange(0);ProcessActorActions(bits);if(actorMode.load())return;if(bits&32)hud.Toggle();if(bits&4){restageStartRequested=false;beginPending=false;attachmentPending.reset();participantsBound=true;actorManager.Close();picker.Cancel();pendingPick.reset();if(core.State()==SessionState::Idle){RestoreParticipants(world.load(),false);return;}Stop(L10n::T("r0113"));return;}
 if((bits&64)&&core.State()==SessionState::Playing){ApplyPlaybackPause(false);SKSE::log::info("Dance user pause: {}",userPaused.load());}
 if((bits&128)&&SafeWorld()&&!settings.Open()&&!RE::UI::GetSingleton()->GameIsPaused()&&!PhotoMode::Active()&&Now()>=photoRequestUntil){if(PhotoMode::Open())photoRequestUntil=Now()+3.;else Notify(L10n::T("r0114"));}
 if(picker.TakeRefresh()&&core.State()==SessionState::Idle)Select();
 if(auto choice=picker.Take()){if(retiringCamera){if(retiringCamera->failed){retiringCamera->failed=false;SKSE::log::info("User-requested safe camera retirement retry");}Notify(L10n::T("qCameraReturnPending"));}pendingPick=std::move(choice);}
 if(pendingPick&&(!retiringCamera||pendingPick->cameraId.empty())&&!catalogPending&&!catalogFuture.valid()&&!loadFuture.valid()&&core.State()==SessionState::Idle&&!settings.Open()&&!RE::UI::GetSingleton()->GameIsPaused()){auto choice=std::move(pendingPick);if(choice->generation!=catalogGeneration){Notify(L10n::T("r0115"));OpenPicker();return;}auto valid=[&](const std::string&id,bool dance){return id.empty()||std::any_of(songs.begin(),songs.end(),[&](const Song&s){return s.id==id&&(dance?s.HasDance():s.HasCamera());});};if(!valid(choice->id,true)||!valid(choice->cameraId,false)){Notify(L10n::T("r0116"));OpenPicker();return;}selectedDanceId=choice->id;selectedCameraId=choice->cameraId;BeginPrepare();return;}
 if(settings.Open())return;
 if(bits&(8|16))core.RequestStart();
 if((bits&2)&&core.State()==SessionState::Idle){if(beginPending)restageStartRequested=true;else OpenPicker();}
 if(core.PendingStart()){if(core.State()==SessionState::Armed&&SafeWorld()&&!RE::PlayerCharacter::GetSingleton()->IsDead()&&!RE::UI::GetSingleton()->GameIsPaused()&&!menuSuspended.load()){if(!ParticipantsValid())throw std::runtime_error(L10n::T(participantIssue));if(participantsWaiting)return;StartParticipants();if(!core.Start())return;backgroundMute.Begin(backgroundCategories);if(auto*manager=RE::BSAudioManager::GetSingleton())manager->ComposeMessage(RE::SOUND_MSG::VolumeChanged);running=true;performanceHeads.Update(performers.slots);ApplyPlaybackPause(false);Notify(L10n::T("r0117")+prepared->song.title);}}

}

void ProcessWheelQueue();
void Tick(){auto*clockUI=RE::UI::GetSingleton();PerformanceActors::AdvanceControlTime(RE::GetSecondsSinceLastFrame(),!clockUI||clockUI->GameIsPaused()||PhotoMode::Frozen());if(SafeWorld())PerformanceAlias::Tick(PerformanceActors::ControlTime());
 if(catalogPending&&!catalogFuture.valid()){catalogPending=false;PublishSongList(L10n::T("r0118"));}
 if(catalogPending&&catalogFuture.wait_for(std::chrono::seconds(0))==std::future_status::ready){catalogPending=false;try{auto result=catalogFuture.get();if(catalogEpoch!=catalogGeneration){songs.clear();Select();return;}catalogSummary=result.summary;songs=std::move(result.songs);PublishSongList();if(songs.empty())Notify(L10n::T("r0119"));else Notify(L10n::T("r0120")+std::to_string(songs.size())+L10n::T("r0149"));}catch(const std::exception&e){songs.clear();if(catalogEpoch!=catalogGeneration){Select();return;}PublishSongList(std::string(L10n::T("r0121"))+e.what());Notify(std::string(L10n::T("r0150"))+e.what());}}

 if(loadFuture.valid()&&loadFuture.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{auto p=loadFuture.get();if(core.State()==SessionState::Preparing&&core.Generation()==loadEpoch){if(!p.facialIssue.empty()){std::string key=p.song.id+"|"+p.song.manifest.value("exportId",std::string{})+"|"+p.facialIssue;if(!facialWarnings.contains(key)){if(facialWarnings.size()>=256)facialWarnings.clear();facialWarnings[key]=true;Notify(std::string(L10n::T("qFacialWarning"))+L10n::T(("qFacial_"+p.facialIssue).c_str()));}}attachmentPending=std::move(p);attachmentDeadline=PerformanceActors::ControlTime()+10;}}catch(const std::exception&e){if(core.State()==SessionState::Preparing)throw;SKSE::log::warn("Discarded load result: {}",e.what());}}
 if(beginPending&&core.State()==SessionState::Idle&&clockUI&&!clockUI->GameIsPaused()&&!PhotoMode::Active()){if(PerformanceActors::ControlTime()>=beginDeadline){beginPending=false;restageStartRequested=false;performers.CancelRestage(participantFaction);Notify(L10n::T("aRestageFailed"));PublishActors();}else BeginPrepare();}

 if(attachmentPending&&clockUI&&!clockUI->GameIsPaused()&&!PhotoMode::Active()){auto*p=RE::PlayerCharacter::GetSingleton();auto issue=performers.ValidateStart(p?p->GetParentCell():nullptr,!cfg.combatStop);if((issue!=PerformanceActors::StartIssue::Loading&&issue!=PerformanceActors::StartIssue::Recovering)||PerformanceActors::ControlTime()>=attachmentDeadline){auto ready=std::move(*attachmentPending);attachmentPending.reset();AttachPrepared(std::move(ready));}}
 ProcessCommands();auto oldForceFailures=performers.forcedInvalidations;if(SafeWorld()&&core.State()!=SessionState::Stopping)performers.HoldPlaced(core.State()!=SessionState::Playing,PerformanceActors::ControlTime(),participantFaction);if(oldForceFailures!=performers.forcedInvalidations){SKSE::log::warn("Placement retired: reason={} count={}",static_cast<unsigned>(performers.lastForceIssue),performers.forcedInvalidations);Notify(L10n::T(performers.lastForceIssue==PerformanceActors::ForceIssue::Loading?"aActorLoadTimeout":performers.lastForceIssue==PerformanceActors::ForceIssue::Unsafe?"aActorMissing":"aActorControl"));rosterRevision++;PublishActors();}if(core.State()==SessionState::Idle)return;if(core.State()==SessionState::Stopping){Cleanup();return;}if(core.Tick(PerformanceActors::ControlTime())){for(auto&w:actorWatches)if(w->generated.load()<3)if(auto a=w->actor.get())Notify(std::string(L10n::T("aGraph"))+a->GetName());Notify(L10n::T("r0122"));Cleanup();return;}
 if(core.State()==SessionState::Playing)performanceHeads.Update(performers.slots);
 if(!prepared)return;if(!ParticipantsValid()){Stop(L10n::T(participantIssue),true);Cleanup();return;}if(participantsWaiting){graphPlayable=false;ApplyPlaybackPause(false);return;}if(!SafeWorld()||RE::PlayerCharacter::GetSingleton()->IsDead()){Stop(L10n::T("r0123"));Cleanup();return;}auto*player=RE::PlayerCharacter::GetSingleton();auto*pc=RE::PlayerCamera::GetSingleton();if(!pc)throw std::runtime_error(L10n::T("r0124"));auto actor=cameraActor.get();
 Vec displacement=Vector(player->GetPosition()-originalPosition);auto*currentCell=player->GetParentCell();const bool withCamera=recordedCamera.load();bool sameState=pc->currentState==pc->GetRuntimeData().cameraStates[RE::CameraState::kFree],sameTarget=pc->cameraTarget==(withCamera?cameraActor:savedTarget);
 auto fault=CheckStage(!withCamera||actor!=nullptr,PerformanceActors::SameSpace(currentCell,stageCell),displacement);if(fault!=StageFault::None){SKSE::log::warn("Stage guard: {}; displacement=({},{},{}) distance={}; cellSame={} cameraStateSame={} cameraTargetSame={} actorPresent={} stateID={}",StageFaultName(fault),displacement.x,displacement.y,displacement.z,std::sqrt(Dot(displacement,displacement)),currentCell==stageCell,sameState,sameTarget,actor!=nullptr,pc->currentState?static_cast<unsigned>(pc->currentState->id):0xFFFFFFFFu);Stop(StageFaultName(fault),true);Cleanup();return;}
 Vec playerPosition=Vector(player->GetPosition());maxPlayerStepSquared=std::max(maxPlayerStepSquared,Dot(playerPosition-previousPlayerPosition,playerPosition-previousPlayerPosition));previousPlayerPosition=playerPosition;
 auto subject=performers.CameraActor();if(!subject){Stop(L10n::T("aLost"),true);Cleanup();return;}stage=stableStage.Update(Vector(subject->GetPosition()),Now());stageRotation=Yaw(-subject->GetAngleZ());
 if(withCamera){
  if(auto*cc=actor->GetCharController();cc&&(!cc->flags.any(RE::CHARACTER_FLAGS::kNoSim)||!cc->flags.any(RE::CHARACTER_FLAGS::kNoCharacterCollisions)))++protectionResets;
  ProtectCameraActor(actor.get());
  // The render camera has no collider. Keep the only physical helper away,
  // even if another plugin/late controller creation moved it from parking.
  if(Now()>=nextHelperRepark&&Dot(Vector(actor->GetPosition())-playerPosition,Vector(actor->GetPosition())-playerPosition)<256.*256.){nextHelperRepark=Now()+.25;actor->SetPosition(Point(CameraParking(playerPosition)),true);ProtectCameraActor(actor.get());if(Now()>=nextParkWarning){SKSE::log::warn("Camera helper approached dancer; re-parked without stopping");nextParkWarning=Now()+5;}}
  auto*actorRoot=actor->Get3D(false);if(cameraAnchor&&cameraAnchor->parent!=actorRoot)cameraAnchor.reset();
  if(auto*root=actorRoot;root&&!cameraAnchor){actor->SetAlpha(0);if(auto*n=root->AsNode()){cameraAnchor.reset(RE::NiNode::Create());if(cameraAnchor){cameraAnchor->name="DanceStudioCameraAnchor";n->AttachChild(cameraAnchor.get());}}}
  if(core.State()!=SessionState::Playing)core.Ready(core.Generation(),ReadyPart::CameraActor3D,cameraAnchor!=nullptr,PerformanceActors::ControlTime());
  if(core.State()==SessionState::Playing&&Now()>=nextCollisionLog){auto*cc=actor->GetCharController();RE::CFilter filter{};if(cc)cc->GetCollisionFilterInfo(filter);SKSE::log::info("Camera physics: helperDistance={} playerMaxStep={} controller={} noSim={} noCharacterCollision={} filterLayer={} filterNoCollision={} protectionResets={} stageOffset={}",std::sqrt(Dot(Vector(actor->GetPosition())-playerPosition,Vector(actor->GetPosition())-playerPosition)),std::sqrt(maxPlayerStepSquared),cc!=nullptr,cc&&cc->flags.any(RE::CHARACTER_FLAGS::kNoSim),cc&&cc->flags.any(RE::CHARACTER_FLAGS::kNoCharacterCollisions),cc?static_cast<int>(filter.GetCollisionLayer()):-1,cc&&filter.QNoCollision(),protectionResets,std::sqrt(Dot(stage-playerPosition,stage-playerPosition)));maxPlayerStepSquared=0;protectionResets=0;SKSE::log::info("Camera path scale: mode=subject-XYZ-captured source={} effective={} reference={} baseHeight={} sceneWorld={} skeletonWorld={} referenceScale={} ratio={} manualOffset={}",playerHeightScale.selected.source,playerHeightScale.selected.value,playerHeightScale.reference,playerHeightScale.base,playerHeightScale.scene,playerHeightScale.skeleton,prepared->camera.referenceScale,sessionScale.Ratio(prepared->camera.referenceScale),prepared->camera.heightOffset);nextCollisionLog=Now()+5;}
 }
 core.Ready(core.Generation(),ReadyPart::WorldValid,true,PerformanceActors::ControlTime());

 bool paused=ExternalPauseReasons()!=0;if(ReclaimPhotoCamera(withCamera,photoActive.load(),cameraReturnPending,recovery.CameraDue(Now(),!sameState||!sameTarget,paused))){
  if(!sameState)pc->ForceThirdPerson();pc->cameraTarget=cameraActor;if(!sameState)pc->SetState(pc->GetRuntimeData().cameraStates[RE::CameraState::kFree].get());ownedFov=static_cast<float>(wheelFov.Value(prepared->camera.At(core.Seconds()).fov));pc->GetRuntimeData2().worldFOV=ownedFov;cameraOwned=true;cameraReturnPending=false;
  if(Now()>=nextPriorityLog){SKSE::log::info("Dance priority: camera reclaimed; stateChanged={} targetChanged={}",!sameState,!sameTarget);nextPriorityLog=Now()+5;}
 }

 if(withCamera&&!photoActive.load()&&!PhotoMode::Active()&&!interop::CameraConfirmed()){Stop("SmoothCam camera ownership lost",true);return;}
 if(wasPaused&&!paused)resumeGraceUntil=Now()+1.;wasPaused=paused;if(withCamera&&!photoActive.load()&&!cameraCalibrated&&!paused&&++calibrationTicks>3){if(Calibrate())cameraCalibrated=true;}
 bool allGenerated=true;
 for(auto&w:actorWatches){bool valid=w->generated.load()>=3&&(paused||Now()-w->generatedAt.load()<1.0||Now()<resumeGraceUntil);allGenerated&=valid;}
 if(prepared->hasDance&&!retryIdle&&Now()-preparedAt>1.0&&!allGenerated){retryIdle=true;for(auto&w:actorWatches)if(w->generated.load()<3)if(auto a=w->actor.get()){oar->ClearConditionStateData(a.get());a->NotifyAnimationGraph("IdleForceDefaultState");}}
 bool releasing=facialTest.PendingRelease()||facialPlayback.Releasing();for(auto&f:actorFaces)if(f&&f->Releasing())releasing=true;
 bool graphReady=(core.State()==SessionState::Playing||!releasing)&&cameraCalibrated&&(!prepared->hasDance||(allGenerated&&Now()-preparedAt>.7));
 if(core.State()!=SessionState::Playing)core.Ready(core.Generation(),ReadyPart::PlayerGraph,graphReady,PerformanceActors::ControlTime());else{
  if(!graphReady&&!paused){if(!recoveringGraph){recoveringGraph=true;SKSE::log::info("Participant graph interrupted: holding common audio clock");}if(recovery.GraphDue(Now(),false,paused))for(auto&w:actorWatches)if(Now()-w->generatedAt.load()>=1.)if(auto a=w->actor.get()){oar->ClearConditionStateData(a.get());a->NotifyAnimationGraph("IdleForceDefaultState");}}
  else if(graphReady&&recoveringGraph){recoveringGraph=false;SKSE::log::info("All participant graphs recovered");}
 }
 if(core.State()==SessionState::Stopping){Cleanup();return;}
 if(core.State()==SessionState::Preparing&&Now()>nextDiagnostic){nextDiagnostic=Now()+5;auto obs=observation.load();SKSE::log::info("Preparing: actor3D={} cameraCalibrated={} generated={} epoch={} observed={} mod='{}' submod='{}' path='{}' bindingDuration={} generatorBindingDuration={} hasControl={} hasControlBinding={}",cameraAnchor!=nullptr,cameraCalibrated,generated.load(),core.Generation(),observedEpoch.load(),obs?obs->mod:"",obs?obs->submod:"",obs?obs->path:"",obs?obs->duration:0,obs?obs->generatorDuration:0,obs&&obs->hasControl,obs&&obs->hasControlBinding);if(obs&&IsDanceMod(obs->mod.c_str())){auto&d=obs->load;SKSE::log::info("Loader clip snapshot: userData=0x{:X} bindingIndex={} names={} bindings={} tableEntry={} mode={} flags=0x{:X} registeredPath='{}'",d.userData,d.index,d.names,d.bindings,d.entry,d.mode,d.flags,d.registeredPath);if(probedLoadPath.empty()&&!d.registeredPath.empty()){probedLoadPath=d.registeredPath;ProbeAnimationResource(d.registeredPath);}}
 if(obs&&IsDanceMod(obs->mod.c_str())&&obs->duration<=0)SKSE::log::warn("OAR selected replacement but the playback control has no usable animation; READY withheld. Inspect control/generator binding diagnostics before attributing the failure to HKX: {}",obs->path);}
 if(core.State()==SessionState::Armed&&!announced){announced=true;Notify(L10n::T("r0125"));}if(core.State()==SessionState::Armed&&core.PendingStart()){commands.fetch_or(16);ProcessCommands();}
 graphPlayable=graphReady;
 if(core.State()==SessionState::Playing){ApplyPlaybackPause(true);paused=(pauseReasons.load()&~PauseGraph)!=0;if(paused||!graphReady)progressWatch.Reset();if(!paused&&graphReady){auto frames=audio.Frames();if(progressWatch.Stalled(Now(),frames,true,cfg.stallSeconds))throw std::runtime_error(L10n::T("r0126"));core.AudioProgress(core.Generation(),frames,audio.Rate());if(core.State()==SessionState::Stopping){Cleanup();return;}if(core.Seconds()>=prepared->song.duration-1./audio.Rate()||audio.Done()){Stop(L10n::T("r0127"));Cleanup();return;}}}
 if(withCamera&&!photoActive.load()&&Now()>=nextFovTrace){nextFovTrace=Now()+5.;if(auto*camera=FindCamera(pc->cameraRoot.get())){auto f=camera->GetRuntimeData2().viewFrustum;SKSE::log::info("FOV projection diagnostic: desired={} owned={} world={} left={} right={} top={} bottom={} near={} ortho={}",wheelFov.Value(prepared->camera.At(core.Seconds()).fov),ownedFov,pc->GetRuntimeData2().worldFOV,f.fLeft,f.fRight,f.fTop,f.fBottom,f.fNear,f.bOrtho);}}
 if(core.State()==SessionState::Playing&&pauseReasons.load()==0){if(auto*frame=facialEvaluation.Next(core.Seconds())){auto started=facialMetrics?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};unsigned participants=0;for(unsigned i=0;i<MaxPerformers;++i)if(performers.slots[i].actor)if(auto&f=actorFaces[i]){++participants;++facialApplications;f->Apply(*frame);}facialMaxActors=std::max(facialMaxActors,participants);if(facialMetrics)facialApplyNs+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count();}if(!facialEvaluationWarned&&!facialEvaluation.Error().empty()){facialEvaluationWarned=true;SKSE::log::warn("{}; music/dance/camera continue",facialEvaluation.Error());}}
 ProcessWheelQueue();Publish(core.State()==SessionState::Playing?core.Seconds():0);
}
using UpdateFn=void(*)();UpdateFn originalUpdate{};bool pumpInstalled{};std::atomic_bool entryHooksReady{};
void SyncPhotoState(){if(deferredInteractive&&!PhotoMode::Active()&&!photoRequestUntil){if(world.load())if(auto*pc=RE::PlayerCamera::GetSingleton())ExitInteractiveCamera(*pc,deferredInteractive->target,deferredInteractive->first);deferredInteractive.reset();}if(deferredFov&&!PhotoMode::Active()&&!photoRequestUntil){auto*pc=RE::PlayerCamera::GetSingleton();if(world.load()&&pc)if(auto value=deferredFov->Value(pc->GetRuntimeData2().worldFOV))pc->GetRuntimeData2().worldFOV=*value;deferredFov.reset();}engineFrozen=PhotoMode::Frozen();bool active=PhotoMode::Active();if(active)photoRequestUntil=0;else if(photoRequestUntil&&Now()>=photoRequestUntil){photoRequestUntil=0;BestEffort([]{Notify(L10n::T("r0128"));});}bool previousPhoto=photoActive.load();photoActive=active||photoRequestUntil>Now();if(previousPhoto&&!photoActive.load()&&cameraOwned&&recordedCamera.load())cameraReturnPending=true;if(photoActive.load()){if(auto previous=view.exchange(nullptr)){auto*pc=RE::PlayerCamera::GetSingleton();if(pc&&pc->currentState.get()==previous->state){auto*free=static_cast<RE::FreeCameraState*>(previous->state);auto subject=previous->subject.get();auto*player=subject.get();auto position=player?Vector(player->GetPosition()):previous->origin;auto basis=player?Yaw(-player->GetAngleZ()):previous->basis;free->translation=Point(FollowCameraPosition(previous->position,previous->origin,previous->basis,position,basis));}}}ApplyPlaybackPause(false);}
bool WheelAllowedNow(){
 auto*pc=RE::PlayerCamera::GetSingleton();auto*ui=RE::UI::GetSingleton();auto*cm=RE::ControlMap::GetSingleton();
 bool owned=pc&&cameraOwned&&pc->cameraTarget==cameraActor&&pc->currentState==pc->GetRuntimeData().cameraStates[RE::CameraState::kFree];
 bool blocked=!world.load()||!running.load()||!prepared||!ui||!cm||!Focused()||settings.Open()||menuSuspended.load()||(ui&&(ui->GameIsPaused()||ui->IsMenuOpen(RE::Console::MENU_NAME)))||(cm&&cm->GetRuntimeData().textEntryCount>0);
 return MayAdjustWheelFov(core.State(),recordedCamera.load(),owned,photoActive.load()||PhotoMode::Active(),blocked);
}
void UpdateWheelAdmission(){wheelQueue.Configure(core.Generation(),WheelAllowedNow());}
void ProcessWheelQueue(){
 UpdateWheelAdmission();auto batch=wheelQueue.Drain(core.Generation());if(!batch.count)return;
 const auto source=prepared->camera.At(core.Seconds()).fov;for(std::size_t i=0;i<batch.count;++i)wheelFov.Step(source,batch.steps[i]);
 SKSE::log::info("Wheel FOV applied: events={} updateThread={} source={} desired={}",batch.count,GetCurrentThreadId(),source,wheelFov.Value(source));
}
void MainUpdate(){
 if(!entryHooksReady.load(std::memory_order_acquire)){originalUpdate();return;}
 DWORD current=GetCurrentThreadId(),unset=0;mainThread.compare_exchange_strong(unset,current);if(current!=mainThread.load()){originalUpdate();return;}
 BestEffort([]{SyncPhotoState();});BestEffort([]{UpdateWheelAdmission();});originalUpdate();BestEffort([]{SyncPhotoState();});
 ++completedEngineUpdates;BestEffort([]{PumpCameraRetirement();});static double nextActorRestore{};if(Now()>=nextActorRestore){nextActorRestore=Now()+1;BestEffort([]{auto before=performers.Pending();auto failed=performers.releaseFailures,abandoned=performers.abandoned;performers.RetryReleases(world.load(),participantFaction);if(before!=performers.Pending()){rosterRevision++;PublishActors();}if(before!=performers.Pending()||failed!=performers.releaseFailures||abandoned!=performers.abandoned)SKSE::log::info("NPC restore retry: pending={} failures={} abandoned={}",performers.Pending(),performers.releaseFailures,performers.abandoned);if(audio.Faulted()){auto codes=std::array{audio.lastReset,audio.lastUnprepare,audio.lastClose};audio.Close();if(!audio.Faulted())SKSE::log::info("WinMM cleanup recovered");else if(codes!=std::array{audio.lastReset,audio.lastUnprepare,audio.lastClose})SKSE::log::warn("WinMM cleanup still pending reset={} unprepare={} close={}",audio.lastReset,audio.lastUnprepare,audio.lastClose);}});}
 for(auto&f:actorFaces)if(f&&f->Releasing())f->Drain(world.load(),PhotoMode::Frozen()||(RE::UI::GetSingleton()&&RE::UI::GetSingleton()->GameIsPaused()));
 if(facialPlayback.Releasing()||facialTest.PendingRelease()){bool suspended=PhotoMode::Frozen()||(RE::UI::GetSingleton()&&RE::UI::GetSingleton()->GameIsPaused());if(facialPlayback.Releasing())facialPlayback.Drain(world.load(),suspended);if(facialTest.PendingRelease())facialTest.PollRestore(world.load(),suspended);}
 facialTest.Pump(world.load(),!facialPlayback.Releasing()&&!(prepared&&!prepared->facial.segments.empty()));
 if(!world&&core.State()==SessionState::Idle&&!loadFuture.valid()){wheelQueue.Disable();return;}
 try{Tick();inputState=core.State();UpdateWheelAdmission();}catch(const std::exception&e){Stop(nullptr,true);Cleanup();EndActorMode(false);BestEffort([]{RestoreParticipants(world.load(),false);});inputState=core.State();BestEffort([&]{Notify(std::string(L10n::T("r0151"))+e.what());});}catch(...){Stop(nullptr,true);Cleanup();EndActorMode(false);BestEffort([]{RestoreParticipants(world.load(),false);});inputState=core.State();}
}

using DispatchFn=void(*)(RE::BSTEventSource<RE::InputEvent*>*,RE::InputEvent**);
DispatchFn originalDispatch{};
bool HandleWheelFov(RE::ButtonEvent&button,std::uint64_t token){
 if(button.GetDevice()!=RE::INPUT_DEVICE::kMouse)return false;
 const auto code=button.GetIDCode();if(code!=RE::BSWin32MouseDevice::Key::kWheelUp&&code!=RE::BSWin32MouseDevice::Key::kWheelDown)return false;
 return wheelQueue.Input(code==RE::BSWin32MouseDevice::Key::kWheelUp?-1:1,button.IsPressed(),token);
}
void DispatchInput(RE::BSTEventSource<RE::InputEvent*>*source,RE::InputEvent**events){
 if(!entryHooksReady.load(std::memory_order_acquire)){originalDispatch(source,events);return;}
 static ConcurrentSpaceGate space;auto state=inputState.load();bool active=state!=SessionState::Idle||actorMode.load()!=0;static bool confirmOwned{},escapeOwned{};
 if(!events||(!active&&!space.Owned()&&!confirmOwned&&!escapeOwned)){originalDispatch(source,events);return;}
 if(actorMode.load())space.Reset();
 const auto wheelToken=wheelQueue.Token();
 std::vector<std::pair<RE::InputEvent*,RE::InputEvent*>> links;struct Restore{decltype(links)& saved;void Run(){for(auto [e,next]:saved)e->next=next;saved.clear();}~Restore(){Run();}}restore{links};RE::InputEvent*head=nullptr,*tail=nullptr;
 try{
 for(auto*e=*events;e;){auto*next=e->next;links.emplace_back(e,next);bool consume=false;
  if(auto*b=e->AsButtonEvent();b&&b->GetDevice()==RE::INPUT_DEVICE::kKeyboard){auto code=b->GetIDCode();if((actorMode.load()&&code==hotkeys.Current().start)||confirmOwned&&code==hotkeys.Current().start){consume=true;if(b->IsDown()&&!confirmOwned){confirmOwned=true;commands.fetch_or(256);}if(b->IsUp())confirmOwned=false;}else if(((actorMode.load()||PreparationCancelOnEscape(inputState.load(),beginPending.load()))&&code==1)||escapeOwned&&code==1){consume=true;if(b->IsDown()&&!escapeOwned){escapeOwned=true;commands.fetch_or(actorMode.load()?512u:4u);}if(b->IsUp())escapeOwned=false;}}
  if(!consume&&!actorMode.load())if(auto*b=e->AsButtonEvent();b&&b->GetDevice()==RE::INPUT_DEVICE::kKeyboard&&b->GetIDCode()==57){auto decision=space.Event(SpaceState(state,actorMode.load()!=0),b->IsDown(),b->IsUp());consume=decision.consume;if(decision.toggle)ToggleUserPauseNow();}
  if(!consume)if(auto*b=e->AsButtonEvent())consume=HandleWheelFov(*b,wheelToken);
  if(!consume){if(tail)tail->next=e;else head=e;tail=e;}e=next;
 }
 if(tail)tail->next=nullptr;
 }catch(...){restore.Run();originalDispatch(source,events);return;}
 originalDispatch(source,&head);
}
bool InstallEntryHooks(){static bool failed=false;if(failed)return false;if(entryHooksReady.load())return true;auto fail=[&](){failed=true;return false;};auto v=REL::Module::get().version();if(!SupportedRuntime(v[0],v[1],v[2],v[3]))return fail();auto h=HooksFor(v[1],v[2]);REL::Relocation<std::uintptr_t> update{REL::ID(h.updateId)},input{REL::ID(h.inputId)};CallPatch a{update.address()+h.updateOffset},b{input.address()+h.inputOffset};
 if(!a.Read()||!b.Read()||!Executable(a.site)||!Executable(b.site)||!Executable(a.old)||!Executable(b.old)){SKSE::log::error("Entry hook preflight failed; neither hook installed runtime={} updateId={} offset={} inputId={} offset={}",v.string(),h.updateId,h.updateOffset,h.inputId,h.inputOffset);return fail();}
 SKSE::log::info("Entry hook preflight runtime={} updateSite={} updateCallee={} inputSite={} inputCallee={}; E8/execute guards passed; ABI/gameplay must be verified per runtime",v.string(),a.site,a.old,b.site,b.old);
 SKSE::AllocTrampoline(128);auto memory=reinterpret_cast<std::uintptr_t>(SKSE::GetTrampoline().allocate(32));if(!a.Prepare(memory,reinterpret_cast<std::uintptr_t>(MainUpdate))||!b.Prepare(memory+16,reinterpret_cast<std::uintptr_t>(DispatchInput)))return fail();
 originalUpdate=reinterpret_cast<UpdateFn>(a.old);originalDispatch=reinterpret_cast<DispatchFn>(b.old);std::atomic_thread_fence(std::memory_order_release);if(!InstallCallPair(a,b)){SKSE::log::error("Entry hook transaction failed; rollback attempted; LDA:R inactive");return fail();}pumpInstalled=true;entryHooksReady.store(true,std::memory_order_release);return true;}

struct Input:RE::BSTEventSink<RE::InputEvent*> {RE::BSEventNotifyControl ProcessEvent(RE::InputEvent*const* list,RE::BSTEventSource<RE::InputEvent*>*)override{if(!list||!world||settings.Open()||actorMode.load())return RE::BSEventNotifyControl::kContinue;auto*cm=RE::ControlMap::GetSingleton();auto*ui=RE::UI::GetSingleton();if(!cm||!ui||cm->GetRuntimeData().textEntryCount>0||ui->IsMenuOpen(RE::Console::MENU_NAME))return RE::BSEventNotifyControl::kContinue;const auto keys=hotkeys.Current();for(auto*e=*list;e;e=e->next)if(auto*b=e->AsButtonEvent();b&&b->GetDevice()==RE::INPUT_DEVICE::kKeyboard&&b->IsDown()){auto k=b->GetIDCode();if(keys.photo&&k==keys.photo&&PhotoMode::available.load())commands.fetch_or(128);else if(keys.hud&&k==keys.hud)commands.fetch_or(32);else if(keys.stop&&k==keys.stop)commands.fetch_or(4);else {if(keys.start&&k==keys.start){commands.fetch_or(PlayCommandFor(inputState.load()));}}}return RE::BSEventNotifyControl::kContinue;}}input;
struct Menus:RE::BSTEventSink<RE::MenuOpenCloseEvent> {
 std::mutex mutex;std::vector<RE::BSFixedString> pausing;
 RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent*e,RE::BSTEventSource<RE::MenuOpenCloseEvent>*)override{
  if(!e)return RE::BSEventNotifyControl::kContinue;
  std::scoped_lock lock(mutex);menuEpoch.fetch_add(1);
  if(e->opening){auto*ui=RE::UI::GetSingleton();auto menu=ui?ui->GetMenu(e->menuName):nullptr;if(menu&&menu->PausesGame()&&std::find(pausing.begin(),pausing.end(),e->menuName)==pausing.end())pausing.push_back(e->menuName);if(!pausing.empty()&&running.load())try{menuSuspended=true;pauseReasons.fetch_or(PauseMenu);audio.Pause(true);}catch(...){commands.fetch_or(4);}}
  else std::erase(pausing,e->menuName);
  menuSuspended=!pausing.empty();return RE::BSEventNotifyControl::kContinue;
 }
 void Reset(){std::scoped_lock lock(mutex);pausing.clear();menuSuspended=false;}
}menus;

void WorldReset(bool valid){EndActorMode(false);RestoreParticipants(valid,true);performers.deferred.clear();PerformanceAlias::ResetWorld();actorWatches.clear();facialPlayback.Stop(valid);facialTest.Close(valid,"world reset");wheelQueue.Disable();deferredInteractive.reset();deferredFov.reset();photoRequestUntil=0;userPaused=false;photoActive=false;hud.Restore();backgroundMute.Restore();picker.Cancel();pendingPick.reset();++catalogGeneration;commands=0;Stop();Cleanup(valid);deferredInteractive.reset();if(retiringCamera){SKSE::log::info("Camera retirement abandoned on world reset; no delayed Disable/Delete into another world");retiringCamera.reset();}if(!valid){std::scoped_lock lock(clipsLock);clips.clear();clipCount=0;}if(selector)selector->value=0;world=false;inputState=SessionState::Idle;menus.Reset();}
void ResolveBackgroundCategories(){
 backgroundCategories.clear();auto path=std::filesystem::path(L"Data\\SKSE\\Plugins\\DanceStudio\\BackgroundAudio.json");

 auto fallback=[](){backgroundCategories.clear();auto*defaults=RE::BGSDefaultObjectManager::GetSingleton();if(defaults)if(auto*music=defaults->GetObject<RE::BGSSoundCategory>(RE::BGSDefaultObjectManager::DefaultObject::kMusicSoundCategory))backgroundCategories.push_back(music);};
 try{if(!std::filesystem::exists(path))path.replace_filename(L"BackgroundAudio.defaults.json");if(std::filesystem::exists(path)){auto j=ReadJson(path,16384);if(j.at("schemaVersion")!=1||!j.at("categories").is_array()||j.at("categories").size()>64)throw std::runtime_error(L10n::T("r0129"));for(auto&row:j.at("categories")){auto plugin=row.at("plugin").get<std::string>(),text=row.at("formId").get<std::string>();size_t used=0;auto id=std::stoul(text,&used,16);if(used!=text.size()||id>0xFFFFFF||plugin.empty())throw std::runtime_error(L10n::T("r0130"));auto*category=RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSSoundCategory>(static_cast<RE::FormID>(id),plugin);if(!category)throw std::runtime_error(L10n::T("r0131")+plugin+":"+text);backgroundCategories.push_back(category);}}
 else{fallback();SKSE::log::warn("BackgroundAudio.json missing: only default music category available");}
 }catch(const std::exception&e){fallback();SKSE::log::error("Background audio configuration rejected; music-only fallback: {}",e.what());}

 SKSE::log::info("Background audio configuration: {} categories",backgroundCategories.size());
}
void RegisterMenus(const char*stage){
 if(developerTools)facialTest.Register();
 std::string pickerError,settingsError;bool pickerOK=false,settingsOK=false;
 BestEffort([&]{pickerOK=picker.Register(pickerError);});BestEffort([]{actorManager.Register();});
 BestEffort([&]{settingsOK=settings.Register(settingsError);});
 SKSE::log::info("Menu registration [{}]: settings={} picker={} settingsError='{}' pickerError='{}'",stage,settingsOK,pickerOK,settingsError,pickerError);
}
void Message(SKSE::MessagingInterface::Message*m){if(!m)return;try{switch(m->type){case SKSE::MessagingInterface::kPostLoad:RegisterMenus("PostLoad");
 if(!BestEffort([]{interop::RegisterCamera();}))SKSE::log::error("SmoothCam listener initialization failed; menu registration retained");
 if(!BestEffort([]{interop::InitTDM();}))SKSE::log::error("TDM initialization failed; menu registration retained");break;
 case SKSE::MessagingInterface::kPostPostLoad:RegisterMenus("PostPostLoad");interop::RequestCamera();break;
 case SKSE::MessagingInterface::kInputLoaded:RegisterMenus("InputLoaded");break;
 case SKSE::MessagingInterface::kDataLoaded:{RegisterMenus("DataLoaded");ResolveBackgroundCategories();PhotoMode::Resolve();auto*data=RE::TESDataHandler::GetSingleton();selector=data->LookupForm<RE::TESGlobal>(0x800,Forms);cameraBase=data->LookupForm<RE::TESNPC>(0x801,Forms);participantFaction=data->LookupForm<RE::TESFaction>(0x802,Forms);if(auto*ref=data->LookupForm<RE::Actor>(0x1A6A4,"Skyrim.esm"))PerformanceActors::excludedReference=ref->GetFormID();else SKSE::log::error("Canonical performer exclusion reference unavailable; no name/base-ID fallback");auto mod=GetModuleHandleW(L"OpenAnimationReplacer.dll");auto request=reinterpret_cast<OAR_API::Animations::_RequestPluginAPI_Animations>(GetProcAddress(mod,"RequestPluginAPI_Animations"));if(request)oar=request(OAR_API::Animations::InterfaceVersion::V1,"DanceStudio",REL::Version{RuntimeMajor,RuntimeMinor,RuntimePatch});SKSE::log::info("Runtime dependencies: ESL selector={} cameraBase={} OAR module={} animationsAPI={}",!!selector,!!cameraBase,!!mod,!!oar);if(!selector||!cameraBase||!oar||!PerformanceAlias::Initialize(data)){SKSE::log::error("Missing data ESL or OAR Animations API; plugin inactive");return;}selector->value=0;if(!InstallEntryHooks())throw std::runtime_error(L10n::T("r0133"));SKSE::log::info("Space dispatch filter installed; idle passthrough enabled");RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&input);RE::UI::GetSingleton()->AddEventSink(&menus);available=true;SKSE::log::info("Runtime armed for world load; configurable keys in SKSE Menu > LDA:R > 단축키");break;}
 case SKSE::MessagingInterface::kPreLoadGame:WorldReset(true);break;
 case SKSE::MessagingInterface::kSaveGame:EndActorMode(false);Stop(L10n::T("r0134"));Cleanup(true);RestoreParticipants(true,false);break;
 case SKSE::MessagingInterface::kPostLoadGame:WorldReset(false);world=m->data&&available;if(world){performers.Defaults();rosterRevision++;PublishActors();Select();}break;
 case SKSE::MessagingInterface::kNewGame:WorldReset(false);world=available;if(world){performers.Defaults();rosterRevision++;PublishActors();Select();}break;
 default:break;}}catch(const std::exception&e){SKSE::log::error("Lifecycle failure: {}",e.what());Stop(nullptr,true);try{Cleanup(world.load());}catch(...){SKSE::log::error("Cleanup failed during lifecycle exception");}if(selector)selector->value=0;world=false;watchClips=false;playback.store(nullptr);view.store(nullptr);inputGate=false;audio.Close();inputState=SessionState::Idle;}}
}
extern "C" __declspec(dllexport) bool SKSEPlugin_Query(const SKSE::QueryInterface*s,SKSE::PluginInfo*i){if(!s||!i)return false;i->infoVersion=SKSE::PluginInfo::kVersion;i->name="DanceStudio";i->version=DanceStudio::RuntimeVersionNumber;auto v=s->RuntimeVersion();return !s->IsEditor()&&DanceStudio::SupportedRuntime(v[0],v[1],v[2],v[3]);}
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface*s){try{auto v=s->RuntimeVersion();if(!DanceStudio::SupportedRuntime(v[0],v[1],v[2],v[3]))return false;SKSE::Init(s);auto path=SKSE::log::log_directory();if(!path)return false;*path/="DanceStudio.log";auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(),true);auto logger=std::make_shared<spdlog::logger>("DanceStudio",sink);spdlog::set_default_logger(logger);spdlog::flush_on(spdlog::level::info);DanceStudio::L10n::Init();if(auto error=DS::cameraFraming.Load();!error.empty())SKSE::log::warn("{}",error);DS::ReadConfig();if(!DS::hud.Register())SKSE::log::warn("HUD filter registration failed");
 static std::jthread focusGuard([](std::stop_token stop){while(!stop.stop_requested()){if(DS::running.load()&&!DS::Focused())DanceStudio::BestEffort([]{DS::pauseReasons.fetch_or(DanceStudio::PauseFocus);DS::audio.Pause(true);});std::this_thread::sleep_for(std::chrono::milliseconds(DS::cfg.focusPollMs));}});
 SKSE::log::info("Camera framing policy: actor/reference coordinate contract, captured scale, whole-track pivot");SKSE::log::info("LDA:R {}; runtime={}; SE 1.5.97 / Steam AE 1.6.640 + 1.6.1170; AE gameplay unverified",DanceStudio::RuntimeVersion,REL::Module::get().version().string());return SKSE::GetMessagingInterface()->RegisterListener(DS::Message);}catch(...){return false;}}

SKSE_PLUGIN_VERSION = []{SKSE::PluginVersionData data;data.PluginVersion(REL::Version{DanceStudio::RuntimeMajor,DanceStudio::RuntimeMinor,DanceStudio::RuntimePatch,0});data.PluginName("DanceStudio");data.AuthorName("Horker312");data.UsesAddressLibrary();data.UsesUpdatedStructs();data.CompatibleVersions({REL::Version{1,5,97,0},REL::Version{1,6,640,0},REL::Version{1,6,1170,0}});return data;}();
