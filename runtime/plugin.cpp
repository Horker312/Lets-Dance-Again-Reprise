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
#include "RecoveryPolicy.h"
#include "HUDGate.h"
#include "NativePlaybackControl.h"
#include "ScopedCategoryMute.h"
#include "InteractiveCamera.h"
#include "FovRestoration.h"
#include <thread>
#if !defined(ENABLE_SKYRIM_SE) || defined(ENABLE_SKYRIM_AE) || defined(ENABLE_SKYRIM_VR)
#error Dance Studio requires an SE-only SDK
#endif
static_assert(sizeof(RE::hkbClipGenerator)==0x110);
static_assert(offsetof(RE::hkbClipGenerator,mode)==0x72&&offsetof(RE::hkbClipGenerator,localTime)==0xF0);
static_assert(offsetof(RE::hkaAnimationBinding,animation)==0x18);
static_assert(offsetof(RE::BShkbAnimationGraph,characterInstance)==0xC0);
static_assert(sizeof(RE::FreeCameraState)==0x50);
static_assert(offsetof(RE::Actor,actorState1)==0xC0&&offsetof(RE::Actor,actorState2)==0xC4);
static_assert(offsetof(RE::bhkCharacterController,flags)==0x218&&offsetof(RE::bhkCharacterController,gravity)==0x248);

namespace DS {
using namespace DanceStudio;
constexpr char Forms[]="DanceStudioData.esl";
constexpr wchar_t Ini[]=L"Data\\SKSE\\Plugins\\DanceStudio.ini";
constexpr auto SongFolderRoot=L"Data\\meshes\\actors\\character\\animations\\OpenAnimationReplacer\\Dance Studio";
constexpr auto SongsRoot=L"Data\\SKSE\\Plugins\\DanceStudio\\Songs";
struct Config {double volume{.7},stallSeconds{5};unsigned focusPollMs{100};bool combatStop{true};}cfg;
HUDGate hud;ProgressWatch progressWatch;ScopedCategoryMute<RE::BGSSoundCategory> backgroundMute;std::vector<RE::BGSSoundCategory*> backgroundCategories;
bool Focused(){DWORD process{};auto window=GetForegroundWindow();if(!window)return false;GetWindowThreadProcessId(window,&process);return process==GetCurrentProcessId();}
Hotkeys::Store hotkeys;
SongPicker picker;std::uint64_t catalogGeneration{};std::shared_ptr<const PickerChoice> pendingPick;StableStage stableStage;std::atomic_bool recordedCamera{true};double nextCollisionLog{},nextHelperRepark{},nextParkWarning{},maxPlayerStepSquared{};Vec previousPlayerPosition;unsigned protectionResets{};
PriorityRecovery recovery;bool recoveringGraph{};double nextPriorityLog{};bool sessionNative{};std::atomic<double> nextClipTrace{};
SessionCore core;std::vector<Song> songs;std::string selectedDanceId,selectedCameraId;std::future<std::vector<Song>> catalogFuture;std::future<Prepared> loadFuture;
std::uint64_t loadEpoch{},catalogEpoch{};bool catalogPending{},available{},wasPaused{},installed{},controlsGated{},cameraOwned{},cameraCalibrated{},announced{},retryIdle{};
std::atomic_bool world{},inputGate{},running{},watchClips{},menuSuspended{};std::atomic<unsigned> commands{};std::atomic<SessionState> inputState{SessionState::Idle};std::atomic<DWORD> mainThread{};
bool SettingsIdle(){return inputState.load()==SessionState::Idle;}
Hotkeys::SettingsMenu settings(hotkeys,SettingsIdle,[]{return PhotoMode::available.load();});
std::atomic<bool> engineFrozen{},photoActive{},userPaused{},graphPlayable{};
std::atomic<std::uint32_t> pauseReasons{PauseNoSession};std::uint32_t loggedPauseReasons{~0u};
double photoRequestUntil{};bool cameraReturnPending{};
RE::TESGlobal* selector{};RE::TESNPC* cameraBase{};RE::ActorHandle cameraActor;RE::TESObjectCELL* stageCell{};
std::array<RE::NiPointer<RE::NiAVObject>,2> hiddenRoots;double nextMeshHide{};
RE::NiPointer<RE::NiNode> cameraAnchor;RE::BSTSmartPointer<RE::TESCameraState> savedState;RE::ActorHandle savedTarget;
RE::BSTSmartPointer<RE::BSAnimationGraphManager> graphManager;RE::BSTSmartPointer<RE::BShkbAnimationGraph> playerGraph;
std::optional<Prepared> prepared;AudioDevice audio;Vec stage;Quat stageRotation,calibration;float savedFov{},ownedFov{};
using DeferredFov=FovRestoration;std::optional<DeferredFov> deferredFov;double nextFovTrace{};unsigned calibrationTicks{};
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
}playerHeightScale;
OAR_API::Animations::IAnimationsInterface* oar{};
struct Playback {std::uint64_t epoch{};RE::hkbCharacter* character{};std::string submod;double duration{},seconds{};bool native{},advance{};};
std::atomic<RE::hkbCharacter*> watchedCharacter{};std::atomic<std::shared_ptr<const Playback>> playback;
struct View {RE::TESCameraState* state{};Vec position;Quat rotation;Vec origin;};std::atomic<std::shared_ptr<const View>> view;
struct Observation {std::string mod,submod,path;float duration{};};std::atomic<std::shared_ptr<const Observation>> observation;std::atomic<double> observationAt{};
std::atomic<std::uint64_t> observedEpoch{};std::atomic<unsigned> generated{};std::atomic<double> generatedAt{};
struct ClipSaved {std::uint64_t epoch{};RE::hkaAnimationBinding* binding{};decltype(RE::hkbClipGenerator::mode) mode;float speed{},fraction{};bool nativeStarted{};};
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
std::string Hash(const std::vector<char>& bytes){BCRYPT_ALG_HANDLE alg{};BCRYPT_HASH_HANDLE hash{};auto check=[](NTSTATUS n){if(n<0)throw std::runtime_error("SHA256 unavailable");};std::array<unsigned char,32> out{};try{check(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0));check(BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0));check(BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),static_cast<ULONG>(bytes.size()),0));check(BCryptFinishHash(hash,out.data(),32,0));}catch(...){if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);throw;}BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);std::string result;const char* h="0123456789abcdef";for(auto v:out){result+=h[v>>4];result+=h[v&15];}return result;}
Prepared Load(Song master,std::optional<Song> dance,std::optional<Song> camera,double volume){Prepared p;p.song=std::move(master);p.hasDance=dance.has_value();
 auto track=[](const Song&s,const char*key,std::uintmax_t max){auto name=s.manifest.at(key).get<std::string>();auto bytes=ReadBytes(RelativeFile(s.root,name),max);if(Hash(bytes)!=s.manifest.at("hashes").at(name).get<std::string>())throw std::runtime_error("Track SHA256 mismatch: "+name);return bytes;};
 if(dance){for(const char*key:{"dance","danceFemale"})track(*dance,key,512ull*1024*1024);p.song.token=dance->token;p.song.submod=dance->submod;}else{p.song.token=0;p.song.submod.clear();}
 if(camera){auto bytes=track(*camera,"cameraSamples",384ull*1024*1024);p.camera=CameraTrack::Read(Json::parse(bytes),camera->duration);}auto music=track(p.song,"music",384ull*1024*1024);p.wave=ReadWave(music,p.song.duration);ApplyVolume(p.wave,volume);return p;}


