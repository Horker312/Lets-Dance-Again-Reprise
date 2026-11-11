#pragma once
#include <cmath>
namespace DanceStudio {
// The generator's optional cached binding is not the playback control's
// binding. Read the object actually sampled by hkaAnimationControl. In
// particular, a missing control must not pass the preparation/READY gate.
// Called only during this clip's engine animation callbacks; never retained
// or dereferenced later by the UI/main-thread diagnostics.
template<class Clip> auto PlaybackBinding(Clip* clip)->decltype(clip->binding){
 if(!clip||!clip->animationControl)return nullptr;
 return clip->animationControl->binding;
}
template<class Binding> bool PlaybackBindingMatches(Binding* binding,double duration){
 return binding&&binding->animation&&std::isfinite(duration)&&duration>0&&
  std::isfinite(binding->animation->duration)&&
  std::abs(binding->animation->duration-duration)<=.005;
}
}
