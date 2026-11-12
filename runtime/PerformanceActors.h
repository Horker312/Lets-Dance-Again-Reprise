#pragma once
#include "TrackData.h"
#include "PerformanceAlias.h"
#include <array>
#include <optional>
#include <chrono>
namespace DanceStudio {
// Placement is deliberately independent of the music/session clock. Future
// stage-relative arrangements can use RelativePlacement without changing slots.
struct StageAnchor {Vec position;Quat rotation;};
struct RelativePlacement {Vec offset;Quat rotation;};
struct CameraAnchorSelection {enum class Kind{Actor,Stage};Kind kind{Kind::Actor};unsigned slot{};};
inline constexpr unsigned MaxPerformers=5;
inline bool FinitePlacement(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})<1.e8;}
struct ActorPlacement {RE::FormID cell{};Vec position;RE::NiPoint3 angle;std::optional<RelativePlacement> relative;RE::FormID world{};};
struct ActorSlot {
 RE::ActorHandle actor;std::string danceId;unsigned danceToken{};
 std::optional<ActorPlacement> savedPlacement; // confirmed, session-only stage configuration
 std::optional<ActorPlacement> placement,original;bool manualPlacement{};RE::TESPackage* originalPackage{};RE::TESPackage* performancePackage{};
 bool placementValid{},controlled{},membership{},collisionByUs{},originalCollision{};
 unsigned participant{};bool moveIssued{};std::uint64_t lease{};bool released{},needsEvaluate{},positionAllowed{},yielded{};double missingSince{-1};bool returning{};bool moving{};double moveDeadline{};bool lifeCaptured{},originalAI{true},enabledAI{},sceneDetached{};RE::FormID detachedScene{};bool recovering{};double recoverySince{-1},aliasReservedAt{};double nextControl{};unsigned controlFailures{},sceneAttempts{};
 RE::NiPointer<RE::NiAVObject> collisionRoot;
 void ClearPlacement(){moveIssued=false;lease=0;recovering=false;recoverySince=-1;aliasReservedAt=0;released=needsEvaluate=positionAllowed=yielded=false;missingSince=-1;returning=false;moving=false;moveDeadline=0;lifeCaptured=enabledAI=sceneDetached=false;detachedScene=0;nextControl=0;controlFailures=sceneAttempts=0;placementValid=false;placement.reset();original.reset();originalPackage=nullptr;performancePackage=nullptr;controlled=false;collisionByUs=false;collisionRoot.reset();}
};
class PerformanceActors {
public:
 // Verified against Skyrim.esm: NazeemREF ACHR 0001A6A4 -> NPC_ 00013BBF.
 // Resolve the canonical placed reference through the data handler, never NPC names/base IDs.
 inline static RE::FormID excludedReference{};
 static bool Excluded(RE::Actor*a){return a&&excludedReference&&a->GetFormID()==excludedReference;}
 bool Restoring(RE::Actor*a)const{if(!a)return false;for(auto&q:deferred)if(q.slot.actor.get().get()==a&&(!q.slot.released||q.slot.needsEvaluate||q.slot.enabledAI||q.slot.collisionByUs||q.slot.membership))return true;return false;}
 enum class StartIssue{None,NoActors,InvalidActor,Loading,Recovering,Control,WrongCell,NeedsPlacement,Restoring,Excluded,Camera};
 enum class ReleaseResult{Restored,Pending,Yielded,Abandoned};
 struct Deferred {ActorSlot slot;unsigned observations{};};std::vector<Deferred> deferred;
 size_t Pending()const{return deferred.size();}
 unsigned releaseFailures{},abandoned{},controlReturns{},positionReturns{},positionSuperseded{},externalYields{};
 enum class ForceIssue{None,Unsafe,RestorePending,Movement,Moving,Loading,Recovering,Control};ForceIssue lastForceIssue{};unsigned forcedInvalidations{};
 inline static double controlClock{};
 static double ControlTime(){return controlClock;}
 static void AdvanceControlTime(double frameSeconds,bool paused){if(!paused&&std::isfinite(frameSeconds)&&frameSeconds>0)controlClock+=std::min(frameSeconds,.25);}
 static bool RenderReady(RE::Actor*a){if(!a||!a->GetParentCell()||!a->Get3D(false)||!a->GetCharController())return false;RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;return a->GetAnimationGraphManager(manager)&&manager&&!manager->graphs.empty();}
 bool restageActive{},lastRestaged{};unsigned restageTouched{},lastRestageSlot{MaxPerformers};
 std::array<ActorSlot,MaxPerformers> slots;CameraAnchorSelection camera;StageAnchor stage;
 static bool Registrable(RE::Actor*a){return a&&a->GetParentCell()&&!a->IsDeleted()&&!a->IsDisabled()&&!a->IsDead();}
 static bool SameSpace(RE::TESObjectCELL*a,RE::TESObjectCELL*b){return a&&b&&(a==b||(a->GetRuntimeData().worldSpace&&a->GetRuntimeData().worldSpace==b->GetRuntimeData().worldSpace));}
 static RE::TESObjectCELL* Cell(RE::FormID id){return RE::TESForm::LookupByID<RE::TESObjectCELL>(id);}
 static bool Usable(RE::Actor*a,bool allowCombat=false){return a&&a->GetParentCell()&&a->Get3D(false)&&!a->IsDeleted()&&!a->IsDisabled()&&!a->IsDead()&&!a->IsInKillMove()&&!a->IsOnMount()&&(a!=RE::PlayerCharacter::GetSingleton()||!a->IsInCombat()||allowCombat)&&!a->AsActorState()->IsSwimming()&&!a->IsInMidair()&&a->AsActorState()->GetSitSleepState()==RE::SIT_SLEEP_STATE::kNormal&&a->AsActorState()->GetKnockState()==RE::KNOCK_STATE_ENUM::kNormal;}
 static bool ExternalControl(RE::Actor*a){return !a||a->GetCurrentScene()||a->GetActorRuntimeData().boolFlags.any(RE::Actor::BOOL_FLAGS::kScenePackage,RE::Actor::BOOL_FLAGS::kIsCommandedActor);}
 unsigned Count()const{unsigned n=0;for(auto&s:slots)if(s.actor)n++;return n;}
 bool HasPlayer()const{auto*p=RE::PlayerCharacter::GetSingleton();for(auto&s:slots)if(auto a=s.actor.get();a&&a.get()==p)return true;return false;}
 RE::NiPointer<RE::Actor> CameraActor()const{return camera.slot<MaxPerformers?slots[camera.slot].actor.get():RE::NiPointer<RE::Actor>{};}
 bool FixCamera(){if(CameraActor())return false;for(unsigned i=0;i<MaxPerformers;++i)if(slots[i].actor.get()){camera.slot=i;return true;}camera.slot=0;return true;}
 bool Duplicate(RE::Actor*a,unsigned except)const{for(unsigned i=0;i<MaxPerformers;++i)if(i!=except)if(auto v=slots[i].actor.get();v&&v.get()==a)return true;return false;}
 static ActorPlacement Capture(RE::Actor*a){if(!a||!a->GetParentCell())throw std::runtime_error("Actor has no capture cell");return{a->GetParentCell()->GetFormID(),{a->GetPositionX(),a->GetPositionY(),a->GetPositionZ()},a->GetAngle(),{},a->GetWorldspace()?a->GetWorldspace()->GetFormID():0};}
 static bool SamePlace(RE::Actor*a,const ActorPlacement&p,double tolerance){if(!a||!a->GetParentCell())return false;auto v=a->GetPosition();return a->GetParentCell()&&SameSpace(a->GetParentCell(),Cell(p.cell))&&Dot(Vec{v.x,v.y,v.z}-p.position,Vec{v.x,v.y,v.z}-p.position)<=tolerance*tolerance;}
 // Exact CommonLibSSE-NG TESObjectREFR.cpp MoveTo_Impl ABI and Address Library IDs.
 // This public adapter retains the SDK's cell/world destination and never caches 3D pointers.
 static void Move(RE::Actor*a,const ActorPlacement&p){auto*cell=Cell(p.cell);auto*world=p.world?RE::TESForm::LookupByID<RE::TESWorldSpace>(p.world):nullptr;if(!a||!cell||(p.world&&!world))throw std::runtime_error("Destination cell/world unavailable");if(a->GetParentCell()!=cell){
 using Fn=void(*)(RE::TESObjectREFR*,const RE::ObjectRefHandle&,RE::TESObjectCELL*,RE::TESWorldSpace*,const RE::NiPoint3&,const RE::NiPoint3&);
 static REL::Relocation<Fn> move{RELOCATION_ID(56227,56626)};RE::ObjectRefHandle target{};RE::NiPoint3 position{float(p.position.x),float(p.position.y),float(p.position.z)};move(a,target,cell,world,position,p.angle);
 }else{a->SetPosition({float(p.position.x),float(p.position.y),float(p.position.z)},true);a->SetAngle(p.angle);}}
 static bool MoveConfirmed(RE::Actor*a,const ActorPlacement&p){Move(a,p);return a->GetParentCell()==Cell(p.cell)&&SamePlace(a,p,2)&&std::isfinite(a->GetAngleZ())&&std::abs(std::remainder(double(a->GetAngleZ()-p.angle.z),6.283185307179586))<=.02;}
 // Actor-local engine APIs only: do not stop a whole scene or rewrite its quest state.
 static bool Hold(ActorSlot&s,RE::Actor*a,bool staging=true,double now=-1){
  if(!Registrable(a)||a==RE::PlayerCharacter::GetSingleton()||a->IsInKillMove()||a->IsOnMount())return false;
  if(!s.lifeCaptured){s.originalAI=a->IsAIEnabled();s.lifeCaptured=true;}
  if(!a->IsAIEnabled()){a->EnableAI(true);s.enabledAI=!s.originalAI;if(!a->IsAIEnabled())return false;}
  if(auto*scene=a->GetCurrentScene()){if(!s.sceneDetached)s.detachedScene=scene->GetFormID();s.sceneDetached=true;a->SetCurrentScene(nullptr);if(a->GetCurrentScene())return false;}
  if(a->IsInCombat())a->StopCombat();
  auto result=PerformanceAlias::Acquire(a,s.lease,s.participant);
  if(result!=AliasResult::Ready){s.recovering=result==AliasResult::Pending;if(s.recovering&&s.recoverySince<0)s.recoverySince=ControlTime();return false;}
  s.controlled=PerformanceAlias::Owned(s.lease,a);s.recovering=!s.controlled;if(s.recovering)return false;s.recoverySince=-1;
  PerformanceAlias::Maintain(s.lease,a);if(staging&&RenderReady(a))a->StopMoving(1.f);return true;
 }
 static void Collision(ActorSlot&s,RE::Actor*a,bool acquire){
  auto*root=a->Get3D(false);auto*cc=a->GetCharController();if(!root||!cc)return;
  if(acquire&&!s.collisionByUs){s.originalCollision=cc->flags.any(RE::CHARACTER_FLAGS::kNoCharacterCollisions);if(!s.originalCollision){cc->flags.set(RE::CHARACTER_FLAGS::kNoCharacterCollisions);s.collisionRoot.reset(root);s.collisionByUs=true;}}
  else if(!acquire&&s.collisionByUs){if(s.collisionRoot.get()==root&&cc->flags.any(RE::CHARACTER_FLAGS::kNoCharacterCollisions))cc->flags.reset(RE::CHARACTER_FLAGS::kNoCharacterCollisions);s.collisionByUs=false;s.collisionRoot.reset();}
 }
 ReleaseResult ReleaseSlot(ActorSlot&s,bool world,RE::TESFaction*faction){
  if(!world){s.membership=false;s.ClearPlacement();return ReleaseResult::Abandoned;}
  auto a=s.actor.get();if(!a||a->IsDeleted()){if(s.lease&&PerformanceAlias::ReleaseLost(s.lease)!=AliasResult::Ready)return ReleaseResult::Pending;s.ClearPlacement();return ReleaseResult::Abandoned;}
  if(s.membership){if(faction&&a->IsInFaction(faction))a->RemoveFromFaction(faction);s.membership=false;}
  if(!s.released){
   bool own=s.lease&&PerformanceAlias::Owned(s.lease,a.get());
   if(!s.positionAllowed)s.positionAllowed=(own||s.moving)&&!ExternalControl(a.get())&&!a->IsDead()&&s.original&&s.placement&&SamePlace(a.get(),*s.placement,256);
   if(s.lease){auto result=PerformanceAlias::Release(s.lease,a.get());if(result!=AliasResult::Ready)return ReleaseResult::Pending;s.needsEvaluate=true;}
   s.controlled=false;s.performancePackage=nullptr;s.released=true;++controlReturns;if(s.yielded)++externalYields;
  }
  if(s.enabledAI){if(a->IsAIEnabled()&&!s.yielded&&(!a->GetCurrentScene()||(s.sceneDetached&&a->GetCurrentScene()->GetFormID()==s.detachedScene)))a->EnableAI(false);s.enabledAI=false;}
  if(s.needsEvaluate){if(!a->IsDead()&&a->IsAIEnabled())a->EvaluatePackage();s.needsEvaluate=false;}
  if(s.collisionByUs){auto*root=a->Get3D(false);if(!root)return ReleaseResult::Pending;if(root!=s.collisionRoot.get()){s.collisionByUs=false;s.collisionRoot.reset();}else{if(!a->GetCharController())return ReleaseResult::Pending;Collision(s,a.get(),false);}}
  // Only position restoration can remain pending below this point.
  if(s.positionAllowed&&s.original){
   if(ExternalControl(a.get())||a->IsDead()||a->IsDisabled()){s.positionAllowed=false;s.yielded=true;}
   else{if(a->GetParentCell()!=Cell(s.original->cell)||!SamePlace(a.get(),*s.original,2)){
     if(!s.returning){s.returning=true;Move(a.get(),*s.original);}if(a->GetParentCell()!=Cell(s.original->cell)||!SamePlace(a.get(),*s.original,2))return ReleaseResult::Pending;
    }++positionReturns;}
  }
  bool yielded=s.yielded;s.ClearPlacement();return yielded?ReleaseResult::Yielded:ReleaseResult::Restored;
 }
 ReleaseResult Release(unsigned i,bool world,RE::TESFaction*faction){auto&s=slots.at(i);auto saved=s.savedPlacement;auto held=s.actor.get();bool player=held&&held.get()==RE::PlayerCharacter::GetSingleton();s.placementValid=false;
  ReleaseResult result=ReleaseResult::Restored;
  if(!s.lease&&!s.controlled&&!s.enabledAI&&!s.membership&&!s.collisionByUs&&!s.original)s.ClearPlacement();else{result=ReleaseSlot(s,world,faction);if(result==ReleaseResult::Pending){if(deferred.size()>=16)throw std::runtime_error("NPC restoration queue full; existing slot retained");deferred.push_back({s});s.membership=false;s.ClearPlacement();}}
  // Coordinates may be useful later; only the player's start marker remains usable.
  if(world&&saved){s.savedPlacement=saved;s.placement=saved;s.placementValid=player;}if(!world)s.savedPlacement.reset();return result;
 }
 void RetryReleases(bool world,RE::TESFaction*faction){for(auto it=deferred.begin();it!=deferred.end();){if(it->observations>=60){++it;continue;}try{bool newer=false;for(auto&live:slots)if(live.controlled&&live.actor.get().get()==it->slot.actor.get().get()&&live.lease!=it->slot.lease)newer=true;if(newer&&it->slot.released){++positionSuperseded;it=deferred.erase(it);continue;}auto result=ReleaseSlot(it->slot,world,faction);if(result==ReleaseResult::Pending&&++it->observations<60){++it;continue;}if(result==ReleaseResult::Pending){++abandoned;if(!it->slot.released||it->slot.needsEvaluate||it->slot.enabledAI||it->slot.collisionByUs||it->slot.membership){++it;continue;}}if(result==ReleaseResult::Abandoned)++abandoned;it=deferred.erase(it);}catch(const std::exception&){++releaseFailures;if(++it->observations>=60)++abandoned;++it;}}}
 void Reset(bool world,RE::TESFaction*faction,bool clear){restageActive=false;restageTouched=0;lastRestaged=false;for(unsigned i=0;i<MaxPerformers;++i){try{Release(i,world,faction);if(clear)slots[i]={};}catch(const std::exception&){++releaseFailures;if(world&&deferred.size()<16){try{deferred.push_back({slots[i]});slots[i].membership=false;slots[i].ClearPlacement();if(clear)slots[i]={};}catch(const std::bad_alloc&){/* Keep the slot lease when queue allocation fails. */}}}}if(!world){deferred.clear();}if(clear)camera={};}
 void Defaults(){if(!Count())if(auto*p=RE::PlayerCharacter::GetSingleton())slots[0].actor=p->GetHandle();}
 bool AssignPlayer(unsigned i){auto*p=RE::PlayerCharacter::GetSingleton();if(i>=MaxPerformers||slots[i].actor.get()||!Usable(p)||Duplicate(p,i))return false;slots[i]={};slots[i].actor=p->GetHandle();FixCamera();return true;}
 bool Assign(unsigned i,RE::Actor*a,RE::TESFaction*faction){lastForceIssue=ForceIssue::None;if(i>=MaxPerformers||Excluded(a)||!Registrable(a)||a==RE::PlayerCharacter::GetSingleton()||Duplicate(a,i))return false;if(!ResolveDeferred(a,faction))return false;Release(i,true,faction);if(!ResolveDeferred(a,faction))return false;slots[i]={};slots[i].actor=a->GetHandle();FixCamera();return true;}
 // Complete one old release before taking a fresh lease. Both run on the engine thread.
 bool ResolveDeferred(RE::Actor*a,RE::TESFaction*faction){for(auto it=deferred.begin();it!=deferred.end();){if(it->slot.actor.get().get()!=a){++it;continue;}try{auto result=ReleaseSlot(it->slot,true,faction);if(result==ReleaseResult::Pending){auto&old=it->slot;
    if(old.released&&!old.needsEvaluate&&!old.enabledAI&&!old.membership&&!old.collisionByUs){
     // Explicitly supersede only the old location job. Preserve its baseline
     // for the new lease; there is no asynchronous movement callback.
     bool inherited=false;for(auto&live:slots)if(live.actor.get().get()==a){if(!live.original)live.original=old.original;inherited=true;break;}
     if(inherited){++positionSuperseded;it=deferred.erase(it);continue;}
    }lastForceIssue=ForceIssue::RestorePending;return false;}
   if(result==ReleaseResult::Abandoned){++abandoned;lastForceIssue=ForceIssue::RestorePending;return false;}it=deferred.erase(it);
  }catch(const std::exception&){++releaseFailures;lastForceIssue=ForceIssue::RestorePending;return false;}}return true;}
 bool Place(unsigned i,RE::TESFaction*faction=nullptr){auto*p=RE::PlayerCharacter::GetSingleton();if(i>=MaxPerformers||!Usable(p))return false;if(!slots[i].moving)PerformanceAlias::UserRetry();return PlaceAt(i,Capture(p),faction,true);}
 bool PlaceAt(unsigned i,const ActorPlacement&destination,RE::TESFaction*faction,bool manual){lastForceIssue=ForceIssue::None;if(i>=MaxPerformers)return false;auto&s=slots[i];s.participant=i;auto a=s.actor.get();auto*p=RE::PlayerCharacter::GetSingleton();if(Excluded(a.get())||!Usable(p)||!Registrable(a.get())||!p->GetCharController()||p->AsActorState()->IsWeaponDrawn()||p->IsInWater()){if(s.moving){if(!Registrable(a.get())){Release(i,true,faction);lastForceIssue=ForceIssue::Unsafe;}else lastForceIssue=ForceIssue::Moving;}return false;}
  if(s.moving){
   if(!s.moveIssued){if(!Hold(s,a.get())){if(s.recovering&&ControlTime()<=s.moveDeadline){lastForceIssue=ForceIssue::Moving;return false;}Release(i,true,faction);lastForceIssue=ForceIssue::Control;return false;}try{Move(a.get(),*s.placement);s.moveIssued=true;}catch(...){Release(i,true,faction);lastForceIssue=ForceIssue::Movement;return false;}lastForceIssue=ForceIssue::Moving;return false;}
   bool arrived=s.placement&&a->GetParentCell()==Cell(s.placement->cell)&&RenderReady(a.get())&&Usable(a.get(),true);
   if(!arrived){if(ControlTime()>s.moveDeadline){lastForceIssue=ForceIssue::Movement;Release(i,true,faction);}else lastForceIssue=ForceIssue::Moving;return false;}
   if(!Hold(s,a.get())){if(ControlTime()>s.moveDeadline){Release(i,true,faction);lastForceIssue=ForceIssue::Control;}else lastForceIssue=ForceIssue::Moving;return false;}
   if(!MoveConfirmed(a.get(),*s.placement)){lastForceIssue=ForceIssue::Movement;Release(i,true,faction);return false;}Collision(s,a.get(),true);s.moving=false;s.placementValid=true;if(s.manualPlacement)s.savedPlacement=s.placement;return true;}

  if(!s.placementValid&&(s.released||s.needsEvaluate||s.returning)){Release(i,true,faction);if(!ResolveDeferred(a.get(),faction))return false;}
  auto place=destination;if(!Cell(place.cell)||!SameSpace(p->GetParentCell(),Cell(place.cell))){lastForceIssue=ForceIssue::Unsafe;return false;}s.manualPlacement=manual;if(!FinitePlacement(place.position)||!std::isfinite(place.angle.z))return false;if(manual)place.angle.x=place.angle.y=0;
  // The live, grounded player's capsule validates the destination. Avoid
  // occupied placed slots rather than silently changing the chosen position.
  for(unsigned n=0;n<MaxPerformers;++n)if(n!=i&&(slots[n].placementValid||slots[n].moving))if(auto other=slots[n].actor.get();other&&other.get()!=p&&SamePlace(other.get(),place,48))return false;
  if(a.get()!=p){if(!ResolveDeferred(a.get(),faction))return false;if(!s.original){auto original=Capture(a.get());if(!FinitePlacement(original.position)||!std::isfinite(original.angle.x)||!std::isfinite(original.angle.y)||!std::isfinite(original.angle.z)){lastForceIssue=ForceIssue::Unsafe;return false;}s.original=original;s.originalPackage=a->GetCurrentPackage();}if(!s.lifeCaptured){s.originalAI=a->IsAIEnabled();s.lifeCaptured=true;}if(!a->IsAIEnabled()){a->EnableAI(true);s.enabledAI=!s.originalAI;}s.placement=place;if(!Hold(s,a.get())){if(s.recovering){s.moving=true;s.moveIssued=false;s.moveDeadline=ControlTime()+5;lastForceIssue=ForceIssue::Moving;return false;}lastForceIssue=ForceIssue::Control;Release(i,true,faction);return false;}
   if(a->GetParentCell()!=Cell(place.cell)||!RenderReady(a.get())||!Usable(a.get(),true)){s.moving=true;s.moveDeadline=ControlTime()+5;try{Move(a.get(),place);s.moveIssued=true;}catch(...){Release(i,true,faction);lastForceIssue=ForceIssue::Movement;return false;}lastForceIssue=ForceIssue::Moving;return false;}
   Collision(s,a.get(),true);s.placement=place;if(!MoveConfirmed(a.get(),place)){lastForceIssue=ForceIssue::Movement;Release(i,true,faction);return false;}}
  s.placement=place;s.placementValid=true;if(manual)s.savedPlacement=place;s.controlFailures=0;s.nextControl=0;return true;
 }
 enum class RestageResult{Ready,Pending,Failed};
 void CancelRestage(RE::TESFaction*faction,bool world=true){auto touched=restageTouched;restageTouched=0;restageActive=false;lastRestaged=false;for(unsigned i=0;i<MaxPerformers;++i)if(touched&(1u<<i)){try{Release(i,world,faction);}catch(const std::exception&){++releaseFailures;if(world&&deferred.size()<16){try{deferred.push_back({slots[i]});slots[i].ClearPlacement();slots[i].placement=slots[i].savedPlacement;}catch(const std::bad_alloc&){/* Keep the live recovery record if allocation fails. */}}}}}
 RestageResult RestageSaved(RE::TESFaction*faction=nullptr){
  lastForceIssue=ForceIssue::None;auto*p=RE::PlayerCharacter::GetSingleton();if(!p||!p->GetParentCell()||!Count()){lastForceIssue=ForceIssue::Unsafe;return RestageResult::Failed;}
  if(p->IsDead()){lastForceIssue=ForceIssue::Unsafe;return RestageResult::Failed;}if(!Usable(p)){lastForceIssue=ForceIssue::Loading;return RestageResult::Pending;}
  if(!restageActive){PerformanceAlias::UserRetry();restageActive=true;restageTouched=0;lastRestaged=false;lastRestageSlot=MaxPerformers;}
  // Check the entire configuration before acquiring a new participant lease.
  for(auto&s:slots)if(s.actor){lastRestageSlot=static_cast<unsigned>(&s-slots.data());auto a=s.actor.get();if(!Registrable(a.get())||Excluded(a.get())){lastForceIssue=ForceIssue::Unsafe;CancelRestage(faction);return RestageResult::Failed;}if(a.get()!=p&&!s.savedPlacement&&!s.placementValid){lastForceIssue=ForceIssue::Unsafe;CancelRestage(faction);return RestageResult::Failed;}
   if(s.savedPlacement&&(!Cell(s.savedPlacement->cell)||!SameSpace(p->GetParentCell(),Cell(s.savedPlacement->cell)))){lastForceIssue=ForceIssue::Unsafe;CancelRestage(faction);return RestageResult::Failed;}}
  for(unsigned i=0;i<MaxPerformers;++i){auto&s=slots[i];if(!s.actor)continue;lastRestageSlot=i;auto a=s.actor.get();if(a.get()==p)continue;
   if(s.placementValid&&!s.moving){if(!RenderReady(a.get())||s.recovering)return RestageResult::Pending;continue;}
   if(!s.savedPlacement){lastForceIssue=ForceIssue::Unsafe;CancelRestage(faction);return RestageResult::Failed;}
   restageTouched|=1u<<i;auto destination=*s.savedPlacement;
   if(!PlaceAt(i,destination,faction,false)){if(lastForceIssue==ForceIssue::Moving||lastForceIssue==ForceIssue::Recovering||lastForceIssue==ForceIssue::Loading)return RestageResult::Pending;auto failed=lastForceIssue;CancelRestage(faction);lastForceIssue=failed;return RestageResult::Failed;}
  }
  lastRestaged=restageTouched!=0;restageTouched=0;restageActive=false;return RestageResult::Ready;
 }
 // Maintain actor control during the whole session; pin position only before playback.
 // Ordinary AI drift is repaired, not treated as lost ownership. Five failed observations
 // (normally 0.2 s apart) retire the lease through the existing release queue.
 void HoldPlaced(bool staging,double now=-1,RE::TESFaction*faction=nullptr,bool immediate=false){if(now<0)now=ControlTime();auto*p=RE::PlayerCharacter::GetSingleton();for(unsigned i=0;i<MaxPerformers;++i){auto&s=slots[i];if(s.moving||!s.placementValid||!s.placement)continue;auto a=s.actor.get();if(a&&a.get()==p)continue;
  if(!a||a->IsDeleted()||a->IsDead()||a->IsDisabled()){++forcedInvalidations;lastForceIssue=ForceIssue::Unsafe;Release(i,true,faction);continue;}
  if(!p||!p->GetParentCell())continue;
  if(!RenderReady(a.get())){if(s.missingSince<0)s.missingSince=now;if(now-s.missingSince>=10){++forcedInvalidations;lastForceIssue=ForceIssue::Loading;Release(i,true,faction);}continue;}
  s.missingSince=-1;if(!SameSpace(p->GetParentCell(),Cell(s.placement->cell))||!SameSpace(a->GetParentCell(),p->GetParentCell()))continue;
  if(!immediate&&now<s.nextControl)continue;s.nextControl=now+.2;
  bool success=false,controlAcquired=false;try{success=controlAcquired=Hold(s,a.get(),staging,now);if(!success&&s.recovering){if(s.recoverySince<0)s.recoverySince=now;if(now-s.recoverySince<10)continue;s.recovering=false;}if(success&&staging){if(!SamePlace(a.get(),*s.placement,1))success=MoveConfirmed(a.get(),*s.placement);else if(std::abs(std::remainder(double(a->GetAngleZ()-s.placement->angle.z),6.283185307179586))>.02)a->SetAngle(s.placement->angle);}}catch(const std::exception&){success=false;}
  if(success)s.controlFailures=0;else if(++s.controlFailures>=5){s.placementValid=false;++forcedInvalidations;lastForceIssue=controlAcquired?ForceIssue::Movement:ForceIssue::Control;Release(i,true,faction);}
 }}
 // Always called while idle, including during world placement interaction.
 bool RefreshPlacements(bool world,RE::TESFaction*faction){bool changed=false;auto*p=RE::PlayerCharacter::GetSingleton();for(unsigned i=0;i<MaxPerformers;++i){auto&s=slots[i];auto a=s.actor.get();if(s.moving||(a&&a.get()==p))continue;if(!s.placementValid&&!s.controlled&&!s.enabledAI&&!s.collisionByUs&&!s.membership&&!s.original)continue;
  bool invalid=!world||!a||a->IsDead()||a->IsDeleted()||a->IsDisabled()||!p||p->IsDead()||!p->GetParentCell()||!s.placement||!SameSpace(p->GetParentCell(),Cell(s.placement->cell))||(a->GetParentCell()&&!SameSpace(p->GetParentCell(),a->GetParentCell()));
  if(invalid){s.placementValid=false;Release(i,world,faction);changed=true;}
 }return changed;}
 StartIssue ValidateStart(RE::TESObjectCELL*cell,bool allowPlayerCombat=false)const{if(!Count())return StartIssue::NoActors;auto*p=RE::PlayerCharacter::GetSingleton();if(!cell||!p)return StartIssue::InvalidActor;
  for(unsigned i=0;i<MaxPerformers;++i)if(slots[i].actor){auto&s=slots[i];auto a=s.actor.get();if(Excluded(a.get()))return StartIssue::Excluded;if(Restoring(a.get()))return StartIssue::Restoring;switch(StateOf(i,cell,true)){case ParticipantState::InvalidActor:return StartIssue::InvalidActor;case ParticipantState::Fatal:return StartIssue::WrongCell;case ParticipantState::Loading:return StartIssue::Loading;case ParticipantState::RecoveringControl:return StartIssue::Recovering;case ParticipantState::NeedsPlacement:return StartIssue::NeedsPlacement;default:break;}if(!a||a->IsDeleted()||a->IsDead()||a->IsDisabled())return StartIssue::InvalidActor;if(s.moving||!RenderReady(a.get()))return StartIssue::Loading;if(s.recovering)return StartIssue::Recovering;if(!Usable(a.get(),a.get()==p&&allowPlayerCombat))return StartIssue::Control;if(!SameSpace(a->GetParentCell(),cell))return StartIssue::WrongCell;
   if(s.placement&&(!FinitePlacement(s.placement->position)||!std::isfinite(s.placement->angle.x)||!std::isfinite(s.placement->angle.y)||!std::isfinite(s.placement->angle.z)||!SameSpace(Cell(s.placement->cell),cell)))return StartIssue::WrongCell;
   if(a.get()!=p&&(!s.placement||!s.placementValid||!s.controlled||!a->IsAIEnabled()||!SamePlace(a.get(),*s.placement,2)))return StartIssue::NeedsPlacement;
  }auto anchor=CameraActor();if(!anchor||!SameSpace(anchor->GetParentCell(),cell))return StartIssue::Camera;return StartIssue::None;
 }
 enum class ParticipantState{Ready,Loading,RecoveringControl,NeedsPlacement,InvalidActor,Fatal};
 ParticipantState StateOf(unsigned i,RE::TESObjectCELL*cell,bool staging=true)const{if(i>=MaxPerformers)return ParticipantState::InvalidActor;auto&s=slots[i];auto a=s.actor.get();if(!a||a->IsDead()||a->IsDeleted()||a->IsDisabled())return ParticipantState::InvalidActor;if(s.moving||!RenderReady(a.get()))return ParticipantState::Loading;if(!SameSpace(a->GetParentCell(),cell))return ParticipantState::Fatal;if(a.get()!=RE::PlayerCharacter::GetSingleton()){if(!s.placementValid||!s.placement)return ParticipantState::NeedsPlacement;if(s.recovering||!s.controlled||!PerformanceAlias::Owned(s.lease,a.get()))return ParticipantState::RecoveringControl;if(staging&&!SamePlace(a.get(),*s.placement,2))return ParticipantState::RecoveringControl;}return ParticipantState::Ready;}
 bool MovePlayerToStart(RE::TESObjectCELL*cell){auto*p=RE::PlayerCharacter::GetSingleton();for(auto&s:slots)if(auto a=s.actor.get();a&&a.get()==p&&s.placement){if(!cell||!p->GetParentCell()||p->GetParentCell()!=cell||!SameSpace(Cell(s.placement->cell),cell))return false;if(!MoveConfirmed(p,*s.placement))return false;}return true;}
 void Separation(){auto*p=RE::PlayerCharacter::GetSingleton();if(!p)return;for(auto&s:slots)if(s.collisionByUs)if(auto a=s.actor.get()){auto delta=a->GetPosition()-p->GetPosition();if(delta.SqrLength()>96*96)Collision(s,a.get(),false);}}
};
}
