#pragma once
#include <RE/Skyrim.h>
#include <array>
namespace DanceStudio {
// Per-actor lease. No tracking targets are cleared: restore only our toggles.
class PerformanceHeadTracking {
 struct Lease {RE::ActorHandle actor;RE::NiPointer<RE::NiAVObject> root;RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;bool active{},state{},ownState{};std::array<bool,2> graph{},present{},ownGraph{};};
 std::array<Lease,5> leases;
 static constexpr const char* names[2]={"bHeadTracking","bHeadTrackSpine"};
 static void Restore(Lease& l,bool valid){if(valid&&l.active)if(auto a=l.actor.get()){
  RE::BSTSmartPointer<RE::BSAnimationGraphManager> check;bool same=a->Get3D(false)==l.root.get()&&a->GetAnimationGraphManager(check)&&check.get()==l.manager.get()&&!a->IsDead()&&!PerformanceActors::ExternalControl(a.get());
  if(!same){l={};return;}if(l.ownState&&!a->AsActorState()->actorState2.headTracking)a->AsActorState()->actorState2.headTracking=l.state;
  RE::BSTSmartPointer<RE::BSAnimationGraphManager> current;
  if(a->Get3D(false)==l.root.get()&&a->GetAnimationGraphManager(current)&&current.get()==l.manager.get())for(unsigned i=0;i<2;++i)if(l.ownGraph[i]){bool value;if(a->GetGraphVariableBool(names[i],value)&&!value)a->SetGraphVariableBool(names[i],l.graph[i]);}
 }l={};}
public:
 void Stop(bool valid=true){for(auto&l:leases)Restore(l,valid);}
 template<class Slots>void Update(const Slots& slots){for(unsigned i=0;i<leases.size();++i){auto a=slots[i].actor.get();auto&l=leases[i];
  if(!a||!a->Get3D(false)||a->IsDead()||PerformanceActors::ExternalControl(a.get())){Restore(l,true);continue;}
  RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;if(!a->GetAnimationGraphManager(manager)||!manager){Restore(l,true);continue;}
  if(l.active&&(l.actor.get().get()!=a.get()||l.root.get()!=a->Get3D(false)||l.manager.get()!=manager.get()))Restore(l,true);
  if(l.active){if(l.ownState&&a->AsActorState()->actorState2.headTracking)l.ownState=false;for(unsigned n=0;n<2;++n)if(l.ownGraph[n]){bool v;if(a->GetGraphVariableBool(names[n],v)&&v)l.ownGraph[n]=false;}}
  if(!l.active){l.actor=slots[i].actor;l.root.reset(a->Get3D(false));l.manager=manager;l.state=a->AsActorState()->actorState2.headTracking;l.ownState=l.state;l.active=true;for(unsigned n=0;n<2;++n){l.present[n]=a->GetGraphVariableBool(names[n],l.graph[n]);l.ownGraph[n]=l.present[n]&&l.graph[n];}}
  if(l.ownState)a->AsActorState()->actorState2.headTracking=false;
  for(unsigned n=0;n<2;++n)if(l.ownGraph[n]){bool value;if(a->GetGraphVariableBool(names[n],value)&&value)a->SetGraphVariableBool(names[n],false);}
 }}
};
}
