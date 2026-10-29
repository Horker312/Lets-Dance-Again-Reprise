#pragma once
#include <cstdint>
#include <cmath>
namespace DanceStudio {
// Only active playback counts towards a stall. Pauses/recovery reset the baseline.
class ProgressWatch {
 double last{};std::uint64_t frames{};bool tracking{};
public:
 void Reset()noexcept{tracking=false;}
 bool Stalled(double now,std::uint64_t sample,bool active,double timeout){
  if(!active||!std::isfinite(now)||timeout<=0){Reset();return false;}
  if(!tracking||sample!=frames||now<last){tracking=true;frames=sample;last=now;return false;}
  return now-last>=timeout;
 }
};
template<class F>bool BestEffort(F&& operation)noexcept{try{operation();return true;}catch(...){return false;}}
}
