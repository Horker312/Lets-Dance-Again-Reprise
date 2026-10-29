#pragma once
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
// Scale the authored path around the dancer's stage origin, not the absolute
// world position. Keep orientation/FOV/time untouched; manual height is in
// world units and must not grow with the player.
inline Vec ScaleCameraPath(Vec offset,double scale){if(!ValidHeightScale(scale))scale=1.;return offset*scale;}
inline Vec CorrectCameraPath(Vec offset,double scale,double height){auto result=ScaleCameraPath(offset,scale);result.z+=height;return result;}
inline Vec CameraAnchorPosition(Vec worldCamera,Vec actorPosition,Quat actorRotation) {
    return actorRotation.Inv().Apply(worldCamera-actorPosition);
}
class StableStage {
    Vec anchor{},target{};double previous{};
public:
    void Reset(Vec position,double now){anchor=target=position;previous=now;}
    Vec Update(Vec position,double now){
        if(!std::isfinite(Dot(position,position))||!std::isfinite(now))throw std::runtime_error("Invalid stage position/time");
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
