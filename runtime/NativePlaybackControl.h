#pragma once
namespace DanceStudio {
// The comparison path may position a clip once at launch. Afterwards it must
// not seek it during Update/Generate, including paused or resumed frames.
template<class Clip,class Mode>void ApplyNativePlayback(Clip&clip,bool&started,bool advance,Mode normalMode){
 if(!started){clip.localTime=0;clip.time=0;if(clip.animationControl)clip.animationControl->localTime=0;started=true;}
 clip.mode=normalMode;clip.playbackSpeed=advance?1.f:0.f;
}
}
