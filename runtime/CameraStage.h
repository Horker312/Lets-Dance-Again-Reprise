#pragma once
#include "Localization.h"
#include "TrackData.h"
namespace DanceStudio {
// Never create a physical character capsule inside the dancer. Keep the
// helper above the same XY/cell while its asynchronously loaded 3D is prepared.
inline Vec CameraParking(Vec stage) { return stage+Vec{0,0,1024}; }
// Camera samples belong to the captured dance stage, not the helper's capsule.
inline Vec StageCameraPosition(Vec stage,Quat rotation,Vec sample) { return stage+rotation.Apply(sample); }
// Prefer the scale of the visible third-person skeleton. Do not multiply
// reference/race/render scales: the world transform already includes parents.
struct HeightScale {double value;const char* source;};
inline bool ValidHeightScale(double s){return std::isfinite(s)&&s>0&&s<=100;}
inline HeightScale ResolveHeightScale(double skeleton,double scene,double base,double reference){
 if(ValidHeightScale(skeleton))return{skeleton,"skeleton-world"};
 if(ValidHeightScale(scene))return{scene,"scene-world"};
 if(ValidHeightScale(base))return{base,"base-height"};
 if(ValidHeightScale(reference))return{reference,"reference"};
 return{1.,"default"};
}
inline Vec ScaleCameraHeight(Vec offset,double playerScale) {
    // A 0.01 decrease from scale 1.00 is one percent of the original height.
    // Never accumulate percentages frame by frame. XY/choreography stay intact.
    if(!std::isfinite(playerScale)||playerScale<=0||playerScale>100)playerScale=1;
    offset.z*=playerScale;return offset;
}
inline Vec CorrectCameraHeight(Vec offset,double scale,double height){auto result=ScaleCameraHeight(offset,scale);result.z+=height;return result;}
inline Vec ScaleCameraPath(Vec offset,double scale){if(!ValidHeightScale(scale))scale=1.;return offset*scale;}
inline Vec CorrectCameraPath(Vec offset,double scale,double height){auto result=ScaleCameraPath(offset,scale);result.z+=height;return result;}
struct CameraFramingTuning {
 // Legacy settings field; no longer exposed or used by the transform.
 unsigned distancePercent{100},motionPercent{100};
 bool scaleEnabled{true};
 bool operator==(const CameraFramingTuning&)const=default;
};
inline bool ValidCameraFraming(CameraFramingTuning t){return t.distancePercent>=25&&t.distancePercent<=400&&t.motionPercent<=400;}
// All inputs are unmodified source-track coordinates at the current audio time.
// Default settings are exactly C'=s*C, independent of the optional effect pivot.
// The pivot comes from explicit metadata or the immutable track-time mean.
// No previous output, actor world position, Z-only scale or camera-helper scale
// participates here. Manual height is added once in unscaled world units.
inline Vec CorrectCameraFraming(Vec sample,Vec pivot,double scale,double height,CameraFramingTuning tuning={}){
 if(!ValidCameraFraming(tuning))tuning={};
 if(!tuning.scaleEnabled||!ValidHeightScale(scale))scale=1.;
 Vec result;
 if(tuning.motionPercent==100)result=sample*scale;
 else result=(pivot+(sample-pivot)*(tuning.motionPercent/100.))*scale;
 result.z+=height;return result;
}
// Per-session snapshot: animated node scaling must never change the camera
// multiplier after preparation. Explicit source scale remains track metadata.
class SessionCameraScale {
 double captured{1};bool ready{};
public:
 void Reset(){captured=1;ready=false;}
 void Capture(double scale){if(!ready){captured=ValidHeightScale(scale)?scale:1.;ready=true;}}
 double Ratio(double referenceScale=1)const{auto ratio=captured/referenceScale;if(!ValidHeightScale(ratio))throw std::runtime_error(L10n::T("r0165"));return ratio;}
};
inline Vec FrameCameraPosition(Vec player,Quat basis,Vec sourceRelative,Vec pivot,double scale,double height,CameraFramingTuning tuning={}){
 auto world=StageCameraPosition(player,basis,CorrectCameraFraming(sourceRelative,pivot,scale,0,tuning));world.z+=height;return world;
}
inline Vec FollowCameraPosition(Vec published,Vec oldOrigin,Quat oldBasis,Vec currentOrigin,Quat currentBasis){return currentOrigin+(currentBasis*oldBasis.Inv()).Apply(published-oldOrigin);}
inline Quat FollowCameraRotation(Quat published,Quat oldBasis,Quat currentBasis){return Unit(currentBasis*oldBasis.Inv()*published);}
inline Vec CameraAnchorPosition(Vec worldCamera,Vec actorPosition,Quat actorRotation) {
    return actorRotation.Inv().Apply(worldCamera-actorPosition);
}
class StableStage {
    Vec anchor{},target{};double previous{};
public:
    void Reset(Vec position,double now){anchor=target=position;previous=now;}
    Vec Update(Vec position,double now){
        if(!std::isfinite(Dot(position,position))||!std::isfinite(now))throw std::runtime_error(L10n::T("r0139"));
        anchor=target=position;previous=now;return anchor;
    }
};
enum class StageFault { None, ActorLost, CellChanged, InvalidPosition };
inline StageFault CheckStage(bool actorPresent,bool sameCell,Vec displacement) {
    if (!actorPresent) return StageFault::ActorLost;
    if (!sameCell) return StageFault::CellChanged;
    double distance=Dot(displacement,displacement);
    if (!std::isfinite(distance)) return StageFault::InvalidPosition;
    return StageFault::None;
}
inline const char* StageFaultName(StageFault fault) {
    switch(fault) {
    case StageFault::ActorLost:return "Camera actor was lost";
    case StageFault::CellChanged:return "Player changed cells";
    case StageFault::InvalidPosition:return "Invalid player position";
    default:return "Stage is valid";
    }
}
// No work while healthy/paused. Persistent conflicts have bounded retry rates;
// they never turn a camera/animation interruption into an automatic stop.
class PriorityRecovery {
    double nextCamera{},nextGraph{};
public:
    void Reset(){nextCamera=nextGraph=0;}
    bool CameraDue(double now,bool changed,bool paused){
        if (!std::isfinite(now)||!changed||paused||now<nextCamera) return false;
        nextCamera=now+.25;return true;
    }
    bool GraphDue(double now,bool ready,bool paused){
        if (!std::isfinite(now)||ready||paused||now<nextGraph) return false;
        nextGraph=now+1.;return true;
    }
};
}
