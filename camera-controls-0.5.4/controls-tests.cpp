#include "../runtime/CameraFramingSettings.h"
#include "../runtime/WheelFov.h"
#include "../runtime/FovRestoration.h"
#include <iostream>
#include <limits>
using namespace DanceStudio;
int wmain(int argc,wchar_t**argv){try{
 int checks=0;auto Check=[&](bool v,const char*why){++checks;if(!v)throw std::runtime_error(why);};
 auto dir=std::filesystem::path(argv[1]);std::filesystem::create_directories(dir);auto path=dir/"CameraFraming.json";
 CameraFramingStore store(path);Check(store.Load().empty()&&store.Current().scaleEnabled,"new installs enabled by default");
 {std::ofstream f(path);f<<R"({"schemaVersion":1,"distancePercent":125,"motionPercent":75})";}
 Check(!store.Load().empty()&&store.Current()==CameraFramingTuning{100,100,true},"legacy first-frame settings migrate with an explicit warning");
 auto tuning=store.Current();tuning.distancePercent=125;tuning.motionPercent=75;tuning.scaleEnabled=false;std::string error;Check(store.Save(tuning,error),"checkbox save");CameraFramingStore reopened(path);Check(reopened.Load().empty()&&reopened.Current()==CameraFramingTuning{100,75,false},"new-policy checkbox persists without losing manual multipliers");
 tuning.scaleEnabled=true;Check(reopened.Save(tuning,error)&&reopened.Current().scaleEnabled,"checkbox can be re-enabled");
 Vec camera{80,-240,150},first{0,-300,100};
 for(double scale:{.5,.75,1.,1.25,1.5,2.}){
  auto off=CorrectCameraFraming(camera,first,scale,20,{100,100,false});Check(Dot(off-(camera+Vec{0,0,20}),off-(camera+Vec{0,0,20}))<1e-16,"off bypasses XYZ scale only");
  auto on=CorrectCameraFraming(camera,first,scale,20,{100,100,true});Check(Dot(on-(camera*scale+Vec{0,0,20}),on-(camera*scale+Vec{0,0,20}))<1e-16,"on scales once, manual height once");
 }
 {std::ofstream f(path);f<<R"({"schemaVersion":1,"distancePercent":100,"motionPercent":100,"scaleEnabled":0})";}
 Check(!reopened.Load().empty()&&reopened.Current()==CameraFramingTuning{},"non-boolean setting rejected safely");
 Check(MayAdjustWheelFov(SessionState::Playing,true,true,false,false),"dedicated playback and internal user-pause permit wheel");
 for(auto state:{SessionState::Idle,SessionState::Preparing,SessionState::Armed,SessionState::Stopping})Check(!MayAdjustWheelFov(state,true,true,false,false),"wheel passthrough outside playback");
 Check(!MayAdjustWheelFov(SessionState::Playing,false,true,false,false),"free camera passthrough");Check(!MayAdjustWheelFov(SessionState::Playing,true,false,false,false),"lost ownership passthrough");Check(!MayAdjustWheelFov(SessionState::Playing,true,true,true,false),"Photo Mode passthrough");Check(!MayAdjustWheelFov(SessionState::Playing,true,true,false,true),"UI, console, typing and focus block wheel handling");
 WheelFov wheel;Check(wheel.Value(45)==45,"source FOV unchanged by default");wheel.Step(45,-1);Check(wheel.Value(45)==43,"wheel up zooms in");Check(wheel.Value(30)==28&&wheel.Value(60)==58&&wheel.Value(70)==68,"source animation continues under a fixed user correction");wheel.Step(45,1);Check(wheel.Value(45)==45,"opposite direction restores FOV");
 for(int i=0;i<1000;++i)wheel.Step(45,-1);Check(wheel.Value(45)==5,"lower limit");wheel.Step(45,1);Check(wheel.Value(45)==7,"immediate reverse at lower bound without dead wheel steps");
 for(int i=0;i<1000;++i)wheel.Step(45,1);Check(wheel.Value(45)==140,"upper limit");wheel.Step(45,-1);Check(wheel.Value(45)==138,"immediate reverse at upper bound");
 wheel.Step(45,0);Check(wheel.Value(45)==138,"invalid direction ignored");wheel.Reset();wheel.Step(45,-1);auto held=wheel.Value(45);for(int i=0;i<100;++i)Check(wheel.Value(45)==held,"paused source time preserves effective FOV");
 auto beforePhoto=wheel.Value(70);Check(!MayAdjustWheelFov(SessionState::Playing,true,true,true,false),"no FOV write while Photo Mode owns camera");Check(wheel.Value(70)==beforePhoto,"Photo Mode return keeps session correction at current source time");
 FovRestoration restore{75,static_cast<float>(held)};Check(restore.Value(static_cast<float>(held))==75,"stop restores pre-session game FOV");Check(!restore.Value(60).has_value(),"external FOV ownership is respected");wheel.Reset();Check(wheel.Value(45)==45,"new song/session resets wheel correction");
 Check(std::isfinite(wheel.Value(std::numeric_limits<double>::quiet_NaN())),"invalid source never produces NaN");
 std::ofstream(dir/"validation.json")<<Json{{"passed",true},{"checks",checks},{"gameTested",false},{"mouseCodes","CommonLib BSWin32MouseDevice::Key::kWheelUp/kWheelDown"}}.dump(2);
 std::cout<<"PASS "<<checks<<" scale checkbox, persistence, wheel scope, FOV limits and restoration checks\n";
 }catch(const std::exception&e){std::cerr<<e.what();return 1;}}