std::vector<Song> Catalog(){std::vector<Song> found;for(auto root:{SongsRoot,SongFolderRoot}){if(!std::filesystem::exists(root))continue;for(auto&e:std::filesystem::directory_iterator(root)){if(!e.is_directory())continue;auto path=e.path()/"song.json";if(!std::filesystem::exists(path))continue;if(found.size()>=512)throw std::runtime_error("More than 512 song packages");try{auto song=ReadSong(path);if(root==SongFolderRoot&&!song.embedded)throw std::runtime_error("Song-folder library requires schema 3");found.push_back(std::move(song));}catch(const std::exception&x){SKSE::log::warn("Rejected song {}: {}",PathText(e.path()),x.what());}}}
 auto selected=SelectSongs(std::move(found));for(auto&entry:selected.collisions)SKSE::log::error("Excluded unsafe song selector collision; remove conflicting copies or re-export: {}",entry);for(auto&path:selected.ignored)SKSE::log::warn("Older/legacy copy of the same song ignored: {}",path);SKSE::log::info("Song catalog: {} selected, {} older copies ignored",selected.songs.size(),selected.ignored.size());return std::move(selected.songs);}

void ReadConfig(){auto read=[](const wchar_t* k,unsigned d){return GetPrivateProfileIntW(L"Keys",k,d,Ini);};
 auto error=hotkeys.Load({read(L"PrepareStart",68),read(L"Stop",66),read(L"ToggleHUD",87)},Hotkeys::MenuKey());
 if(!error.empty())SKSE::log::warn("{}",error);
 auto keys=hotkeys.Current();SKSE::log::info("Hotkeys DIK: play={}, stop={}",keys.start,keys.stop);
 cfg.volume=std::clamp(double(GetPrivateProfileIntW(L"Playback",L"VolumePercent",70,Ini))/100.,0.,1.);cfg.combatStop=GetPrivateProfileIntW(L"Playback",L"StopOnCombat",1,Ini)!=0;cfg.stallSeconds=std::clamp(double(GetPrivateProfileIntW(L"Recovery",L"AudioStallSeconds",5,Ini)),1.,120.);cfg.focusPollMs=std::clamp(GetPrivateProfileIntW(L"Recovery",L"FocusPollMilliseconds",100,Ini),20u,1000u);
 HUDPolicy policy;const auto path=std::filesystem::path(L"Data\\SKSE\\Plugins\\DanceStudio\\HUD.json");if(std::filesystem::exists(path)){try{auto j=ReadJson(path,16384);if(j.at("schemaVersion")!=1||!j.at("movies").is_array()||j.at("movies").size()>128)throw std::runtime_error("Invalid HUD configuration");std::vector<std::string> names;for(auto&v:j.at("movies")){auto name=v.get<std::string>();if(name.empty()||name.size()>128||HUDPolicy::FileName(name)!=name)throw std::runtime_error("HUD movies must be lowercase SWF filenames");names.push_back(std::move(name));}if(j.value("includeDefaults",true)){for(auto&name:names)if(std::find(policy.movies.begin(),policy.movies.end(),name)==policy.movies.end())policy.movies.push_back(std::move(name));}else policy.movies=std::move(names);}catch(const std::exception&e){SKSE::log::warn("HUD configuration rejected; defaults used: {}",e.what());}}hud.Configure(std::move(policy));}


using ClipUpdateFn=void(*)(RE::hkbClipGenerator*,const RE::hkbContext&,float);
using ClipDeactivateFn=void(*)(RE::hkbClipGenerator*,const RE::hkbContext&);
// CommonLib's Generate declaration omits active children/output/timeOffset. This is OAR's five-argument ABI.
using ClipGenerateFn=void(*)(RE::hkbClipGenerator*,const RE::hkbContext&,const RE::hkbGeneratorOutput**,RE::hkbGeneratorOutput&,float);
ClipUpdateFn originalClipUpdate{};ClipDeactivateFn originalClipDeactivate{};ClipGenerateFn originalClipGenerate{};
void RestoreClip(RE::hkbClipGenerator* c,const ClipSaved&s){c->mode=s.mode;c->playbackSpeed=s.speed;c->userControlledTimeFraction=s.fraction;}
bool ClockClip(RE::hkbClipGenerator*c,const RE::hkbContext&ctx,const std::shared_ptr<const Playback>& p){
 if((!p&&clipCount.load()==0)||(p&&ctx.character!=p->character))return false;
 std::scoped_lock lock(clipsLock);auto it=clips.find(c);
 if(it!=clips.end()&&(!p||it->second.epoch!=p->epoch||c->binding!=it->second.binding)){RestoreClip(c,it->second);clips.erase(it);clipCount=static_cast<unsigned>(clips.size());it=clips.end();}
 if(!p||ctx.character!=p->character||!oar)return false;
 auto info=oar->GetCurrentReplacementAnimationInfo(c);
 double last=observationAt.load(),now=Now();if(now-last>1&&observationAt.compare_exchange_strong(last,now))observation.store(std::make_shared<Observation>(Observation{info.modName.c_str(),info.subModName.c_str(),info.animationPath.c_str(),c->binding&&c->binding->animation?c->binding->animation->duration:0}));
 const bool match=IsDanceMod(info.modName.c_str())&&std::string_view(info.subModName.c_str())==p->submod;
 if(!match||!c->binding||!c->binding->animation||std::abs(c->binding->animation->duration-p->duration)>.005){if(it!=clips.end()){RestoreClip(c,it->second);clips.erase(it);clipCount=static_cast<unsigned>(clips.size());}return false;}
 if(it==clips.end()){if(clips.size()>=32)return false;it=clips.emplace(c,ClipSaved{p->epoch,c->binding,c->mode,c->playbackSpeed,c->userControlledTimeFraction}).first;clipCount=static_cast<unsigned>(clips.size());}
 bool freezeNow=ObserveFreezeNow();
 if(p->native&&running.load()){
  ApplyNativePlayback(*c,it->second.nativeStarted,NativeMayAdvance(p->advance,freezeNow,pauseReasons.load()),RE::hkbClipGenerator::PlaybackMode::kModeLooping);return true;
 }
 float time=static_cast<float>(std::clamp(p->seconds,0.,std::max(0.,p->duration-.00001)));
 c->mode=RE::hkbClipGenerator::PlaybackMode::kModeUserControlled;c->playbackSpeed=0;c->userControlledTimeFraction=time/static_cast<float>(p->duration);
 c->localTime=time;c->time=time;if(c->animationControl)c->animationControl->localTime=time;return true;
}
void ClipUpdate(RE::hkbClipGenerator*c,const RE::hkbContext&ctx,float dt){if(ctx.character!=watchedCharacter.load()||(!watchClips.load()&&clipCount.load()==0)){originalClipUpdate(c,ctx,dt);return;}auto p=playback.load();ClockClip(c,ctx,p);originalClipUpdate(c,ctx,dt);ClockClip(c,ctx,p);}
void ClipDeactivate(RE::hkbClipGenerator*c,const RE::hkbContext&ctx){if(clipCount.load()==0){originalClipDeactivate(c,ctx);return;}{std::scoped_lock lock(clipsLock);auto i=clips.find(c);if(i!=clips.end()){RestoreClip(c,i->second);clips.erase(i);clipCount=static_cast<unsigned>(clips.size());}}originalClipDeactivate(c,ctx);}
void ClipGenerate(RE::hkbClipGenerator*c,const RE::hkbContext&ctx,const RE::hkbGeneratorOutput** children,RE::hkbGeneratorOutput& output,float offset){if(ctx.character!=watchedCharacter.load()||(!watchClips.load()&&clipCount.load()==0)){originalClipGenerate(c,ctx,children,output,offset);return;}auto p=playback.load();bool match=ClockClip(c,ctx,p);originalClipGenerate(c,ctx,children,output,offset);if(match&&p){if(observedEpoch.exchange(p->epoch)!=p->epoch)generated=0;generated.fetch_add(1);generatedAt=Now();double now=Now(),due=nextClipTrace.load();if(now>=due&&nextClipTrace.compare_exchange_strong(due,now+1.))SKSE::log::info("Playback comparison: native={} audioTime={} clipTime={} drift={} advance={}",p->native,p->seconds,c->localTime,c->localTime-p->seconds,p->advance);}}
using CameraRotationFn=void(*)(RE::TESCameraState*,RE::NiQuaternion&);using CameraTranslationFn=void(*)(RE::TESCameraState*,RE::NiPoint3&);
CameraRotationFn originalRotation{};CameraTranslationFn originalTranslation{};
void CameraRotation(RE::TESCameraState*s,RE::NiQuaternion&q){auto v=view.load();if(v&&!photoActive.load()&&!PhotoMode::Active()&&s==v->state){q=Quaternion(v->rotation);return;}originalRotation(s,q);}
void CameraTranslation(RE::TESCameraState*s,RE::NiPoint3&p){auto v=view.load();if(v&&!photoActive.load()&&!PhotoMode::Active()&&s==v->state){auto*player=RE::PlayerCharacter::GetSingleton();Vec delta=player?Vector(player->GetPosition())-v->origin:Vec{};if(std::isfinite(Dot(delta,delta)))p=Point(v->position+delta);else p=Point(v->position);return;}originalTranslation(s,p);}
using InputFn=RE::BSEventNotifyControl(*)(RE::PlayerControls*,RE::InputEvent*const*,RE::BSTEventSource<RE::InputEvent*>*);
InputFn originalPlayerInput{};
RE::BSEventNotifyControl PlayerInput(RE::PlayerControls*p,RE::InputEvent*const* e,RE::BSTEventSource<RE::InputEvent*>*s){if(!inputGate.load())return originalPlayerInput(p,e,s);if(!recordedCamera.load()||photoActive.load())return originalPlayerInput(p,e,s);p->data.moveInputVec={0,0};p->data.lookInputVec={0,0};p->data.prevMoveVec={0,0};p->data.prevLookVec={0,0};p->data.autoMove=false;
 return RE::BSEventNotifyControl::kContinue;}


