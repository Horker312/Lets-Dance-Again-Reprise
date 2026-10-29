#include "../runtime/WheelFov.h"
#include "../runtime/SessionInputPolicy.h"
#include "../runtime/InteractiveCamera.h"
#include <thread>
#include <vector>
#include <iostream>
#include <stdexcept>
using namespace DanceStudio;
struct Camera {bool free{},freeze{};int cameraTarget{1},restored{},toggles{};bool IsInFreeCameraMode(){return free;}void ToggleFreeCameraMode(bool frozen){freeze=frozen;free=!free;++toggles;}void ForceFirstPerson(){restored=1;}void ForceThirdPerson(){restored=3;}};
int main(){try{int checks=0;auto Check=[&](bool v,const char*why){++checks;if(!v)throw std::runtime_error(why);};
 WheelFovQueue queue;Check(!queue.Input(-1,true),"inactive input is passed through");queue.Configure(1,true);
 Check(queue.Input(-1,true)&&queue.Input(1,true),"both wheel directions accepted");Check(queue.Input(1,false),"release consumed without another step");auto batch=queue.Drain(1);Check(batch.count==2&&batch.steps[0]==-1&&batch.steps[1]==1,"FIFO preserves directions");Check(queue.Drain(1).count==0,"events applied only once");
 for(int i=0;i<1000;++i)Check(queue.Input(1,true),"overflow remains consumed");Check(queue.Drain(1).count==128,"bounded queue");
 queue.Input(1,true);queue.Configure(1,false);Check(queue.Drain(1).count==0&&!queue.Input(1,true),"Photo Mode/UI transition drops pending input and passes new input");
 queue.Configure(1,true);queue.Input(1,true);queue.Configure(2,true);Check(queue.Drain(2).count==0,"new session discards stale events");
 queue.Input(-1,true);queue.Disable();Check(queue.Drain(2).count==0&&!queue.Input(-1,true),"stop clears and disables queue");
 queue.Configure(2,true);auto stale=queue.Token();queue.Configure(2,false);queue.Configure(2,true);Check(!queue.Input(1,true,stale),"a delayed callback cannot cross a menu/Photo Mode transition");stale=queue.Token();queue.Configure(3,true);Check(!queue.Input(1,true,stale),"a delayed callback cannot cross a session transition");
 queue.Configure(3,true);std::vector<std::thread> workers;for(int i=0;i<8;++i)workers.emplace_back([&]{for(int n=0;n<8;++n)queue.Input(-1,true);});for(auto&t:workers)t.join();batch=queue.Drain(3);Check(batch.count==64,"worker input joins update thread without loss");WheelFov fov;for(size_t i=0;i<batch.count;++i)fov.Step(45,batch.steps[i]);Check(fov.Value(45)==5,"queued steps use normal FOV limits");
 ConcurrentSpaceGate space;std::atomic<unsigned> toggles{};workers.clear();for(int i=0;i<8;++i)workers.emplace_back([&]{if(space.Event(SessionState::Playing,true,false).toggle)++toggles;});for(auto&t:workers)t.join();Check(toggles==1,"concurrent Space downs toggle once");space.Event(SessionState::Playing,false,true);Check(space.Event(SessionState::Playing,true,false).toggle,"Space can toggle again after release");
 Camera camera;Check(EnterInteractiveCamera(camera)&&camera.free&&!camera.freeze,"native free camera enters without stopping time");Check(EnterInteractiveCamera(camera)&&camera.toggles==1,"repeated enter never toggles back to orbit");Check(ExitInteractiveCamera(camera,1,true)&&!camera.free&&camera.restored==1,"first-person restoration");Check(!ExitInteractiveCamera(camera,1,true),"cleanup idempotence");EnterInteractiveCamera(camera);camera.cameraTarget=2;Check(!ExitInteractiveCamera(camera,1,false)&&camera.free,"do not steal Photo Mode/other target");camera.cameraTarget=1;Check(ExitInteractiveCamera(camera,1,false)&&camera.restored==3,"deferred restoration when our target returns");
 std::cout<<"PASS "<<checks<<" queue threading, FIFO, bounds, transition, Space and free-camera restoration checks\n";
 }catch(const std::exception&e){std::cerr<<e.what();return 1;}}
