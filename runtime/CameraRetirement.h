#pragma once
#include <cmath>
#include <cstdint>
namespace DanceStudio {
// Observations must come after distinct completed engine updates, never from
// the cleanup call that changed camera state. This is an observable return
// barrier, not a claim that undocumented engine jobs expose a completion API.
class CameraRetirementGate {
public:
 enum class Action { None, Disable, Delete, Release };
 explicit CameraRetirementGate(std::uint64_t tick):lastTick(tick){}
 Action Observe(bool returned,std::uint64_t tick,double now){
  if(tick<=lastTick)return Action::None;
  lastTick=tick;
  if(!returned||!std::isfinite(now)){stable=0;since=0;return Action::None;}
  if(!stable||now<since){stable=1;since=now;}else ++stable;
  if(stable<3||now-since<.1)return Action::None;
  return phase==0?Action::Disable:phase==1?Action::Delete:Action::Release;
 }
 void Completed(){++phase;stable=0;since=0;}
private:
 std::uint64_t lastTick{};unsigned phase{},stable{};double since{};
};
}
