#pragma once
#ifndef LDA_FACIAL_PORTABLE_TEST
#include "FacialTest.h"
#endif
#include "FacialRuntimePolicy.h"
#include "FacialTimeline.h"
namespace DanceStudio::Facial {
class Playback {
 Test backend;std::map<std::string,Target> applied;std::set<std::string> blocked;PendingBudget retries;double lastTime{-1};bool disabled{};
 static std::string Slot(const Key&k){return k.kind==Kind::Expression?"expression-group":Identity(k);}
public:
 bool Releasing()const{return backend.PendingRelease();}
 void Drain(bool world,bool suspended=false)noexcept{if(!backend.PollRestore(world,suspended)){applied.clear();disabled=true;}}
 bool Active()const{return !applied.empty();}
 void Stop(bool restore=true)noexcept{backend.Close(restore,"performance end",true);applied.clear();blocked.clear();retries.Clear();lastTime=-1;disabled=false;}
 void Update(const Timeline&track,double seconds)noexcept{
  if(!std::isfinite(seconds)||seconds<0||disabled||track.segments.empty()||seconds==lastTime)return;
  try{
   if(!backend.PerformanceValid()){backend.Close(false,"performance face changed");applied.clear();disabled=true;return;}
   auto targets=track.At(seconds);std::map<std::string,Target> next;for(auto&t:targets)next.emplace(Slot(t.key),t);
   bool boundary=next.size()!=applied.size();if(!boundary)for(auto&[id,t]:next){auto it=applied.find(id);if(it==applied.end()||it->second.runStart!=t.runStart||!(it->second.key==t.key)){boundary=true;break;}}
   if(!boundary&&lastTime>=0&&seconds>lastTime&&seconds-lastTime<1./30)return;lastTime=seconds;
   backend.BeginCycle();struct End{Test&b;bool finished{};void Finish(){finished=true;b.EndCycle();}~End(){if(!finished)try{b.EndCycle();}catch(...){}}}end{backend};
   for(auto&[id,old]:applied)if(!next.contains(id)||next.at(id).runStart!=old.runStart){backend.PerformanceRelease(old.key);retries.Gap(id);}
   for(auto&[id,t]:next){auto it=applied.find(id);if(blocked.contains(id)||retries.Suppressed(id)||(it!=applied.end()&&it->second==t&&!retries.Waiting(id)))continue;
    auto result=backend.PerformanceApply(t.key,t.value);if(result==Test::ApplyResult::Applied){retries.Applied(id);if(it==applied.end())SKSE::log::info("Facial channel acquired: {} value={} musicTime={}; numeric only, visual unverified",id,t.value,seconds);}else if(result==Test::ApplyResult::Pending){if(!retries.Pending(id)){SKSE::log::warn("Facial PENDING budget exhausted for {}; yield until gap",id);}}else{blocked.insert(id);SKSE::log::warn("Facial external/unavailable {}; yield for song",id);}}

   applied=std::move(next);end.Finish();
  }catch(const std::exception&e){try{SKSE::log::warn("Facial performance disabled; music continues: {}",e.what());}catch(...){}backend.Close(true,"facial exception",true);applied.clear();disabled=true;}catch(...){backend.Close(false,"facial exception");applied.clear();disabled=true;}
 }
};
}
