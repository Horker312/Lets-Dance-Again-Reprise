#pragma once
#include <cstdint>
namespace DanceStudio {
enum class WorldModeDecision{Cancel,Suspend,Continue};
inline WorldModeDecision WorldModePolicy(bool validWorld,bool validActor,bool foreignMenu,bool actorReady){if(!validWorld||!validActor)return WorldModeDecision::Cancel;if(foreignMenu||!actorReady)return WorldModeDecision::Suspend;return WorldModeDecision::Continue;}
// Framework pause/input locks are released by its render/update cycle. Do not
// mistake that hand-off for a new external menu and reopen our window.
class WorldInteractionGate {
 bool ready{};unsigned clearFrames{};
public:
 void Begin(double now){ready=false;clearFrames=0;}
 enum class Result{Waiting,Ready};
 Result Update(double now,bool frameworkOpen,bool paused){if(frameworkOpen||paused){ready=false;clearFrames=0;return Result::Waiting;}if(ready)return Result::Ready;if(++clearFrames>=2){ready=true;return Result::Ready;}return Result::Waiting;}
 bool Ready()const{return ready;}
};
// UI time is excluded from recovery budgets; Begin is called only on entry.
class WorldModeMonitor {
 double last{},clock{},missing{-1},abnormal{-1};bool wasMenu{};
public:
 void Begin(double now){last=now;clock=0;missing=abnormal=-1;wasMenu=false;}
 void UITransition(){wasMenu=true;}
 WorldModeDecision Update(double now,bool terminal,bool available,bool menu,bool usable){
  double dt=now>last?now-last:0;last=now;
  if(terminal)return WorldModeDecision::Cancel;
  const bool exclude=menu||wasMenu;wasMenu=menu;
  if(menu)return WorldModeDecision::Suspend;
  if(!exclude)clock+=dt;
  if(!available){if(missing<0)missing=clock;return clock-missing>=3?WorldModeDecision::Cancel:WorldModeDecision::Suspend;}
  missing=-1;
  if(!usable){if(abnormal<0)abnormal=clock;return clock-abnormal>=15?WorldModeDecision::Cancel:WorldModeDecision::Suspend;}
  abnormal=-1;return WorldModeDecision::Continue;
 }
};
}
