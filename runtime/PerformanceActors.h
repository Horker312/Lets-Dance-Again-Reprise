#pragma once
#include "TrackData.h"
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
struct ActorPlacement {RE::FormID cell{};Vec position;RE::NiPoint3 angle;std::optional<RelativePlacement> relative;};
struct ActorSlot {
 RE::ActorHandle actor;std::string danceId;unsigned danceToken{};
 std::optional<ActorPlacement> placement,original;RE::TESPackage* originalPackage{};
 bool placementValid{},restrained{},membership{},collisionByUs{},originalCollision{};
 bool lifeCaptured{},originalAI{true},enabledAI{},sceneDetached{};RE::ACTOR_LIFE_STATE originalLife{RE::ACTOR_LIFE_STATE::kAlive};RE::FormID detachedScene{};double nextControl{};unsigned controlFailures{};
 RE::NiPointer<RE::NiAVObject> collisionRoot;
 void ClearPlacement(){lifeCaptured=enabledAI=sceneDetached=false;detachedScene=0;nextControl=0;controlFailures=0;placementValid=false;placement.reset();original.reset();originalPackage=nullptr;restrained=false;collisionByUs=false;collisionRoot.reset();}
};
class PerformanceActors {
public:
 // Verified against Skyrim.esm: NazeemREF ACHR 0001A6A4 -> NPC_ 00013BBF.
 // Resolve the canonical placed reference through the data handler, never NPC names/base IDs.
 inline static RE::FormID excludedReference{};
 static bool Excluded(RE::Actor*a){return a&&excludedReference&&a->GetFormID()==excludedReference;}
 bool Restoring(RE::Actor*a)const{if(!a)return false;for(auto&q:deferred)if(q.slot.actor.get().get()==a)return true;return false;}
 enum class StartIssue{None,NoActors,InvalidActor,WrongCell,NeedsPlacement,Restoring,Excluded,Camera};
 enum class ReleaseResult{Restored,Pending,Yielded,Abandoned};
 struct Deferred {ActorSlot slot;unsigned observations{};};std::vector<Deferred> deferred;
 size_t Pending()const{return deferred.size();}
 unsigned releaseFailures{},abandoned{};
 enum class ForceIssue{None,Unsafe,RestorePending,Movement};ForceIssue lastForceIssue{};unsigned forcedInvalidations{};
 static double ControlTime(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
 std::array<ActorSlot,MaxPerformers> slots;CameraAnchorSelection camera;StageAnchor stage;
 static bool Usable(RE::Actor*a,bool allowCombat=false){return a&&a->GetParentCell()&&a->Get3D(false)&&!a->IsDeleted()&&!a->IsDisabled()&&!a->IsDead()&&!a->IsInKillMove()&&!a->IsOnMount()&&(a!=RE::PlayerCharacter::GetSingleton()||!a->IsInCombat()||allowCombat)&&!a->AsActorState()->IsSwimming()&&!a->IsInMidair()&&a->AsActorState()->GetSitSleepState()==RE::SIT_SLEEP_STATE::kNormal&&a->AsActorState()->GetKnockState()==RE::KNOCK_STATE_ENUM::kNormal;}
 static bool ExternalControl(RE::Actor*a){return !a||a->GetCurrentScene()||a->GetActorRuntimeData().boolFlags.any(RE::Actor::BOOL_FLAGS::kScenePackage,RE::Actor::BOOL_FLAGS::kIsCommandedActor);}
 unsigned Count()const{unsigned n=0;for(auto&s:slots)if(s.actor)n++;return n;}
 bool HasPlayer()const{auto*p=RE::PlayerCharacter::GetSingleton();for(auto&s:slots)if(auto a=s.actor.get();a&&a.get()==p)return true;return false;}
 RE::NiPointer<RE::Actor> CameraActor()const{return camera.slot<MaxPerformers?slots[camera.slot].actor.get():RE::NiPointer<RE::Actor>{};}
 bool FixCamera(){if(CameraActor())return false;for(unsigned i=0;i<MaxPerformers;++i)if(slots[i].actor.get()){camera.slot=i;return true;}camera.slot=0;return true;}
 bool Duplicate(RE::Actor*a,unsigned except)const{for(unsigned i=0;i<MaxPerformers;++i)if(i!=except)if(auto v=slots[i].actor.get();v&&v.get()==a)return true;return false;}
 static ActorPlacement Capture(RE::Actor*a){if(!a||!a->GetParentCell())throw std::runtime_error("Actor has no capture cell");return{a->GetParentCell()->GetFormID(),{a->GetPositionX(),a->GetPositionY(),a->GetPositionZ()},a->GetAngle()};}
 static bool SamePlace(RE::Actor*a,const ActorPlacement&p,double tolerance){if(!a||!a->GetParentCell())return false;auto v=a->GetPosition();return a->GetParentCell()&&a->GetParentCell()->GetFormID()==p.cell&&Dot(Vec{v.x,v.y,v.z}-p.position,Vec{v.x,v.y,v.z}-p.position)<=tolerance*tolerance;}
 static void Move(RE::Actor*a,const ActorPlacement&p){a->SetPosition({static_cast<float>(p.position.x),static_cast<float>(p.position.y),static_cast<float>(p.position.z)},true);a->SetAngle(p.angle);}
 static bool MoveConfirmed(RE::Actor*a,const ActorPlacement&p){Move(a,p);return SamePlace(a,p,2)&&std::isfinite(a->GetAngleZ())&&std::abs(std::remainder(double(a->GetAngleZ()-p.angle.z),6.283185307179586))<=.02;}
 // Actor-local engine APIs only: do not stop a whole scene or rewrite its quest state.
 static bool Hold(ActorSlot&s,RE::Actor*a,bool stopTravel=true){if(!Usable(a,true)||!a->GetCharController()||a==RE::PlayerCharacter::GetSingleton())return false;auto life=a->AsActorState()->GetLifeState();if(life!=RE::ACTOR_LIFE_STATE::kAlive&&life!=RE::ACTOR_LIFE_STATE::kRestrained)return false;
  bool interrupted=a->GetCurrentScene()||a->IsInCombat()||life!=RE::ACTOR_LIFE_STATE::kRestrained;
  if(!s.lifeCaptured){s.originalLife=life;s.originalAI=a->IsAIEnabled();s.lifeCaptured=true;}
  if(!a->IsAIEnabled()){s.enabledAI=!s.originalAI;a->EnableAI(true);if(!a->IsAIEnabled())return false;}
  if(auto*scene=a->GetCurrentScene()){if(!s.sceneDetached)s.detachedScene=scene->GetFormID();s.sceneDetached=true;a->SetCurrentScene(nullptr);if(a->GetCurrentScene())return false;}
  if(a->IsInCombat())a->StopCombat();
  if(!Usable(a,true))return false;life=a->AsActorState()->GetLifeState();if(life!=RE::ACTOR_LIFE_STATE::kAlive&&life!=RE::ACTOR_LIFE_STATE::kRestrained)return false;
  if(life!=RE::ACTOR_LIFE_STATE::kRestrained)a->SetLifeState(RE::ACTOR_LIFE_STATE::kRestrained);
  s.restrained=a->AsActorState()->GetLifeState()==RE::ACTOR_LIFE_STATE::kRestrained;if(!s.restrained)return false;if(stopTravel||interrupted)a->StopMoving(1.f);return true;
 }
 static void Collision(ActorSlot&s,RE::Actor*a,bool acquire){
  auto*root=a->Get3D(false);auto*cc=a->GetCharController();if(!root||!cc)return;
  if(acquire&&!s.collisionByUs){s.originalCollision=cc->flags.any(RE::CHARACTER_FLAGS::kNoCharacterCollisions);if(!s.originalCollision){cc->flags.set(RE::CHARACTER_FLAGS::kNoCharacterCollisions);s.collisionRoot.reset(root);s.collisionByUs=true;}}
  else if(!acquire&&s.collisionByUs){if(s.collisionRoot.get()==root&&cc->flags.any(RE::CHARACTER_FLAGS::kNoCharacterCollisions))cc->flags.reset(RE::CHARACTER_FLAGS::kNoCharacterCollisions);s.collisionByUs=false;s.collisionRoot.reset();}
 }
 ReleaseResult ReleaseSlot(ActorSlot&s,bool world,RE::TESFaction*faction){if(!world){s.membership=false;s.ClearPlacement();return ReleaseResult::Abandoned;}auto a=s.actor.get();if(a&&a->IsDeleted()){s.membership=false;s.ClearPlacement();return ReleaseResult::Abandoned;}if(!a||a->IsDisabled()||!a->GetParentCell()||!a->Get3D(false)||(s.collisionByUs&&a->Get3D(false)==s.collisionRoot.get()&&!a->GetCharController()))return ReleaseResult::Pending;{
  if(s.membership&&faction&&a->IsInFaction(faction))a->RemoveFromFaction(faction);
  bool own=s.restrained&&s.lifeCaptured&&a->AsActorState()->GetLifeState()==RE::ACTOR_LIFE_STATE::kRestrained&&!a->IsDead();
  bool returnSafe=own&&!ExternalControl(a.get())&&s.original&&FinitePlacement(s.original->position)&&s.placement&&Usable(a.get())&&SamePlace(a.get(),*s.placement,256)&&s.original->cell==s.placement->cell&&a->GetCurrentPackage()==s.originalPackage;
  if(returnSafe)Move(a.get(),*s.original);
  if(own)a->SetLifeState(s.originalLife);if(s.enabledAI&&a->IsAIEnabled()&&(!a->GetCurrentScene()||(s.sceneDetached&&a->GetCurrentScene()->GetFormID()==s.detachedScene))){a->EnableAI(false);s.enabledAI=false;}if(own&&a->IsAIEnabled())a->EvaluatePackage();
  Collision(s,a.get(),false);
 }bool yielded=ExternalControl(a.get());s.membership=false;s.ClearPlacement();return yielded?ReleaseResult::Yielded:ReleaseResult::Restored;}
 ReleaseResult Release(unsigned i,bool world,RE::TESFaction*faction){auto&s=slots.at(i);auto saved=s.placement;auto held=s.actor.get();bool player=held&&held.get()==RE::PlayerCharacter::GetSingleton();s.placementValid=false;
  ReleaseResult result=ReleaseResult::Restored;
  if(!s.restrained&&!s.enabledAI&&!s.membership&&!s.collisionByUs&&!s.original)s.ClearPlacement();else{result=ReleaseSlot(s,world,faction);if(result==ReleaseResult::Pending){if(deferred.size()>=16)throw std::runtime_error("NPC restoration queue full; existing slot retained");deferred.push_back({s});s.membership=false;s.ClearPlacement();}}
  // Coordinates may be useful later; only the player's start marker remains usable.
  if(world&&saved){s.placement=saved;s.placementValid=player;}return result;
 }
 void RetryReleases(bool world,RE::TESFaction*faction){for(auto it=deferred.begin();it!=deferred.end();){try{auto result=ReleaseSlot(it->slot,world,faction);if(result==ReleaseResult::Pending&&++it->observations<60){++it;continue;}if(result==ReleaseResult::Pending||result==ReleaseResult::Abandoned)++abandoned;it=deferred.erase(it);}catch(const std::exception&){++releaseFailures;if(++it->observations>=60){++abandoned;it=deferred.erase(it);}else ++it;}}}
 void Reset(bool world,RE::TESFaction*faction,bool clear){for(unsigned i=0;i<MaxPerformers;++i){try{Release(i,world,faction);if(clear)slots[i]={};}catch(const std::exception&){++releaseFailures;if(world&&deferred.size()<16){try{deferred.push_back({slots[i]});slots[i].membership=false;slots[i].ClearPlacement();if(clear)slots[i]={};}catch(const std::bad_alloc&){/* Keep the slot lease when queue allocation fails. */}}}}if(!world){deferred.clear();}if(clear)camera={};}
 void Defaults(){if(!Count())if(auto*p=RE::PlayerCharacter::GetSingleton())slots[0].actor=p->GetHandle();}
 bool AssignPlayer(unsigned i){auto*p=RE::PlayerCharacter::GetSingleton();if(i>=MaxPerformers||slots[i].actor.get()||!Usable(p)||Duplicate(p,i))return false;slots[i]={};slots[i].actor=p->GetHandle();FixCamera();return true;}
 bool Assign(unsigned i,RE::Actor*a,RE::TESFaction*faction){lastForceIssue=ForceIssue::None;if(i>=MaxPerformers||Excluded(a)||!Usable(a,true)||a==RE::PlayerCharacter::GetSingleton()||Duplicate(a,i))return false;auto life=a->AsActorState()->GetLifeState();if(life!=RE::ACTOR_LIFE_STATE::kAlive&&life!=RE::ACTOR_LIFE_STATE::kRestrained)return false;Release(i,true,faction);slots[i]={};slots[i].actor=a->GetHandle();FixCamera();return true;}
 // Complete one old release before taking a fresh lease. Both run on the engine thread.
 bool ResolveDeferred(RE::Actor*a,RE::TESFaction*faction){for(auto it=deferred.begin();it!=deferred.end();){if(it->slot.actor.get().get()!=a){++it;continue;}try{auto result=ReleaseSlot(it->slot,true,faction);if(result==ReleaseResult::Pending){lastForceIssue=ForceIssue::RestorePending;return false;}if(result==ReleaseResult::Abandoned){++abandoned;lastForceIssue=ForceIssue::RestorePending;return false;}it=deferred.erase(it);}catch(const std::exception&){++releaseFailures;lastForceIssue=ForceIssue::RestorePending;return false;}}return true;}
 bool Place(unsigned i,RE::TESFaction*faction=nullptr){lastForceIssue=ForceIssue::None;if(i>=MaxPerformers)return false;auto&s=slots[i];auto a=s.actor.get();auto*p=RE::PlayerCharacter::GetSingleton();if(Excluded(a.get())||!Usable(p)||!Usable(a.get(),true)||!p->GetCharController()||p->GetParentCell()!=a->GetParentCell()||p->AsActorState()->IsWeaponDrawn()||p->IsInWater())return false;
  auto place=Capture(p);if(!FinitePlacement(place.position)||!std::isfinite(place.angle.z))return false;place.angle.x=place.angle.y=0;
  // The live, grounded player's capsule validates the destination. Avoid
  // occupied placed slots rather than silently changing the chosen position.
  for(unsigned n=0;n<MaxPerformers;++n)if(n!=i)if(auto other=slots[n].actor.get();other&&other.get()!=p&&SamePlace(other.get(),place,48))return false;
  if(a.get()!=p){if(!ResolveDeferred(a.get(),faction))return false;if(!s.original){auto original=Capture(a.get());if(!FinitePlacement(original.position)||!std::isfinite(original.angle.x)||!std::isfinite(original.angle.y)||!std::isfinite(original.angle.z)){lastForceIssue=ForceIssue::Unsafe;return false;}s.original=original;s.originalPackage=a->GetCurrentPackage();}if(!Hold(s,a.get())){lastForceIssue=ForceIssue::Unsafe;Release(i,true,faction);return false;}Collision(s,a.get(),true);s.placement=place;if(!MoveConfirmed(a.get(),place)){lastForceIssue=ForceIssue::Movement;Release(i,true,faction);return false;}}
  s.placement=place;s.placementValid=true;s.controlFailures=0;s.nextControl=0;return true;
 }
 // Maintain actor control during the whole session; pin position only before playback.
 // Ordinary AI drift is repaired, not treated as lost ownership. Five failed observations
 // (normally 0.2 s apart) retire the lease through the existing release queue.
 void HoldPlaced(bool staging,double now=-1,RE::TESFaction*faction=nullptr,bool immediate=false){if(now<0)now=ControlTime();auto*p=RE::PlayerCharacter::GetSingleton();for(unsigned i=0;i<MaxPerformers;++i){auto&s=slots[i];if(!s.placementValid||!s.placement)continue;auto a=s.actor.get();if(!a||a.get()==p||!p||!p->GetParentCell()||!a->GetParentCell()||p->GetParentCell()->GetFormID()!=s.placement->cell||a->GetParentCell()!=p->GetParentCell())continue;if(!immediate&&now<s.nextControl)continue;s.nextControl=now+.2;
  bool success=false;try{success=Hold(s,a.get(),staging);if(success&&staging){if(!SamePlace(a.get(),*s.placement,1))success=MoveConfirmed(a.get(),*s.placement);else if(std::abs(std::remainder(double(a->GetAngleZ()-s.placement->angle.z),6.283185307179586))>.02)a->SetAngle(s.placement->angle);}}catch(const std::exception&){success=false;}
  if(success)s.controlFailures=0;else if(++s.controlFailures>=5){s.placementValid=false;++forcedInvalidations;lastForceIssue=ForceIssue::Movement;Release(i,true,faction);}
 }}
 // Always called while idle, including during world placement interaction.
 bool RefreshPlacements(bool world,RE::TESFaction*faction){bool changed=false;auto*p=RE::PlayerCharacter::GetSingleton();for(unsigned i=0;i<MaxPerformers;++i){auto&s=slots[i];auto a=s.actor.get();if(a&&a.get()==p)continue;if(!s.placementValid&&!s.restrained&&!s.enabledAI&&!s.collisionByUs&&!s.membership&&!s.original)continue;
  bool invalid=!world||!a||!a->Get3D(false)||a->IsDead()||a->IsDeleted()||a->IsDisabled()||!a->GetParentCell()||!p||p->IsDead()||!p->GetParentCell()||!s.placement||p->GetParentCell()->GetFormID()!=s.placement->cell||p->GetParentCell()!=a->GetParentCell();
  if(invalid){s.placementValid=false;Release(i,world,faction);changed=true;}
 }return changed;}
 StartIssue ValidateStart(RE::TESObjectCELL*cell,bool allowPlayerCombat=false)const{if(!Count())return StartIssue::NoActors;auto*p=RE::PlayerCharacter::GetSingleton();if(!cell||!p)return StartIssue::InvalidActor;
  for(auto&s:slots)if(s.actor){auto a=s.actor.get();if(Excluded(a.get()))return StartIssue::Excluded;if(Restoring(a.get()))return StartIssue::Restoring;if(!Usable(a.get(),a.get()==p&&allowPlayerCombat))return StartIssue::InvalidActor;if(a->GetParentCell()!=cell)return StartIssue::WrongCell;
   if(s.placement&&(!FinitePlacement(s.placement->position)||!std::isfinite(s.placement->angle.x)||!std::isfinite(s.placement->angle.y)||!std::isfinite(s.placement->angle.z)||s.placement->cell!=cell->GetFormID()))return StartIssue::WrongCell;
   if(a.get()!=p&&(!s.placement||!s.placementValid||!s.restrained||a->AsActorState()->GetLifeState()!=RE::ACTOR_LIFE_STATE::kRestrained||!SamePlace(a.get(),*s.placement,2)))return StartIssue::NeedsPlacement;
  }auto anchor=CameraActor();if(!anchor||anchor->GetParentCell()!=cell)return StartIssue::Camera;return StartIssue::None;
 }
 bool MovePlayerToStart(RE::TESObjectCELL*cell){auto*p=RE::PlayerCharacter::GetSingleton();for(auto&s:slots)if(auto a=s.actor.get();a&&a.get()==p&&s.placement){if(!cell||!p->GetParentCell()||p->GetParentCell()!=cell||s.placement->cell!=cell->GetFormID())return false;if(!MoveConfirmed(p,*s.placement))return false;}return true;}
 void Separation(){auto*p=RE::PlayerCharacter::GetSingleton();if(!p)return;for(auto&s:slots)if(s.collisionByUs)if(auto a=s.actor.get()){auto delta=a->GetPosition()-p->GetPosition();if(delta.SqrLength()>96*96)Collision(s,a.get(),false);}}
};
}
