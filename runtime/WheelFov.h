#pragma once
#include "SessionCore.h"
#include <atomic>
#include <algorithm>
#include <cmath>
namespace DanceStudio {
inline bool MayAdjustWheelFov(SessionState state,bool recordedCamera,bool ownsCamera,bool photoMode,bool blocked){return state==SessionState::Playing&&recordedCamera&&ownsCamera&&!photoMode&&!blocked;}
// Session-only additive correction. Source animated FOV remains unchanged.
class WheelFov {
 std::atomic<double> offset{};
 static double Source(double fov){return std::isfinite(fov)?std::clamp(fov,5.,140.):45.;}
public:
 void Reset(){offset=0;}
 double Value(double source)const{return std::clamp(Source(source)+offset.load(),5.,140.);}
 void Step(double source,int direction){
  if(direction!=1&&direction!=-1)return;
  source=Source(source);auto previous=offset.load();
  for(;;){auto next=std::clamp(std::clamp(source+previous,5.,140.)+direction*2.,5.,140.)-source;if(offset.compare_exchange_weak(previous,next))return;}
 }
};
}