bool InstallRuntimeHooks(){if(installed)return true;
 REL::Relocation<std::uintptr_t> clip{RE::VTABLE_hkbClipGenerator[0]},cam{RE::VTABLE_FreeCameraState[0]},pc{RE::VTABLE_PlayerControls[0]};
 for(auto [table,slot]:{std::pair{clip.address(),5u},{clip.address(),7u},{clip.address(),0x17u},{cam.address(),4u},{cam.address(),5u},{pc.address(),1u}})if(!Executable(reinterpret_cast<std::uintptr_t*>(table)[slot]))return false;
 originalClipUpdate=reinterpret_cast<ClipUpdateFn>(reinterpret_cast<std::uintptr_t*>(clip.address())[5]);originalClipDeactivate=reinterpret_cast<ClipDeactivateFn>(reinterpret_cast<std::uintptr_t*>(clip.address())[7]);originalClipGenerate=reinterpret_cast<ClipGenerateFn>(reinterpret_cast<std::uintptr_t*>(clip.address())[0x17]);originalRotation=reinterpret_cast<CameraRotationFn>(reinterpret_cast<std::uintptr_t*>(cam.address())[4]);originalTranslation=reinterpret_cast<CameraTranslationFn>(reinterpret_cast<std::uintptr_t*>(cam.address())[5]);originalPlayerInput=reinterpret_cast<InputFn>(reinterpret_cast<std::uintptr_t*>(pc.address())[1]);
 // Publish only after every chained function is initialized: NPC jobs may run concurrently.
 clip.write_vfunc(5,ClipUpdate);clip.write_vfunc(7,ClipDeactivate);clip.write_vfunc(0x17,ClipGenerate);cam.write_vfunc(4,CameraRotation);cam.write_vfunc(5,CameraTranslation);pc.write_vfunc(1,PlayerInput);installed=true;SKSE::log::info("Chained clip, free-camera and player-only input hooks installed");return true;
}
RE::NiCamera* FindCamera(RE::NiAVObject* node,unsigned depth=0){if(!node||depth>16)return nullptr;if(auto*c=netimmerse_cast<RE::NiCamera*>(node))return c;if(auto*n=node->AsNode())for(auto&child:n->GetChildren())if(auto*c=FindCamera(child.get(),depth+1))return c;return nullptr;}
bool Calibrate(){auto* pc=RE::PlayerCamera::GetSingleton();auto* cam=FindCamera(pc->cameraRoot.get());if(!cam)return false;RE::NiPoint3 origin,direction;if(!cam->WindowPointToRay(500,500,origin,direction,1000,1000))return false;auto f=Vector(direction);if(Dot(f,f)<.01||std::abs(Unit(f).z)>.995)return false;
 // Built-in FreeCameraState has yaw/pitch and no roll. World up avoids screen-Y convention assumptions.
 auto basis=Look(f,{0,0,1});RE::NiQuaternion q;originalRotation(pc->currentState.get(),q);calibration=Unit(basis.Inv()*Quaternion(q));return true;}

bool SafeWorld(){auto*p=RE::PlayerCharacter::GetSingleton();auto*ui=RE::UI::GetSingleton();return world&&p&&p->GetParentCell()&&p->Get3D(false)&&ui&&!ui->IsMenuOpen(RE::MainMenu::MENU_NAME)&&!ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);}
bool SafePlayer(){auto*p=RE::PlayerCharacter::GetSingleton();return SafeWorld()&&!p->IsDead()&&!p->AsActorState()->IsBleedingOut()&&!p->IsOnMount()&&!p->AsActorState()->IsSwimming()&&!p->AsActorState()->IsSneaking()&&static_cast<int>(p->AsActorState()->GetSitSleepState())==0&&static_cast<int>(p->AsActorState()->GetKnockState())==0&&(!cfg.combatStop||!p->IsInCombat());}
void Stop(const char* reason=nullptr,bool failed=false){if(core.Stop(failed)&&reason)Notify(reason);running=false;watchClips=false;playback.store(nullptr);view.store(nullptr);}
void Cleanup(bool touchingWorld=true)noexcept{if(core.State()!=SessionState::Stopping)return;
 // Release our input/render gates BEFORE optional engine restoration.
 inputGate=false;running=false;watchClips=false;playback.store(nullptr);view.store(nullptr);controlsGated=false;graphPlayable=false;pauseReasons=PauseNoSession;loggedPauseReasons=~0u;progressWatch.Reset();
 auto attempt=[](auto&&action){if(!BestEffort(action))BestEffort([]{SKSE::log::error("A cleanup step failed; continuing independent recovery steps");});};
 attempt([]{picker.Cancel();});pendingPick.reset();recovery.Reset();recoveringGraph=false;nextPriorityLog=0;
 attempt([]{if(!backgroundMute.Restore())SKSE::log::error("Background volume restoration pending");if(auto*manager=RE::BSAudioManager::GetSingleton())manager->ComposeMessage(RE::SOUND_MSG::VolumeChanged);});
 attempt([]{audio.Close();});if(selector)selector->value=0;
 attempt([&]{auto*pc=RE::PlayerCamera::GetSingleton();if(touchingWorld&&pc&&cameraOwned){if(PhotoMode::Active()){if(recordedCamera.load())deferredFov=DeferredFov{savedFov,ownedFov};if(pc->cameraTarget==cameraActor)pc->cameraTarget=savedTarget;return;}if(!recordedCamera.load()){ExitInteractiveCamera(*pc,savedTarget,savedState&&savedState->id==RE::CameraState::kFirstPerson);return;}auto free=pc->GetRuntimeData().cameraStates[RE::CameraState::kFree];if(pc->currentState==free&&pc->cameraTarget==cameraActor){pc->cameraTarget=savedTarget;if(savedState&&savedState->id==RE::CameraState::kFirstPerson)pc->ForceFirstPerson();else if(savedState&&savedState->id==RE::CameraState::kThirdPerson)pc->ForceThirdPerson();else if(savedState)pc->SetState(savedState.get());else pc->ForceThirdPerson();}else if(pc->cameraTarget==cameraActor)pc->cameraTarget=savedTarget;if(auto value=DeferredFov{savedFov,ownedFov}.Value(pc->GetRuntimeData2().worldFOV))pc->GetRuntimeData2().worldFOV=*value;}});
 attempt([&]{if(touchingWorld)if(auto a=cameraActor.get()){a->Disable();a->SetDelete(true);}});
 attempt([&]{if(touchingWorld)if(auto*p=RE::PlayerCharacter::GetSingleton();p&&p->Get3D(false)&&oar)oar->ClearConditionStateData(p);});
 attempt([]{interop::Release();});
 hiddenRoots={};playerHeightScale.Reset();nextMeshHide=0;cameraOwned=false;cameraCalibrated=false;cameraAnchor.reset();cameraActor.reset();savedState.reset();savedTarget.reset();playerGraph.reset();graphManager.reset();stageCell=nullptr;prepared.reset();core.Cleaned();announced=false;wasPaused=false;userPaused=false;cameraReturnPending=false;photoRequestUntil=0;resumeGraceUntil=0;
 BestEffort([&]{SKSE::log::info("Session cleanup acknowledged; touchingWorld={}",touchingWorld);});
}
// SetCollision only sets a reference flag in CommonLib. Harden the actual
// controller as soon as it exists, including asynchronously replaced controllers.
void ProtectCameraActor(RE::Actor* actor){
 // Only this session's helper: never cull the player's or another actor's model.
 // Alpha can be reset while a camera target's biped is refreshed. Hide the
 // scene root and meshes independently, including late/replaced 3D roots.
 bool recull=Now()>=nextMeshHide;for(unsigned i=0;i<2;++i){auto*root=actor->Get3D(i!=0);if(root){bool replaced=hiddenRoots[i].get()!=root;root->SetAppCulled(true);if(replaced||recull){root->CullGeometry(true);root->SetCollisionLayer(RE::COL_LAYER::kNonCollidable);}hiddenRoots[i].reset(root);}else hiddenRoots[i].reset();}if(recull)nextMeshHide=Now()+.5;
 actor->SetCollision(false);if(auto*cc=actor->GetCharController()){cc->flags.set(RE::CHARACTER_FLAGS::kNoSim,RE::CHARACTER_FLAGS::kNoCharacterCollisions,RE::CHARACTER_FLAGS::kNotPushablePermanent);cc->gravity=0;}}
