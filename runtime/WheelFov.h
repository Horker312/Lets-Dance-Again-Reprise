#pragma once
#include "SessionCore.h"
#include <atomic>
#include <algorithm>
#include <cmath>
#include <array>
#include <mutex>
namespace DanceStudio {
inline bool MayAdjustWheelFov(SessionState state,bool recordedCamera,bool ownsCamera,bool photoMode,bool blocked){return state==SessionState::Playing&&recordedCamera&&ownsCamera&&!photoMode&&!blocked;}
// Input callbacks run on multiple worker threads. They only enqueue integers;
// camera/session objects are read and written exclusively by the update thread.
class WheelFovQueue {
 std::mutex mutex;std::uint64_t generation{},revision{1};bool admitted{};
 std::array<int,128> events{};std::size_t count{};
public:
 struct Batch {std::array<int,128> steps{};std::size_t count{};};
 void Configure(std::uint64_t session,bool allow){std::scoped_lock lock(mutex);if(session!=generation||allow!=admitted){++revision;count=0;}if(!allow)count=0;generation=session;admitted=allow;}
 void Disable(){std::scoped_lock lock(mutex);++revision;admitted=false;count=0;}
 std::uint64_t Token(){std::scoped_lock lock(mutex);return admitted?revision:0;}
 bool Input(int direction,bool pressed,std::uint64_t token){std::scoped_lock lock(mutex);if(!admitted||!token||token!=revision)return false;if(pressed&&(direction==1||direction==-1)&&count<events.size())events[count++]=direction;return true;}
 bool Input(int direction,bool pressed){return Input(direction,pressed,Token());}
 Batch Drain(std::uint64_t session){std::scoped_lock lock(mutex);Batch batch;if(admitted&&session==generation){batch.count=count;std::copy_n(events.begin(),count,batch.steps.begin());}count=0;return batch;}
};
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
