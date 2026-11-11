#pragma once
#ifndef LDA_FACIAL_PORTABLE_TEST
#include "FacialTest.h"
#endif
#include "FacialRuntimePolicy.h"
#include "FacialTimeline.h"
#include "FacialEvaluation.h"
namespace DanceStudio::Facial {
class Playback {
 Test backend;SharedEvaluation ownEvaluation;const Timeline*ownTrack{};std::map<std::string,Target> applied;std::set<std::string> blocked;PendingBudget retries;double lastTime{-1};bool disabled{};
 static std::string Slot(const Key&k){return k.kind==Kind::Expression?"expression-group":Identity(k);}
public:
 #ifndef LDA_FACIAL_PORTABLE_TEST
 void SetActor(RE::ActorHandle a){backend.SetPerformanceActor(a);}
 auto Metrics()const{return backend.Metrics();}
 void ResetMetrics(){backend.ResetMetrics();}
#endif
 bool Releasing()const{return backend.PendingRelease();}
 void Drain(bool world,bool suspended=false)noexcept{if(!backend.PollRestore(world,suspended)){applied.clear();disabled=true;}}
 bool Active()const{return !applied.empty();}
 void Stop(bool restore=true)noexcept{backend.Close(restore,"performance end",true);applied.clear();blocked.clear();retries.Clear();lastTime=-1;disabled=false;ownEvaluation.Reset();ownTrack=nullptr;}
 void Update(const Timeline&track,double seconds)noexcept{
  if(ownTrack!=&track){ownEvaluation.Bind(track);ownTrack=&track;}if(auto*frame=ownEvaluation.Next(seconds))Apply(*frame);
 }
 void Apply(const EvaluationFrame&frame)noexcept{
  double seconds=frame.seconds;if(!std::isfinite(seconds)||seconds<0||disabled||seconds==lastTime)return;
  try{
   if(!backend.PerformanceValid()){backend.Close(false,"performance face changed");applied.clear();disabled=true;return;}
   const auto&next=frame.targets;lastTime=seconds;
   backend.BeginCycle();struct End{Test&b;bool finished{};void Finish(){finished=true;b.EndCycle();}~End(){if(!finished)try{b.EndCycle();}catch(...){}}}end{backend};
   for(auto&[id,old]:applied)if(!next.contains(id)||next.at(id).runStart!=old.runStart){backend.PerformanceRelease(old.key);retries.Gap(id);}
   for(auto&[id,t]:next){auto it=applied.find(id);if(blocked.contains(id)||retries.Suppressed(id)||(it!=applied.end()&&it->second==t&&!retries.Waiting(id)))continue;
    auto result=backend.PerformanceApply(t.key,t.value);if(result==Test::ApplyResult::Applied){retries.Applied(id);if(it==applied.end())SKSE::log::debug("Facial channel acquired: {} value={} musicTime={}; numeric only, visual unverified",id,t.value,seconds);}else if(result==Test::ApplyResult::Pending){if(!retries.Pending(id)){SKSE::log::warn("Facial PENDING budget exhausted for {}; yield until gap",id);}}else{blocked.insert(id);SKSE::log::warn("Facial external/unavailable {}; yield for song",id);}}

   applied=next;end.Finish();
  }catch(const std::exception&e){try{SKSE::log::warn("Facial performance disabled; music continues: {}",e.what());}catch(...){}backend.Close(true,"facial exception",true);applied.clear();disabled=true;}catch(...){backend.Close(false,"facial exception");applied.clear();disabled=true;}
 }
};
}