void Publish(double seconds){if(!prepared)return;if(prepared->hasDance&&playerGraph){watchedCharacter=&playerGraph->characterInstance;watchClips=true;playback.store(std::make_shared<Playback>(Playback{core.Generation(),&playerGraph->characterInstance,prepared->song.submod,prepared->song.duration,seconds,sessionNative,core.State()==SessionState::Playing&&pauseReasons.load()==0}));}else{watchClips=false;playback.store(nullptr);}if(!recordedCamera.load()||photoActive.load()){view.store(nullptr);return;}if(!cameraCalibrated)return;auto sample=prepared->camera.At(seconds);auto*activeCamera=RE::PlayerCamera::GetSingleton();if(activeCamera&&activeCamera->currentState==activeCamera->GetRuntimeData().cameraStates[RE::CameraState::kFree]&&activeCamera->cameraTarget==cameraActor){ownedFov=static_cast<float>(sample.fov);activeCamera->GetRuntimeData2().worldFOV=ownedFov;}
 auto actor=cameraActor.get();if(!actor)return;
 // The helper never overlaps the dancer; its physics/position is not a camera origin.
 auto*player=RE::PlayerCharacter::GetSingleton();double scale=playerHeightScale.Read(player,Now());Vec pos=StageCameraPosition(stage,stageRotation,CorrectCameraPath(sample.pos,scale,prepared->camera.heightOffset));Quat direction=Unit(stageRotation*prepared->camera.Direction(sample.rot));Quat rot=Unit(direction*calibration);auto*pc=RE::PlayerCamera::GetSingleton();auto*state=pc->GetRuntimeData().cameraStates[RE::CameraState::kFree].get();view.store(std::make_shared<View>(View{state,pos,rot,stage}));if(cameraAnchor){auto actorRotation=Yaw(-actor->GetAngleZ());cameraAnchor->local.translate=Point(CameraAnchorPosition(pos,Vector(actor->GetPosition()),actorRotation));Quaternion(Unit(actorRotation.Inv()*direction)).ToRotation(cameraAnchor->local.rotate);}}

void AttachPrepared(Prepared&& p){if(!SafePlayer())throw std::runtime_error("Stand still on foot, out of combat, before preparing");auto*player=RE::PlayerCharacter::GetSingleton();auto*pc=RE::PlayerCamera::GetSingleton();auto*controls=RE::PlayerControls::GetSingleton();if(!pc||!controls||controls->blockPlayerInput)throw std::runtime_error("Another scene owns player controls");if(!pc->IsInFirstPerson()&&!pc->IsInThirdPerson())throw std::runtime_error("Camera is already in a special mode");if(auto target=pc->cameraTarget.get();target&&target.get()!=player)throw std::runtime_error("Return camera/control to original player before preparing");float speed=0;if(!player->GetGraphVariableFloat("Speed",speed))throw std::runtime_error("Player movement graph unavailable");if(speed>1||player->AsActorState()->IsWeaponDrawn())throw std::runtime_error("Stop moving and sheathe weapons first");
 if(!InstallRuntimeHooks()||!oar)throw std::runtime_error("OAR API/hooks unavailable");
 const bool withCamera=recordedCamera.load();if(!interop::Acquire(withCamera))throw std::runtime_error("Camera/TDM ownership denied");
 graphPlayable=false;pauseReasons=PauseNoSession|PauseGraph;prepared=std::move(p);audio.Open(std::move(prepared->wave));core.Ready(core.Generation(),ReadyPart::AudioBuffered,true,Now());core.Ready(core.Generation(),ReadyPart::CameraTrack,true,Now());
 if(prepared->hasDance){ if(!player->GetAnimationGraphManager(graphManager)||!graphManager)throw std::runtime_error("Player graph manager missing");{RE::BSSpinLockGuard lock(graphManager->GetRuntimeData().updateLock);for(auto&g:graphManager->graphs)if(g&&g->holder==player&&(std::string_view(g->projectName.c_str())=="DefaultFemale"||std::string_view(g->projectName.c_str())=="DefaultMale")){playerGraph=g;break;}if(!playerGraph)for(auto&g:graphManager->graphs)if(g&&g->holder==player){playerGraph=g;break;}}
 if(!playerGraph)throw std::runtime_error("Player body graph missing");}

 stage=Vector(player->GetPosition());stageRotation=Yaw(-player->GetAngleZ());originalPosition=player->GetPosition();stageCell=player->GetParentCell();stableStage.Reset(stage,Now());
 savedState=pc->currentState;savedTarget=pc->cameraTarget;savedFov=pc->GetRuntimeData2().worldFOV;
 if(withCamera){
 // Same engine API used by CommonLib's PlaceObjectAtMe, but spawn away from
 // the player from the very first instruction (no overlapping placement/move).
 auto parked=Point(CameraParking(stage));auto spawnHandle=RE::TESDataHandler::GetSingleton()->CreateReferenceAtLocation(cameraBase,parked,player->GetAngle(),stageCell,player->GetWorldspace(),nullptr,nullptr,RE::ObjectRefHandle(),false,true);
 auto spawned=spawnHandle.get();auto*actor=spawned?spawned->As<RE::Actor>():nullptr;if(!actor){if(spawned){spawned->Disable();spawned->SetDelete(true);}throw std::runtime_error("Camera actor spawn failed");}cameraActor=actor->GetHandle();if(!cameraActor.get()){actor->Disable();actor->SetDelete(true);throw std::runtime_error("Camera actor handle unavailable");}
 actor->EnableAI(false);actor->SetAlpha(0);ProtectCameraActor(actor);auto*ghost=actor->extraList.GetByType<RE::ExtraGhost>();if(!ghost){ghost=RE::BSExtraData::Create<RE::ExtraGhost>();if(ghost)actor->extraList.Add(ghost);}if(ghost)ghost->ghost=true;
 SKSE::log::info("Camera helper parked away from dancer: stage=({},{},{}), helper=({},{},{}), controllerReady={}",stage.x,stage.y,stage.z,parked.x,parked.y,parked.z,actor->GetCharController()!=nullptr);

 ownedFov=static_cast<float>(prepared->camera.At(0).fov);
 pc->ForceThirdPerson();pc->cameraTarget=cameraActor;cameraOwned=true;pc->SetState(pc->GetRuntimeData().cameraStates[RE::CameraState::kFree].get());pc->GetRuntimeData2().worldFOV=ownedFov;
 }else{cameraOwned=false;cameraCalibrated=true;core.Ready(core.Generation(),ReadyPart::CameraActor3D,true,Now());SKSE::log::info("Free view: existing game camera retained");}
 inputGate=true;controlsGated=true;controls->data.moveInputVec={0,0};controls->data.lookInputVec={0,0};controls->data.autoMove=false;
 selector->value=prepared->hasDance?static_cast<float>(prepared->song.token):0;if(prepared->hasDance){oar->ClearConditionStateData(player);player->NotifyAnimationGraph("IdleForceDefaultState");}
 generated=0;observedEpoch=0;observation.store(nullptr);observationAt=0;preparedAt=Now();nextDiagnostic=preparedAt+5;lastGraphSeen=Now();calibrationTicks=0;if(withCamera)cameraCalibrated=false;retryIdle=false;nextCollisionLog=nextHelperRepark=nextParkWarning=0;maxPlayerStepSquared=0;protectionResets=0;previousPlayerPosition=stage;Publish(0);Notify("Loading dance... wait for READY");
}

