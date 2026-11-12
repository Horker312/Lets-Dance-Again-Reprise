#pragma once
#include "AliasLease.h"
#ifndef LDA_ALIAS_TEST_PORT
namespace DanceStudio {
struct AliasEnginePort {
 using Actor=RE::Actor;
 inline static RE::TESQuest*quest{};inline static RE::TESPackage*package{};inline static bool initialized{},startRequested{};
 inline static std::array<RE::BGSRefAlias*,5>aliases{};inline static std::array<RE::FormID,5>markerIDs{};
 inline static std::array<RE::NiPointer<RE::Actor>,5>releasing{};inline static std::array<double,5>evaluationDue{};
 inline static std::array<RE::FormID,5>lastPackage{};
 inline static std::array<AliasPackageState,5>packageState{};inline static std::array<bool,5>packageObserved{},reevaluating{};
 inline static std::array<AliasDetachState,5>detachState{};inline static std::array<bool,5>detachObserved{};inline static std::uint64_t frame{};inline static double now{};
 static bool Ready(){if(!initialized||!quest||!package)return false;if(quest->IsStopped()){if(!startRequested){startRequested=true;bool started=quest->Start();SKSE::log::info("Performance quest start requested: quest={:08X} accepted={}",quest->GetFormID(),started);}return false;}return !quest->IsStarting()&&!quest->IsStopping();}
 static std::uint32_t Identity(Actor*a){return a?a->GetFormID():0;}
 static RE::TESObjectREFR*Marker(unsigned i){if(i>=5)return nullptr;auto*ref=RE::TESForm::LookupByID<RE::TESObjectREFR>(markerIDs[i]);return ref&&!ref->IsDeleted()&&!ref->IsDisabled()&&!ref->As<RE::Actor>()&&ref->IsPersistent()&&ref->GetBaseObject()&&ref->GetBaseObject()->GetFormID()==0x3B?ref:nullptr;}
 static std::uint32_t Read(unsigned i){if(!quest||i>=5)return 0;auto ref=quest->GetAliasedRef(i).get();return ref?ref->GetFormID():0;}
 static bool Parked(unsigned i,std::uint32_t ref){return i<5&&ref==markerIDs[i]&&Marker(i);}
 static bool Available(unsigned i,std::uint32_t ref){return ref==0||Parked(i,ref);}
 static std::uint64_t Frame(){return frame;}
 static void Force(unsigned i,Actor*a,std::uint32_t token,unsigned participant){releasing[i].reset();detachObserved[i]=false;packageObserved[i]=false;reevaluating[i]=true;evaluationDue[i]=now+1;aliases[i]->ForceRefTo(a);a->EvaluatePackage();SKSE::log::info("Alias assign request: actor={:08X} participant={} session={} quest={:08X} alias={} observed={:08X}",Identity(a),participant+1,token,quest->GetFormID(),i,Read(i));}
 static AliasDetachState Detached(unsigned i,std::uint32_t actor){
  AliasDetachState state=AliasDetachState::Detached;
  if(actor){auto held=releasing[i];if(!held){auto*raw=RE::TESForm::LookupByID<RE::Actor>(actor);if(raw)held=raw->GetHandle().get();}
   if(!held)state=AliasDetachState::Unresolved;
   else if(held->GetFormID()!=actor||held->IsDeleted())state=AliasDetachState::Invalid;
   else if(auto*extra=held->extraList.GetByType<RE::ExtraAliasInstanceArray>()){RE::BSReadLockGuard guard(extra->lock);for(auto*link:extra->aliases)if(link&&link->quest==quest){state=AliasDetachState::Attached;break;}}
  }
  if(!detachObserved[i]||detachState[i]!=state){detachObserved[i]=true;detachState[i]=state;SKSE::log::info("Alias detach observation: alias={} actor={:08X} state={} (0=detached 1=attached 2=unresolved 3=invalid)",i,actor,static_cast<unsigned>(state));}
  return state;
 }
 static bool Evict(unsigned i,std::uint32_t token,std::uint32_t actor,unsigned participant){auto*marker=Marker(i);if(!marker||!aliases[i])return false;auto actual=Read(i);if(actual!=actor&&!Available(i,actual))return false;
  if(!releasing[i]){auto ref=quest->GetAliasedRef(i).get();if(ref&&ref->GetFormID()==actor)if(auto*a=ref->As<RE::Actor>())releasing[i].reset(a);}
  aliases[i]->ForceRefTo(marker);
  bool parked=Parked(i,Read(i)),detached=parked&&AliasDetachConfirmed(Detached(i,actor));
  if(detached&&releasing[i]&&!releasing[i]->IsDeleted()&&!releasing[i]->IsDead()&&releasing[i].get()!=RE::PlayerCharacter::GetSingleton())releasing[i]->EvaluatePackage(true,false);
  SKSE::log::info("Native alias eviction: actor={:08X} participant={} session={} quest={:08X} alias={} marker={:08X} parked={} detached={}",actor,participant+1,token,quest->GetFormID(),i,marker->GetFormID(),parked,detached);return parked;
 }
};
class PerformanceAlias {
 inline static AliasLeaseTable<AliasEnginePort>table;inline static std::array<std::uint64_t,5>orphan{};inline static std::array<std::uint32_t,5>orphanActor{},confirmed{};inline static std::array<AliasRetryBudget,5>orphanBudget{};
public:
 static bool Initialize(RE::TESDataHandler*data){AliasEnginePort::initialized=false;auto*q=data?data->LookupForm<RE::TESQuest>(0x803,"DanceStudioData.esl"):nullptr;auto*p=data?data->LookupForm<RE::TESPackage>(0x804,"DanceStudioData.esl"):nullptr;if(!q||!p||p->ownerQuest!=q||q->aliases.size()!=5)return false;AliasEnginePort::quest=q;AliasEnginePort::package=p;AliasEnginePort::aliases={};
  for(auto*base:q->aliases)if(base&&base->aliasID<5&&base->GetTypeString()=="Ref")AliasEnginePort::aliases[base->aliasID]=static_cast<RE::BGSRefAlias*>(base);
  for(unsigned i=0;i<5;++i){auto*a=AliasEnginePort::aliases[i];auto*marker=data->LookupForm<RE::TESObjectREFR>(0x806+i,"DanceStudioData.esl");if(!a||!a->flags.any(RE::BGSBaseAlias::FLAGS::kOptional)||a->flags.any(RE::BGSBaseAlias::FLAGS::kActorsOnly)||!marker)return false;AliasEnginePort::markerIDs[i]=marker->GetFormID();if(!AliasEnginePort::Marker(i))return false;}
  AliasEnginePort::initialized=true;SKSE::log::info("Native eviction data resolved: quest={:08X} priority={} package={:08X} aliases=5 persistentNonActorMarkers=5",q->GetFormID(),q->data.priority,p->GetFormID());return true;
 }
 static void ResetWorld(){table.Reset();orphan={};orphanActor={};confirmed={};orphanBudget={};AliasEnginePort::releasing={};AliasEnginePort::initialized=false;AliasEnginePort::startRequested=false;AliasEnginePort::evaluationDue={};AliasEnginePort::lastPackage={};AliasEnginePort::packageState={};AliasEnginePort::packageObserved={};AliasEnginePort::reevaluating={};AliasEnginePort::detachObserved={};}
 static void Tick(double now){++AliasEnginePort::frame;AliasEnginePort::now=now;if(!AliasEnginePort::initialized&&!Initialize(RE::TESDataHandler::GetSingleton()))return;if(!AliasEnginePort::Ready())return;
  for(unsigned i=0;i<5;++i){auto actor=AliasEnginePort::Read(i);if(!orphan[i]&&!AliasEnginePort::Parked(i,actor)){orphan[i]=table.Orphan(i,actor);if(orphan[i])orphanActor[i]=actor;}if(orphan[i]&&table.Release(orphan[i],orphanActor[i],orphanBudget[i].Permit(now))==AliasResult::Ready){SKSE::log::info("Saved alias swept: alias={} previous={:08X} parked=true detached=true",i,orphanActor[i]);orphan[i]=0;orphanActor[i]=0;orphanBudget[i]={};AliasEnginePort::releasing[i].reset();}}
 }
 static void UserRetry(){if(!AliasEnginePort::Ready())return;for(unsigned i=0;i<5;++i)if(orphan[i]&&table.Release(orphan[i],orphanActor[i])==AliasResult::Ready){orphan[i]=0;orphanActor[i]=0;orphanBudget[i]={};AliasEnginePort::releasing[i].reset();}}
 static AliasResult Acquire(RE::Actor*a,std::uint64_t&ticket,unsigned participant){if(!AliasEnginePort::initialized)return AliasResult::Pending;// Recovering slots already hold leases in table; other slots remain available.
  auto result=table.Acquire(a,ticket,participant);
  if(result==AliasResult::Ready){int i=table.SlotOf(ticket);if(i>=0&&confirmed[i]!=ticket){confirmed[i]=static_cast<std::uint32_t>(ticket);SKSE::log::info("Alias assignment confirmed: actor={:08X} participant={} session={} quest={:08X} alias={}",a->GetFormID(),participant+1,ticket,AliasEnginePort::quest->GetFormID(),i);}Maintain(ticket,a);}
  if(result==AliasResult::Failed)SKSE::log::warn("Alias assignment failed: actor={:08X} participant={} session={} quest={:08X}",a?a->GetFormID():0,participant+1,ticket,AliasEnginePort::quest?AliasEnginePort::quest->GetFormID():0);return result;
 }
 static bool Owned(std::uint64_t ticket,RE::Actor*a){return AliasEnginePort::initialized&&a&&table.Owned(ticket,a->GetFormID());}
 static AliasResult Release(std::uint64_t ticket,RE::Actor*a){if(!a)return AliasResult::Failed;int i=table.SlotOf(ticket);auto result=table.Release(ticket,a->GetFormID());if(result==AliasResult::Ready&&i>=0){confirmed[i]=0;AliasEnginePort::releasing[i].reset();SKSE::log::info("Alias native release confirmed: actor={:08X} session={} alias={} parked=true detached=true",a->GetFormID(),ticket,i);}return result;}
 static AliasResult ReleaseLost(std::uint64_t ticket){auto actor=table.ActorOf(ticket);int i=table.SlotOf(ticket);auto r=actor?table.Release(ticket,actor):AliasResult::Ready;if(r==AliasResult::Ready&&i>=0)AliasEnginePort::releasing[i].reset();return r;}
 static void Maintain(std::uint64_t ticket,RE::Actor*a){int i=table.SlotOf(ticket);if(i<0||!a)return;
  const bool owned=Owned(ticket,a),known=a->GetActorRuntimeData().currentProcess!=nullptr;
  auto*current=known?a->GetCurrentPackage():nullptr;auto id=current?current->GetFormID():0;
  const bool matches=current&&current==AliasEnginePort::package;if(matches)AliasEnginePort::reevaluating[i]=false;
  auto state=ObserveAliasPackage(known&&current,matches,AliasEnginePort::reevaluating[i]);
  if(!AliasEnginePort::packageObserved[i]||state!=AliasEnginePort::packageState[i]||id!=AliasEnginePort::lastPackage[i]){AliasEnginePort::packageObserved[i]=true;AliasEnginePort::packageState[i]=state;AliasEnginePort::lastPackage[i]=id;SKSE::log::info("Alias package state: actor={:08X} session={} alias={} owned={} state={} current={:08X} expected={:08X} (0=unknown 1=performance 2=other 3=reevaluating)",a->GetFormID(),ticket,i,owned,static_cast<unsigned>(state),id,AliasEnginePort::package->GetFormID());}
  // Package execution is diagnostic, never a new admission/cancellation condition.
  if(owned&&known&&!matches&&AliasEnginePort::now>=AliasEnginePort::evaluationDue[i]){AliasEnginePort::evaluationDue[i]=AliasEnginePort::now+1;AliasEnginePort::reevaluating[i]=true;a->EvaluatePackage();}
 }

};
}
#endif
