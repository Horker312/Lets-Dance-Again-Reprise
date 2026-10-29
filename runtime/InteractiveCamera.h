#pragma once
namespace DanceStudio {
template<class Camera>bool EnterInteractiveCamera(Camera&camera){
 if(!camera.IsInFreeCameraMode())camera.ToggleFreeCameraMode(false);
 return camera.IsInFreeCameraMode();
}
template<class Camera,class Target>bool ExitInteractiveCamera(Camera&camera,const Target&expectedTarget,bool firstPerson){
 if(!camera.IsInFreeCameraMode()||camera.cameraTarget!=expectedTarget)return false;
 camera.ToggleFreeCameraMode(false);
 if(firstPerson)camera.ForceFirstPerson();else camera.ForceThirdPerson();
 return true;
}
}