void BeginPrepare(){if(!SafePlayer()){Notify("Stand on foot, out of combat before preparing");return;}if(RE::UI::GetSingleton()->GameIsPaused()||loadFuture.valid())return;auto selection=ResolveContent(songs,selectedDanceId,selectedCameraId);if(!selection){Notify("Track selection changed; choose again");return;}auto dance=selection->dance,camera=selection->camera;auto epoch=core.Prepare(Now(),45);if(!epoch)return;loadEpoch=*epoch;recordedCamera=camera.has_value();sessionNative=hotkeys.NativePlayback();nextClipTrace=0;Song master=dance?*dance:*camera;SKSE::log::info("Selection: dance={} camera={} audioMaster={} duration={}",selectedDanceId,selectedCameraId,master.id,master.duration);loadFuture=std::async(std::launch::async,[master=std::move(master),dance=std::move(dance),camera=std::move(camera),volume=cfg.volume]()mutable{return Load(std::move(master),std::move(dance),std::move(camera),volume);});Notify("Preparing selected tracks");}
void PublishSongList(std::string error={}){auto data=std::make_shared<PickerList>();data->generation=catalogGeneration;data->loading=catalogPending;data->error=std::move(error);FillPickerTracks(*data,songs);picker.Publish(std::move(data));}

void Select(){if(core.State()!=SessionState::Idle){Notify("Stop playback before changing songs");return;}if(catalogPending||catalogFuture.valid())return;
 const auto generation=++catalogGeneration;
 auto loading=std::make_shared<PickerList>();loading->generation=generation;loading->loading=true;picker.Publish(std::move(loading));
 try{auto task=std::async(std::launch::async,[generation]{try{auto result=Catalog();auto data=std::make_shared<PickerList>();data->generation=generation;FillPickerTracks(*data,result);picker.Publish(std::move(data));return result;}catch(const std::exception&e){auto data=std::make_shared<PickerList>();data->generation=generation;data->error=e.what();picker.Publish(std::move(data));throw;}});catalogFuture=std::move(task);catalogEpoch=generation;catalogPending=true;}
 catch(const std::exception&e){catalogPending=false;PublishSongList(std::string("목록 작업 시작 실패: ")+e.what());}
}
void OpenPicker(){pendingPick.reset();if(songs.empty()&&!catalogPending)Select();if(!picker.Open(selectedDanceId)){Notify("Track picker unavailable; SKSE Menu Framework is required");}}

void ProcessCommands(){unsigned bits=commands.exchange(0);if(bits&32)hud.Toggle();if(bits&4){picker.Cancel();pendingPick.reset();Stop("Stopped");return;}
 if((bits&64)&&core.State()==SessionState::Playing){ApplyPlaybackPause(false);SKSE::log::info("Dance user pause: {}",userPaused.load());}
 if((bits&128)&&SafeWorld()&&!settings.Open()&&!RE::UI::GetSingleton()->GameIsPaused()&&!PhotoMode::Active()&&Now()>=photoRequestUntil){if(PhotoMode::Open())photoRequestUntil=Now()+3.;else Notify("Photo Mode could not be opened");}
 if(picker.TakeRefresh()&&core.State()==SessionState::Idle)Select();
 if(auto choice=picker.Take())pendingPick=std::move(choice);
 if(pendingPick&&!catalogPending&&!catalogFuture.valid()&&!loadFuture.valid()&&core.State()==SessionState::Idle&&!settings.Open()&&!RE::UI::GetSingleton()->GameIsPaused()){auto choice=std::move(pendingPick);if(choice->generation!=catalogGeneration){Notify("Song list changed; choose again");OpenPicker();return;}auto valid=[&](const std::string&id,bool dance){return id.empty()||std::any_of(songs.begin(),songs.end(),[&](const Song&s){return s.id==id&&(dance?s.HasDance():s.HasCamera());});};if(!valid(choice->id,true)||!valid(choice->cameraId,false)){Notify("Selected track no longer exists");OpenPicker();return;}selectedDanceId=choice->id;selectedCameraId=choice->cameraId;BeginPrepare();return;}
 if(settings.Open())return;
 if(bits&(8|16))core.RequestStart();
 if((bits&2)&&core.State()==SessionState::Idle)OpenPicker();
 if(core.PendingStart()){if(core.State()==SessionState::Armed&&SafeWorld()&&!RE::PlayerCharacter::GetSingleton()->IsDead()&&!RE::UI::GetSingleton()->GameIsPaused()&&!menuSuspended.load()&&core.Start()){backgroundMute.Begin(backgroundCategories);if(auto*manager=RE::BSAudioManager::GetSingleton())manager->ComposeMessage(RE::SOUND_MSG::VolumeChanged);running=true;ApplyPlaybackPause(false);Notify("Playing "+prepared->song.title);}}

}

