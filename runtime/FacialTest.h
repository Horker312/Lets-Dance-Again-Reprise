#pragma once
#include "FacialLedger.h"
#include "MfgFrames.h"
#include "FacialRuntimePolicy.h"
#include "Localization.h"
// Official resource uses the generic Win32 macro with a wide literal.
// Scope that compatibility macro; keep the upstream header byte-for-byte.
#pragma push_macro("GetModuleHandle")
#undef GetModuleHandle
#define GetModuleHandle GetModuleHandleW
#include "vendor/MFEE/Interface.h"
#pragma pop_macro("GetModuleHandle")
#include <deque>
#include <chrono>
#include <mutex>
#include <sstream>
#include <nlohmann/json.hpp>
#include <fstream>

namespace DanceStudio::Facial {
class Test {
 enum class Action {Enable,Disable,Read,Apply,End,EndExtended};
 struct Request {Action action;Key key;float value{};};
 struct Identity {
  std::vector<std::uintptr_t> parts;
  bool operator==(const Identity&)const=default;
 };
 std::chrono::steady_clock::time_point restoreDue{};
 Ledger ledger;RE::ActorHandle target;Identity identity;RE::FormID cell{};RE::NiPoint3 position{};
 MFEE::InterfaceManager manager;MFEE::IFacialExpressionExtended* api{};HMODULE module{};bool apiTried{};
 std::mutex mutex;std::deque<Request> requests;std::atomic<bool> needed{};bool enabled{},draftEnabled{};
 std::string result="NOT TESTED";std::vector<std::string> morphs,morphLabels;int selectedMorph{},strength{1};
 static inline Test* instance{};
 using Text=void(*)(const char*,const char*);using Button=bool(*)(const char*);using Check=bool(*)(const char*,bool*);using Combo=bool(*)(const char*,int*,const char*const[],int,int);
 Text text{};Button button{};Check check{};Combo combo{};bool registered{};
 static std::string Values(const Value&v){std::ostringstream s;s<<'[';for(auto x:v)s<<x<<',';return s.str()+']';}
 void Log(const char*status,const Key&k,const Value&before={},const Value&last={},const Value&current={}){
  auto line=std::string(status)+" backend="+(k.kind==Kind::Extended?"MFEE":"MFG-engine")+" kind="+std::to_string(static_cast<int>(k.kind))+" morph="+(k.name.empty()?std::to_string(k.id):k.name)+" before="+Values(before)+" lastWritten="+Values(last)+" observed="+Values(current);
  SKSE::log::info("FacialTest {}",line);std::scoped_lock l(mutex);result=std::move(line);
 }
 static Identity Identify(RE::Actor*a){
  Identity i;if(!a||!a->Get3D(false)||!a->GetFaceGenAnimationData())return i;auto*n=a->GetActorBase();if(!n)return i;
  auto add=[&](auto*p){i.parts.push_back(reinterpret_cast<std::uintptr_t>(p));};
  add(a->Get3D(false));auto*f=a->GetFaceGenAnimationData();add(f);{RE::BSSpinLockGuard lock(f->lock);for(auto*frame:{&f->expressionKeyFrame,&f->modifierKeyFrame,&f->phenomeKeyFrame}){add(frame->values);i.parts.push_back(frame->count);}}add(a->GetRace());add(n);add(n->faceNPC);
  auto head=[&](RE::TESNPC*base){if(!base)return true;auto count=static_cast<int>(base->numHeadParts);if(count<0||count>64||(count&&!base->headParts))return false;i.parts.push_back(count);for(int j=0;j<count;++j)add(base->headParts[j]);return true;};
  if(!head(n)||!head(n->faceNPC))i.parts.clear();return i;
 }
 RE::Actor* Resolve(){auto a=target.get();return a&&Identify(a.get())==identity&&!identity.parts.empty()?a.get():nullptr;}
 bool Bind(){
  if(!ledger.Empty())return Resolve()!=nullptr;auto*a=RE::PlayerCharacter::GetSingleton();auto next=Identify(a);if(next.parts.empty())return false;
  target=a->GetHandle();identity=std::move(next);cell=a->GetParentCell()?a->GetParentCell()->GetFormID():0;position=a->GetPosition();
  SKSE::log::info("FacialTest actor={:08X} race={:08X} face identity captured",a->GetFormID(),a->GetRace()?a->GetRace()->GetFormID():0);return true;
 }
 bool API(){
  auto current=GetModuleHandleW(L"MuFacialExpressionExtended.dll");if(apiTried)return api&&current&&current==module;
  apiTried=true;module=current;if(!current){Log("UNSUPPORTED MFEE DLL absent",{Kind::Extended});return false;}
  api=manager.GetInterface();if(!api){Log("UNSUPPORTED MFEE interface exchange failed",{Kind::Extended});return false;}
  auto version=api->GetVersion();if(version!=7){api=nullptr;Log(("UNSUPPORTED MFEE version "+std::to_string(version)).c_str(),{Kind::Extended});return false;}
  Log("PASS MFEE API version 7 detected; actor/visual NOT TESTED",{Kind::Extended});return true;
 }
 struct MorphValues:MFEE::IFacialExpressionExtended::IContainer {
  std::string wanted;std::optional<int> value;
  void Container(const char*,const char*name,std::int32_t v)override{if(name&&MfeeName(wanted)==MfeeName(name))value=v;}
 };
 struct CycleValues:MFEE::IFacialExpressionExtended::IContainer {std::unordered_map<std::string,int> values;void Container(const char*,const char*n,std::int32_t v)override{if(!n||values.size()>=4096)return;auto [it,ok]=values.emplace(MfeeName(n),v);if(!ok)it->second=-1;}};
 MorphTargetBatch cycle;bool cycling{};
 std::unordered_map<std::string,int> FetchCycle(){CycleValues result;if(API())if(auto*a=Resolve())api->GetAllMorph(a,result,true);return std::move(result.values);}
 static bool FrameValid(RE::BSFaceGenKeyframeMultiple&f,unsigned count){return f.values&&f.count==count;}
 std::optional<Value> Read(const Key&k){
  auto*a=Resolve();if(!a)return std::nullopt;
  if(k.kind==Kind::Extended){if(!API()||!api->IsValidExpressionMorph(k.name.c_str()))return std::nullopt;
   int value=api->GetExpressionValue(a,k.name.c_str());MorphValues destination;destination.wanted=k.name;if(cycling){cycle.Ensure([&]{return FetchCycle();});auto it=cycle.values.find(MfeeName(k.name));if(it!=cycle.values.end())destination.value=it->second;}else api->GetAllMorph(a,destination,true);
   if(!destination.value||value<0||value>100||*destination.value<0||*destination.value>100)return std::nullopt;
   return Value{static_cast<float>(value),static_cast<float>(*destination.value)};
  }
  auto*f=a->GetFaceGenAnimationData();if(!f)return std::nullopt;RE::BSSpinLockGuard lock(f->lock);
  if(k.kind==Kind::Expression)return MfgFrames::Expression(f->expressionKeyFrame,f->exprOverride);
  auto&frame=k.kind==Kind::Modifier?f->modifierKeyFrame:f->phenomeKeyFrame;return MfgFrames::Single(frame,k.kind,k.id);


 }
 bool Write(const Key&k,const Value&v){
  auto*a=Resolve();if(!a)return false;
  if(k.kind==Kind::Extended){if(v.size()!=2||!Equal(Value{v[0]},Value{v[1]})||!API()||!api->IsValidExpressionMorph(k.name.c_str()))return false;
   api->SetExpression(a,k.name.c_str(),static_cast<int>(v[0]),0);if(cycling){cycle.Written(MfeeName(k.name),static_cast<int>(v[0]));}return true;
  }
  auto*f=a->GetFaceGenAnimationData();if(!f)return false;RE::BSSpinLockGuard lock(f->lock);
  if(k.kind==Kind::Expression)return MfgFrames::Expression(f->expressionKeyFrame,f->exprOverride,v);
  auto&frame=k.kind==Kind::Modifier?f->modifierKeyFrame:f->phenomeKeyFrame;return MfgFrames::Single(frame,k.kind,k.id,v);


 }
 void EndKind(std::optional<Kind>kind={},bool deferPending=true){
  if(ledger.Empty()){target={};identity={};cell=0;position={};return;}if(!Resolve()){Discard("PARTIAL identity invalid; no stale face writes");return;}
  ledger.End(kind,[&](auto&k){return Read(k);},[&](auto&k,auto&v){return Write(k,v);},[&](auto s,const auto&k,const auto&b,const auto&l,const auto&v){Log(s,k,b,l,v);},nullptr,deferPending);
  if(ledger.Empty()){target={};identity={};cell=0;position={};}
 }
 void Discard(const char*why){ledger.Discard();target={};identity={};cell=0;position={};Log(why,{});}
 void Snapshot(){
  if(!Bind()){Log("UNSUPPORTED actor/face unavailable",{});return;}
  struct Names:MFEE::IFacialExpressionExtended::IContainer {std::vector<std::string> names;std::unordered_map<std::string,std::string> categories;void Container(const char*c,const char*n,std::int32_t)override{if(n&&*n&&names.size()<4096){names.emplace_back(n);categories[n]=c?c:"";}}}names;
  if(API())api->GetAllExpressionList(names);std::sort(names.names.begin(),names.names.end());names.names.erase(std::unique(names.names.begin(),names.names.end()),names.names.end());
  struct Batch:MFEE::IFacialExpressionExtended::IContainer {std::unordered_map<std::string,int> values;void Container(const char*,const char*n,std::int32_t v)override{if(!n||values.size()>=4096)return;auto [it,added]=values.emplace(MfeeName(n),v);if(!added)it->second=-1;}}batch;
  if(API())api->GetAllMorph(Resolve(),batch,true);
  for(size_t j=0;j<std::min<size_t>(names.names.size(),128);++j){Key k{Kind::Extended,0,names.names[j]};std::optional<Value> v;auto it=batch.values.find(MfeeName(k.name));if(it!=batch.values.end()&&it->second>=0&&it->second<=100){auto current=api->GetExpressionValue(Resolve(),k.name.c_str());if(current>=0&&current<=100)v=Value{static_cast<float>(current),static_cast<float>(it->second)};}Log(v?"PASS MFEE numeric read; mesh support NOT TESTED":"UNSUPPORTED MFEE read",k,{}, {},v.value_or(Value{}));}
  // User-triggered snapshot only. This records API registration/access, never TRI visual support.
  if(auto folder=SKSE::log::log_directory()){nlohmann::json entries=nlohmann::json::array();for(auto&n:names.names){auto it=batch.values.find(MfeeName(n));bool readable=it!=batch.values.end()&&it->second>=0&&it->second<=100;entries.push_back({{"id",MfeeName(n)},{"name",n},{"category",names.categories[n]},{"apiRegistered",true},{"actorReadable",readable}});}nlohmann::json catalog={{"schemaVersion",1},{"apiVersion",7},{"actorFormId",Resolve()?Resolve()->GetFormID():0},{"raceFormId",Resolve()&&Resolve()->GetRace()?Resolve()->GetRace()->GetFormID():0},{"visualVerified",false},{"entries",entries}};auto path=*folder/"DanceStudio.MFEE.catalog.json";std::ofstream out(path);out<<catalog.dump(2);if(!out.good())SKSE::log::warn("MFEE registry write failed: {}",path.string());else SKSE::log::info("MFEE registry saved: {}. TRI/visual support is NOT TESTED",path.string());}
  {std::scoped_lock l(mutex);morphLabels.clear();for(auto&n:names.names)morphLabels.push_back(names.categories[n]+" / "+n);morphs=std::move(names.names);selectedMorph=0;}
 }
 void Apply(const Request&r){
  if(!Bind()){Log("UNSUPPORTED actor/face unavailable",r.key);return;}
  Value value;
  if(r.key.kind!=Kind::Extended)return;
  auto before=Read(r.key);if(!before||before->size()!=2||(*before)[0]!=(*before)[1]){Log("UNSUPPORTED MFEE unavailable or transition active; apply skipped",r.key);return;}value={r.value,r.value};auto key=r.key;
  ledger.Apply(key,value,[&](auto&k){return Read(k);},[&](auto&k,auto&v){return Write(k,v);},[&](auto s,const auto&k,const auto&b,const auto&l,const auto&v){Log(s,k,b,l,v);});
 }
 void Submit(Request r){if(requests.size()<32){requests.push_back(std::move(r));needed=true;}}
 static void __stdcall Draw()noexcept{if(instance)instance->Render();}
 void Render()noexcept{try{
  std::scoped_lock l(mutex);text(L10n::T("r0169"),nullptr);
  if(check(L10n::T("r0170"),&draftEnabled))Submit({draftEnabled?Action::Enable:Action::Disable});
  if(!draftEnabled){text(result.c_str(),nullptr);return;}
  text(L10n::T("r0171"),nullptr);if(button(L10n::T("r0172")))Submit({Action::Read});
  const char*values[]={"10%","25%","50%"};combo(L10n::T("r0173"),&strength,values,3,3);float value=strength==0?10.f:strength==2?50.f:25.f;
  std::vector<const char*>names;for(auto&n:morphLabels)names.push_back(n.c_str());if(!names.empty()){combo("MFEE morph",&selectedMorph,names.data(),static_cast<int>(names.size()),10);if(button(L10n::T("r0177"))&&selectedMorph>=0&&selectedMorph<static_cast<int>(morphs.size()))Submit({Action::Apply,{Kind::Extended,0,morphs[selectedMorph]},value});}
  if(button(L10n::T("r0178")))Submit({Action::EndExtended});if(button(L10n::T("r0179")))Submit({Action::End});text(result.c_str(),nullptr);
 }catch(...){}}
public:
 bool PendingRelease()const{return ledger.PendingRelease();}
 bool PollRestore(bool world,bool suspended=false)noexcept{if(!ledger.PendingRelease())return true;try{
  auto*ui=RE::UI::GetSingleton();if(!world||!ui||ui->IsMenuOpen(RE::MainMenu::MENU_NAME)||ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)||!Resolve()){Close(false,"deferred restore world/face invalid");return false;}
  if(suspended)return true;
  static constexpr auto interval=std::chrono::milliseconds(33);auto now=std::chrono::steady_clock::now();if(now<restoreDue)return true;restoreDue=now+interval;
  BeginCycle();try{ledger.RetryReleases([&](auto&k){return Read(k);},[&](auto&k,auto&v){return Write(k,v);},[&](auto s,const auto&k,const auto&b,const auto&l,const auto&v){Log(s,k,b,l,v);});EndCycle();}catch(...){cycling=false;cycle.Reset();throw;}
  if(ledger.Empty()){target={};identity={};cell=0;position={};}
 return true;}catch(...){Close(false,"deferred restore exception");return false;}}
 bool PerformanceValid(){return ledger.Empty()||Resolve()!=nullptr;}
 enum class ApplyResult {Applied,Pending,Yield};
 void BeginCycle(){cycling=true;cycle.Reset();}
 void EndCycle(){try{cycle.Finish([&]{return FetchCycle();},[&]{ledger.ObserveExtended([&](auto&k){return Read(k);});});}catch(...){cycling=false;throw;}cycling=false;}
 ApplyResult PerformanceApply(Key key,int value){
  if(!Bind())return ApplyResult::Yield;Value desired;
  if(value<0||value>100)return ApplyResult::Yield;
  if(key.kind==Kind::Expression){if(key.id>=17)return ApplyResult::Yield;desired.assign(18,0);desired[key.id]=value/100.f;desired[17]=1;key.id=0;}
  else if(key.kind!=Kind::Extended){if((key.kind==Kind::Modifier&&key.id>=14)||(key.kind==Kind::Phoneme&&key.id>=16))return ApplyResult::Yield;desired={value/100.f};}
  else {auto current=Read(key);if(!current||current->size()!=2||(*current)[0]!=(*current)[1]){return current&&ledger.Status(key,[&](auto&k){return Read(k);})==Observation::Pending?ApplyResult::Pending:ApplyResult::Yield;}desired={static_cast<float>(value),static_cast<float>(value)};}
  bool applied=ledger.Apply(key,desired,[&](auto&k){return Read(k);},[&](auto&k,auto&v){return Write(k,v);},[&](auto status,const auto&k,const auto&b,const auto&l,const auto&v){if(std::string_view(status).starts_with("PASS"))return;Log(status,k,b,l,v);});
  return applied?ApplyResult::Applied:ledger.Status(key,[&](auto&k){return Read(k);})==Observation::Pending?ApplyResult::Pending:ApplyResult::Yield;
 }
 void PerformanceRelease(Key key){if(key.kind==Kind::Expression)key.id=0;if(!Resolve()){Discard("PARTIAL invalid facial identity");return;}ledger.End(std::nullopt,[&](auto&k){return Read(k);},[&](auto&k,auto&v){return Write(k,v);},[&](auto status,const auto&k,const auto&b,const auto&l,const auto&v){Log(status,k,b,l,v);},&key,true);}
 void Register(){if(registered)return;auto mod=GetModuleHandleW(L"SKSEMenuFramework.dll");if(!mod)return;
  auto get=[&](auto&out,const char*n){out=reinterpret_cast<std::remove_reference_t<decltype(out)>>(GetProcAddress(mod,n));};
  unsigned(*version)(){};void(*add)(const char*,void(__stdcall*)()){};get(version,"GetMenuFrameworkAPIVersion");get(add,"AddSectionItem");get(text,"igTextUnformatted");get(button,"igSmallButton");get(check,"igCheckbox");get(combo,"igCombo_Str_arr");
  if(!version||version()!=1||!add||!text||!button||!check||!combo)return;instance=this;add("LDA:R/Facial Test",Draw);registered=true;
 }
 // Lifecycle callers execute on engine thread. No destructor calls into engine.
 void Close(bool restore,const char*reason,bool deferPending=true)noexcept{try{
  {std::scoped_lock l(mutex);requests.clear();}if(!ledger.Empty()){auto*ui=RE::UI::GetSingleton();if(!ui||ui->IsMenuOpen(RE::MainMenu::MENU_NAME)||ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME))restore=false;SKSE::log::info("FacialTest ending reason={}",reason);if(restore)EndKind({},deferPending);else Discard("PARTIAL lifecycle invalidation; restore skipped");}
  if(ledger.Empty()){target={};identity={};cell=0;position={};}enabled=false;{std::scoped_lock l(mutex);draftEnabled=false;needed=!requests.empty();}
 }catch(...){ledger.Discard();target={};identity={};cell=0;position={};enabled=false;needed=false;try{SKSE::log::error("FacialTest FAIL cleanup exception; dance unaffected");}catch(...){}}}
 void Pump(bool world,bool allowActions=true)noexcept{if(!needed.load())return;try{
  if(!allowActions){Close(world,"facial diagnostic excluded during performance");return;}
  auto*ui=RE::UI::GetSingleton();bool validWorld=world&&ui&&!ui->IsMenuOpen(RE::MainMenu::MENU_NAME)&&!ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
  if(!ledger.Empty()){
   if(!validWorld){Close(false,"menu/loading");return;}auto*a=Resolve();if(!a){Close(false,"actor/race/headpart/3D invalidated");return;}
   auto p=a->GetPosition();auto d=p-position;auto id=a->GetParentCell()?a->GetParentCell()->GetFormID():0;
   if(id!=cell||d.SqrLength()>1024.f*1024.f||a->IsDead()){Close(true,"cell/teleport/death");return;}position=p;
  }
  std::deque<Request> batch;{std::scoped_lock l(mutex);batch.swap(requests);}
  for(auto&r:batch){if(r.action==Action::Enable){enabled=true;Log("NOT TESTED enabled; player-only diagnostic",{});continue;}if(r.action==Action::Disable){EndKind();enabled=false;continue;}if(!enabled)continue;
   if(r.action==Action::End){EndKind();continue;}if(r.action==Action::EndExtended){EndKind(Kind::Extended);continue;}
   if(!validWorld){Log("NOT TESTED no valid loaded world",r.key);continue;}if(r.action==Action::Read)Snapshot();else Apply(r);
  }
  {std::scoped_lock l(mutex);needed=enabled||!requests.empty();}
 }catch(const std::exception&e){try{SKSE::log::error("FacialTest FAIL exception {}; isolated from dance",e.what());}catch(...){}Close(true,"exception");}catch(...){Close(false,"unknown exception");}}
};
}
