#pragma once
#include "FacialTimeline.h"
#include <chrono>
#include <cstdint>
#include <limits>
namespace DanceStudio::Facial {
struct EvaluationFrame {double seconds{};std::map<std::string,Target> targets;};
// One evaluator per immutable timeline, not per actor. Actor snapshots, pending
// budgets and Ledgers stay in Playback. No game-time clock is introduced here.
class SharedEvaluation {
 const Timeline*track{};std::vector<double> boundaries;EvaluationFrame frame;
 double last{-1};bool disabled{},measure{};std::string error;
 std::uint64_t evaluations{},nanoseconds{};
public:
 void Reset(){track=nullptr;boundaries.clear();frame={};last=-1;disabled=false;error.clear();evaluations=nanoseconds=0;}
 void Bind(const Timeline&t,bool timed=false)noexcept{Reset();measure=timed;track=&t;try{for(auto&s:t.segments){boundaries.push_back(s.start);boundaries.push_back(s.end);if(s.curve)for(auto&p:*s.curve){double at=s.start+p.time-s.offset;if(at>s.start&&at<s.end)boundaries.push_back(at);}}std::sort(boundaries.begin(),boundaries.end());boundaries.erase(std::unique(boundaries.begin(),boundaries.end()),boundaries.end());}catch(...){disabled=true;error="facial evaluation indexing failed";}}
 const EvaluationFrame*Next(double seconds)noexcept{
  if(!track||disabled||track->segments.empty()||!std::isfinite(seconds)||seconds<0||seconds==last)return nullptr;
  // Never throttle a segment/key/gap boundary. A backwards seek also evaluates
  // immediately. Paused music produces no new frame and no API work.
  auto boundary=std::upper_bound(boundaries.begin(),boundaries.end(),last);
  bool crossed=boundary!=boundaries.end()&&*boundary<=seconds;
  if(last>=0&&seconds>last&&seconds-last+1.e-9<1./30&&!crossed)return nullptr;
  auto started=measure?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
  try{EvaluationFrame next;next.seconds=seconds;for(auto&t:track->At(seconds))next.targets.emplace(Timeline::Slot(t.key),std::move(t));frame=std::move(next);++evaluations;last=seconds;}
  catch(...){disabled=true;error="facial evaluation failed; participant control released";frame={seconds,{}};last=seconds;}
  if(measure)nanoseconds+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count();
  return &frame; // Empty results release only each actor's owned morphs.
 }
 std::uint64_t Evaluations()const{return evaluations;}std::uint64_t Nanoseconds()const{return nanoseconds;}
 const std::string&Error()const{return error;}
};
}