void Tick(){
 if(catalogPending&&!catalogFuture.valid()){catalogPending=false;PublishSongList("목록 작업이 취소되었습니다.");}
 if(catalogPending&&catalogFuture.wait_for(std::chrono::seconds(0))==std::future_status::ready){catalogPending=false;try{auto result=catalogFuture.get();if(catalogEpoch!=catalogGeneration){songs.clear();Select();return;}songs=std::move(result);PublishSongList();if(songs.empty())Notify("No game-ready songs found");else Notify("Loaded "+std::to_string(songs.size())+" songs. Press prepare to choose");}catch(const std::exception&e){songs.clear();if(catalogEpoch!=catalogGeneration){Select();return;}PublishSongList(std::string("목록 읽기 실패: ")+e.what());Notify(std::string("Song list failed: ")+e.what());}}

 if(loadFuture.valid()&&loadFuture.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{auto p=loadFuture.get();if(core.State()==SessionState::Preparing&&core.Generation()==loadEpoch)AttachPrepared(std::move(p));}catch(const std::exception&e){if(core.State()==SessionState::Preparing)throw;SKSE::log::warn("Discarded load result: {}",e.what());}}
 ProcessCommands();if(core.State()==SessionState::Idle)return;if(core.State()==SessionState::Stopping){Cleanup();return;}if(core.Tick(Now())){Notify("Preparation timed out; see DanceStudio.log");Cleanup();return;}
 if(!prepared)return;if(!SafeWorld()||RE::PlayerCharacter::GetSingleton()->IsDead()){Stop("World/player state changed; stopped");Cleanup();return;}auto*player=RE::PlayerCharacter::GetSingleton();auto*pc=RE::PlayerCamera::GetSingleton();if(!pc)throw std::runtime_error("Player camera unavailable");auto actor=cameraActor.get();
 Vec displacement=Vector(player->GetPosition()-originalPosition);auto*currentCell=player->GetParentCell();const bool withCamera=recordedCamera.load();bool sameState=pc->currentState==pc->GetRuntimeData().cameraStates[RE::CameraState::kFree],sameTarget=pc->cameraTarget==(withCamera?cameraActor:savedTarget);
 auto fault=CheckStage(!withCamera||actor!=nullptr,currentCell==stageCell,displacement);if(fault!=StageFault::None){SKSE::log::warn("Stage guard: {}; displacement=({},{},{}) distance={}; cellSame={} cameraStateSame={} cameraTargetSame={} actorPresent={} stateID={}",StageFaultName(fault),displacement.x,displacement.y,displacement.z,std::sqrt(Dot(displacement,displacement)),currentCell==stageCell,sameState,sameTarget,actor!=nullptr,pc->currentState?static_cast<unsigned>(pc->currentState->id):0xFFFFFFFFu);Stop(StageFaultName(fault),true);Cleanup();return;}
 Vec playerPosition=Vector(player->GetPosition());maxPlayerStepSquared=std::max(maxPlayerStepSquared,Dot(playerPosition-previousPlayerPosition,playerPosition-previousPlayerPosition));previousPlayerPosition=playerPosition;
 stage=stableStage.Update(playerPosition,Now());
 if(withCamera){
  if(auto*cc=actor->GetCharController();cc&&(!cc->flags.any(RE::CHARACTER_FLAGS::kNoSim)||!cc->flags.any(RE::CHARACTER_FLAGS::kNoCharacterCollisions)))++protectionResets;
  ProtectCameraActor(actor.get());
  // The render camera has no collider. Keep the only physical helper away,
  // even if another plugin/late controller creation moved it from parking.
  if(Now()>=nextHelperRepark&&Dot(Vector(actor->GetPosition())-playerPosition,Vector(actor->GetPosition())-playerPosition)<256.*256.){nextHelperRepark=Now()+.25;actor->SetPosition(Point(CameraParking(playerPosition)),true);ProtectCameraActor(actor.get());if(Now()>=nextParkWarning){SKSE::log::warn("Camera helper approached dancer; re-parked without stopping");nextParkWarning=Now()+5;}}
  auto*actorRoot=actor->Get3D(false);if(cameraAnchor&&cameraAnchor->parent!=actorRoot)cameraAnchor.reset();
  if(auto*root=actorRoot;root&&!cameraAnchor){actor->SetAlpha(0);if(auto*n=root->AsNode()){cameraAnchor.reset(RE::NiNode::Create());if(cameraAnchor){cameraAnchor->name="DanceStudioCameraAnchor";n->AttachChild(cameraAnchor.get());}}}
  if(core.State()!=SessionState::Playing)core.Ready(core.Generation(),ReadyPart::CameraActor3D,cameraAnchor!=nullptr,Now());
  if(core.State()==SessionState::Playing&&Now()>=nextCollisionLog){auto*cc=actor->GetCharController();RE::CFilter filter{};if(cc)cc->GetCollisionFilterInfo(filter);SKSE::log::info("Camera physics: helperDistance={} playerMaxStep={} controller={} noSim={} noCharacterCollision={} filterLayer={} filterNoCollision={} protectionResets={} stageOffset={}",std::sqrt(Dot(Vector(actor->GetPosition())-playerPosition,Vector(actor->GetPosition())-playerPosition)),std::sqrt(maxPlayerStepSquared),cc!=nullptr,cc&&cc->flags.any(RE::CHARACTER_FLAGS::kNoSim),cc&&cc->flags.any(RE::CHARACTER_FLAGS::kNoCharacterCollisions),cc?static_cast<int>(filter.GetCollisionLayer()):-1,cc&&filter.QNoCollision(),protectionResets,std::sqrt(Dot(stage-playerPosition,stage-playerPosition)));maxPlayerStepSquared=0;protectionResets=0;SKSE::log::info("Camera path scale: mode=XYZ source={} effective={} reference={} baseHeight={} sceneWorld={} skeletonWorld={} manualOffset={}",playerHeightScale.selected.source,playerHeightScale.selected.value,playerHeightScale.reference,playerHeightScale.base,playerHeightScale.scene,playerHeightScale.skeleton,prepared->camera.heightOffset);nextCollisionLog=Now()+5;}
 }
 core.Ready(core.Generation(),ReadyPart::WorldValid,true,Now());

 bool paused=ExternalPauseReasons()!=0;if(ReclaimPhotoCamera(withCamera,photoActive.load(),cameraReturnPending,recovery.CameraDue(Now(),!sameState||!sameTarget,paused))){
  if(!sameState)pc->ForceThirdPerson();pc->cameraTarget=cameraActor;if(!sameState)pc->SetState(pc->GetRuntimeData().cameraStates[RE::CameraState::kFree].get());ownedFov=static_cast<float>(prepared->camera.At(core.Seconds()).fov);pc->GetRuntimeData2().worldFOV=ownedFov;cameraOwned=true;cameraReturnPending=false;
  if(Now()>=nextPriorityLog){SKSE::log::info("Dance priority: camera reclaimed; stateChanged={} targetChanged={}",!sameState,!sameTarget);nextPriorityLog=Now()+5;}
 }

 if(wasPaused&&!paused)resumeGraceUntil=Now()+1.;wasPaused=paused;if(withCamera&&!photoActive.load()&&!cameraCalibrated&&!paused&&++calibrationTicks>3){if(Calibrate())cameraCalibrated=true;}
 if(prepared->hasDance&&!retryIdle&&Now()-preparedAt>1.0&&generated.load()<3){retryIdle=true;oar->ClearConditionStateData(player);player->NotifyAnimationGraph("IdleForceDefaultState");}
 bool graphReady=cameraCalibrated&&(!prepared->hasDance||(observedEpoch.load()==core.Generation()&&generated.load()>=3&&(paused||Now()-generatedAt.load()<1.0||Now()<resumeGraceUntil)&&Now()-preparedAt>.7));
 if(core.State()!=SessionState::Playing)core.Ready(core.Generation(),ReadyPart::PlayerGraph,graphReady,Now());else {
  if(!graphReady&&!paused){if(!recoveringGraph){recoveringGraph=true;SKSE::log::info("Dance priority: graph interrupted; holding audio and recovering until stop");}if(recovery.GraphDue(Now(),false,paused)){oar->ClearConditionStateData(player);player->NotifyAnimationGraph("IdleForceDefaultState");}}
  else if(graphReady&&recoveringGraph){recoveringGraph=false;SKSE::log::info("Dance priority: graph recovered; resuming held audio clock");}
 }
 if(core.State()==SessionState::Stopping){Cleanup();return;}
 if(core.State()==SessionState::Preparing&&Now()>nextDiagnostic){nextDiagnostic=Now()+5;auto obs=observation.load();SKSE::log::info("Preparing: actor3D={} cameraCalibrated={} generated={} epoch={} observed={} mod='{}' submod='{}' path='{}' bindingDuration={}",cameraAnchor!=nullptr,cameraCalibrated,generated.load(),core.Generation(),observedEpoch.load(),obs?obs->mod:"",obs?obs->submod:"",obs?obs->path:"",obs?obs->duration:0);}
 if(core.State()==SessionState::Armed&&!announced){announced=true;Notify("READY - press play to start");}if(core.State()==SessionState::Armed&&core.PendingStart()){commands.fetch_or(16);ProcessCommands();}
 graphPlayable=graphReady;
 if(core.State()==SessionState::Playing){ApplyPlaybackPause(true);paused=(pauseReasons.load()&~PauseGraph)!=0;if(paused||!graphReady)progressWatch.Reset();if(!paused&&graphReady){auto frames=audio.Frames();if(progressWatch.Stalled(Now(),frames,true,cfg.stallSeconds))throw std::runtime_error("Audio clock stopped advancing; controls restored");core.AudioProgress(core.Generation(),frames,audio.Rate());if(core.State()==SessionState::Stopping){Cleanup();return;}if(core.Seconds()>=prepared->song.duration-1./audio.Rate()||audio.Done()){Stop("Finished");Cleanup();return;}}}
 if(withCamera&&!photoActive.load()&&Now()>=nextFovTrace){nextFovTrace=Now()+5.;if(auto*camera=FindCamera(pc->cameraRoot.get())){auto f=camera->GetRuntimeData2().viewFrustum;SKSE::log::info("FOV projection: world={} left={} right={} top={} bottom={} near={} ortho={}",pc->GetRuntimeData2().worldFOV,f.fLeft,f.fRight,f.fTop,f.fBottom,f.fNear,f.bOrtho);}}
 Publish(core.State()==SessionState::Playing?core.Seconds():0);
}
using UpdateFn=void(*)();UpdateFn originalUpdate{};bool pumpInstalled{};
void SyncPhotoState(){if(deferredFov&&!PhotoMode::Active()&&!photoRequestUntil){auto*pc=RE::PlayerCamera::GetSingleton();if(world.load()&&pc)if(auto value=deferredFov->Value(pc->GetRuntimeData2().worldFOV))pc->GetRuntimeData2().worldFOV=*value;deferredFov.reset();}engineFrozen=PhotoMode::Frozen();bool active=PhotoMode::Active();if(active)photoRequestUntil=0;else if(photoRequestUntil&&Now()>=photoRequestUntil){photoRequestUntil=0;BestEffort([]{Notify("Photo Mode did not open; check its requirements");});}bool previousPhoto=photoActive.load();photoActive=active||photoRequestUntil>Now();if(previousPhoto&&!photoActive.load()&&cameraOwned&&recordedCamera.load())cameraReturnPending=true;if(photoActive.load()){if(auto previous=view.exchange(nullptr)){auto*pc=RE::PlayerCamera::GetSingleton();if(pc&&pc->currentState.get()==previous->state){auto*free=static_cast<RE::FreeCameraState*>(previous->state);free->translation=Point(previous->position);}}}ApplyPlaybackPause(false);}
void MainUpdate(){BestEffort([]{SyncPhotoState();});originalUpdate();BestEffort([]{SyncPhotoState();});DWORD current=GetCurrentThreadId(),unset=0;mainThread.compare_exchange_strong(unset,current);if(current!=mainThread.load())return;if(!world&&core.State()==SessionState::Idle&&!loadFuture.valid())return;try{Tick();inputState=core.State();}catch(const std::exception&e){Stop(nullptr,true);Cleanup();inputState=core.State();BestEffort([&]{Notify(std::string("Stopped: ")+e.what());});}catch(...){Stop(nullptr,true);Cleanup();inputState=core.State();}}
bool InstallPump(){if(pumpInstalled)return true;REL::Relocation<std::uintptr_t> update{REL::ID(35565)};auto site=update.address()+0x748;if(*reinterpret_cast<unsigned char*>(site)!=0xE8)return false;std::int32_t displacement;std::memcpy(&displacement,reinterpret_cast<void*>(site+1),4);auto old=site+5+displacement;if(!Executable(old))return false;originalUpdate=reinterpret_cast<UpdateFn>(old);SKSE::AllocTrampoline(64);SKSE::GetTrampoline().write_call<5>(site,MainUpdate);pumpInstalled=true;return true;}
using DispatchFn=void(*)(RE::BSTEventSource<RE::InputEvent*>*,RE::InputEvent**);
DispatchFn originalDispatch{};
void DispatchInput(RE::BSTEventSource<RE::InputEvent*>*source,RE::InputEvent**events){
 static SpaceGate space;auto state=inputState.load();bool active=state!=SessionState::Idle;
 if(!events||(!active&&!space.Owned())){originalDispatch(source,events);return;}
 std::vector<std::pair<RE::InputEvent*,RE::InputEvent*>> links;RE::InputEvent*head=nullptr,*tail=nullptr;
 for(auto*e=*events;e;){auto*next=e->next;links.emplace_back(e,next);bool consume=false;
  if(auto*b=e->AsButtonEvent();b&&b->GetDevice()==RE::INPUT_DEVICE::kKeyboard&&b->GetIDCode()==57){auto decision=space.Event(state,b->IsDown(),b->IsUp());consume=decision.consume;if(decision.toggle)ToggleUserPauseNow();}
  if(!consume){if(tail)tail->next=e;else head=e;tail=e;}e=next;
 }
 if(tail)tail->next=nullptr;
 struct Restore{std::vector<std::pair<RE::InputEvent*,RE::InputEvent*>>&links;~Restore(){for(auto [e,next]:links)e->next=next;}}restore{links};originalDispatch(source,&head);
}
bool InstallInputDispatch(){if(originalDispatch)return true;REL::Relocation<std::uintptr_t> target{REL::ID(67315)};auto site=target.address()+0x7B;if(*reinterpret_cast<unsigned char*>(site)!=0xE8)return false;std::int32_t displacement;std::memcpy(&displacement,reinterpret_cast<void*>(site+1),4);auto old=site+5+displacement;if(!Executable(old))return false;originalDispatch=reinterpret_cast<DispatchFn>(old);SKSE::GetTrampoline().write_call<5>(site,DispatchInput);return true;}
struct Input:RE::BSTEventSink<RE::InputEvent*> {RE::BSEventNotifyControl ProcessEvent(RE::InputEvent*const* list,RE::BSTEventSource<RE::InputEvent*>*)override{if(!list||!world||settings.Open())return RE::BSEventNotifyControl::kContinue;auto*cm=RE::ControlMap::GetSingleton();auto*ui=RE::UI::GetSingleton();if(!cm||!ui||cm->GetRuntimeData().textEntryCount>0||ui->IsMenuOpen(RE::Console::MENU_NAME))return RE::BSEventNotifyControl::kContinue;const auto keys=hotkeys.Current();for(auto*e=*list;e;e=e->next)if(auto*b=e->AsButtonEvent();b&&b->GetDevice()==RE::INPUT_DEVICE::kKeyboard&&b->IsDown()){auto k=b->GetIDCode();if(keys.photo&&k==keys.photo&&PhotoMode::available.load())commands.fetch_or(128);else if(keys.hud&&k==keys.hud)commands.fetch_or(32);else if(keys.stop&&k==keys.stop)commands.fetch_or(4);else {if(keys.start&&k==keys.start){commands.fetch_or(PlayCommandFor(inputState.load()));}}}return RE::BSEventNotifyControl::kContinue;}}input;
struct Menus:RE::BSTEventSink<RE::MenuOpenCloseEvent> {
 std::mutex mutex;std::vector<RE::BSFixedString> pausing;
 RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent*e,RE::BSTEventSource<RE::MenuOpenCloseEvent>*)override{
  if(!e)return RE::BSEventNotifyControl::kContinue;
  std::scoped_lock lock(mutex);
  if(e->opening){auto*ui=RE::UI::GetSingleton();auto menu=ui?ui->GetMenu(e->menuName):nullptr;if(menu&&menu->PausesGame()&&std::find(pausing.begin(),pausing.end(),e->menuName)==pausing.end())pausing.push_back(e->menuName);if(!pausing.empty()&&running.load())try{menuSuspended=true;pauseReasons.fetch_or(PauseMenu);audio.Pause(true);}catch(...){commands.fetch_or(4);}}
  else std::erase(pausing,e->menuName);
  menuSuspended=!pausing.empty();return RE::BSEventNotifyControl::kContinue;
 }
 void Reset(){std::scoped_lock lock(mutex);pausing.clear();menuSuspended=false;}
}menus;

void WorldReset(bool valid){deferredFov.reset();photoRequestUntil=0;userPaused=false;photoActive=false;hud.Restore();backgroundMute.Restore();picker.Cancel();pendingPick.reset();++catalogGeneration;commands=0;Stop();Cleanup(valid);if(!valid){std::scoped_lock lock(clipsLock);clips.clear();clipCount=0;}if(selector)selector->value=0;world=false;inputState=SessionState::Idle;menus.Reset();}
void ResolveBackgroundCategories(){
 backgroundCategories.clear();auto path=std::filesystem::path(L"Data\\SKSE\\Plugins\\DanceStudio\\BackgroundAudio.json");
 auto fallback=[](){backgroundCategories.clear();auto*defaults=RE::BGSDefaultObjectManager::GetSingleton();if(defaults)if(auto*music=defaults->GetObject<RE::BGSSoundCategory>(RE::BGSDefaultObjectManager::DefaultObject::kMusicSoundCategory))backgroundCategories.push_back(music);};
 try{if(std::filesystem::exists(path)){auto j=ReadJson(path,16384);if(j.at("schemaVersion")!=1||!j.at("categories").is_array()||j.at("categories").size()>64)throw std::runtime_error("Invalid background audio configuration");for(auto&row:j.at("categories")){auto plugin=row.at("plugin").get<std::string>(),text=row.at("formId").get<std::string>();size_t used=0;auto id=std::stoul(text,&used,16);if(used!=text.size()||id>0xFFFFFF||plugin.empty())throw std::runtime_error("Invalid audio category identity");auto*category=RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSSoundCategory>(static_cast<RE::FormID>(id),plugin);if(!category)throw std::runtime_error("Background audio category missing: "+plugin+":"+text);backgroundCategories.push_back(category);}}
 else{fallback();SKSE::log::warn("BackgroundAudio.json missing: only default music category available");}
 }catch(const std::exception&e){fallback();SKSE::log::error("Background audio configuration rejected; music-only fallback: {}",e.what());}

 SKSE::log::info("Background audio configuration: {} categories",backgroundCategories.size());
}
void RegisterMenus(const char*stage){
 std::string pickerError,settingsError;bool pickerOK=false,settingsOK=false;
 BestEffort([&]{pickerOK=picker.Register(pickerError);});
 BestEffort([&]{settingsOK=settings.Register(settingsError);});
 SKSE::log::info("Menu registration [{}]: settings={} picker={} settingsError='{}' pickerError='{}'",stage,settingsOK,pickerOK,settingsError,pickerError);
}
void Message(SKSE::MessagingInterface::Message*m){if(!m)return;try{switch(m->type){case SKSE::MessagingInterface::kPostLoad:RegisterMenus("PostLoad");
 if(!BestEffort([]{interop::RegisterCamera();}))SKSE::log::error("SmoothCam listener initialization failed; menu registration retained");
 if(!BestEffort([]{interop::InitTDM();}))SKSE::log::error("TDM initialization failed; menu registration retained");break;
 case SKSE::MessagingInterface::kPostPostLoad:RegisterMenus("PostPostLoad");interop::RequestCamera();break;
 case SKSE::MessagingInterface::kInputLoaded:RegisterMenus("InputLoaded");break;
 case SKSE::MessagingInterface::kDataLoaded:{RegisterMenus("DataLoaded");ResolveBackgroundCategories();PhotoMode::Resolve();auto*data=RE::TESDataHandler::GetSingleton();selector=data->LookupForm<RE::TESGlobal>(0x800,Forms);cameraBase=data->LookupForm<RE::TESNPC>(0x801,Forms);auto mod=GetModuleHandleW(L"OpenAnimationReplacer.dll");auto request=reinterpret_cast<OAR_API::Animations::_RequestPluginAPI_Animations>(GetProcAddress(mod,"RequestPluginAPI_Animations"));if(request)oar=request(OAR_API::Animations::InterfaceVersion::V1,"DanceStudio",REL::Version{0,4,8});if(!selector||!cameraBase||!oar){SKSE::log::error("Missing data ESL or OAR Animations API; plugin inactive");return;}selector->value=0;if(!InstallPump())throw std::runtime_error("Main update hook incompatible");if(!InstallInputDispatch())throw std::runtime_error("Space dispatch hook incompatible; LDA:R inactive");SKSE::log::info("Space dispatch filter installed; idle passthrough enabled");RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&input);RE::UI::GetSingleton()->AddEventSink(&menus);available=true;SKSE::log::info("Runtime armed for world load; configurable keys in SKSE Menu > LDA:R > 단축키");break;}
 case SKSE::MessagingInterface::kPreLoadGame:WorldReset(true);break;
 case SKSE::MessagingInterface::kSaveGame:Stop("Playback ended before saving");Cleanup(true);break;
 case SKSE::MessagingInterface::kPostLoadGame:WorldReset(false);world=m->data&&available;if(world)Select();break;
 case SKSE::MessagingInterface::kNewGame:WorldReset(false);world=available;if(world)Select();break;
 default:break;}}catch(const std::exception&e){SKSE::log::error("Lifecycle failure: {}",e.what());Stop(nullptr,true);try{Cleanup(world.load());}catch(...){SKSE::log::error("Cleanup failed during lifecycle exception");}if(selector)selector->value=0;world=false;watchClips=false;playback.store(nullptr);view.store(nullptr);inputGate=false;audio.Close();inputState=SessionState::Idle;}}
}
extern "C" __declspec(dllexport) bool SKSEPlugin_Query(const SKSE::QueryInterface*s,SKSE::PluginInfo*i){if(!s||!i)return false;i->infoVersion=SKSE::PluginInfo::kVersion;i->name="DanceStudio";i->version=0x414;return !s->IsEditor()&&s->RuntimeVersion()==REL::Version(1,5,97,0);}
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface*s){try{SKSE::Init(s);auto path=SKSE::log::log_directory();if(!path)return false;*path/="DanceStudio.log";auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(),true);auto logger=std::make_shared<spdlog::logger>("DanceStudio",sink);spdlog::set_default_logger(logger);spdlog::flush_on(spdlog::level::info);DS::ReadConfig();if(!DS::hud.Register())SKSE::log::warn("HUD filter registration failed");
 static std::jthread focusGuard([](std::stop_token stop){while(!stop.stop_requested()){if(DS::running.load()&&!DS::Focused())DanceStudio::BestEffort([]{DS::pauseReasons.fetch_or(DanceStudio::PauseFocus);DS::audio.Pause(true);});std::this_thread::sleep_for(std::chrono::milliseconds(DS::cfg.focusPollMs));}});
 SKSE::log::info("LDA:R 0.4.14 TEST; Skyrim SE 1.5.97 only; no Papyrus; original LDA untouched");return SKSE::GetMessagingInterface()->RegisterListener(DS::Message);}catch(...){return false;}}
