#include "../runtime/CameraFramingSettings.h"
#include <iostream>
#include <limits>
using namespace DanceStudio;
int checks{};double maxError{},maxOffscreenError{};unsigned visibleCount{},hiddenCount{},closeupCount{},realVisible{},singular{};
void Check(bool v,const char*why){++checks;if(!v)throw std::runtime_error(why);}
bool Near(Vec a,Vec b){return Dot(a-b,a-b)<1e-16;}
struct Screen {double x,y,depth;bool visible;};
Screen Project(Vec point,Vec camera,Quat rotation,double fov,double aspect){
 auto p=rotation.Inv().Apply(point-camera);double h=std::tan(fov*3.141592653589793/360.);double x=p.x/(p.y*h),y=p.z/(p.y*h/aspect);
 return{x,y,p.y,p.y>0&&std::abs(x)<=1&&std::abs(y)<=1};
}
void Compare(Vec camera,Quat rotation,const std::vector<Vec>&points,Vec origin,Quat yaw,double fov,double aspect,bool real=false){
 for(double scale:{.5,.75,1.,1.25,1.5,2.}){
  auto adjusted=CorrectCameraFraming(camera,{91,-277,80},scale,0);
  if(scale==1)Check(adjusted.x==camera.x&&adjusted.y==camera.y&&adjusted.z==camera.z,"scale 1 source path identity");
  auto worldCamera=StageCameraPosition(origin,yaw,adjusted);auto worldRotation=Unit(yaw*rotation);
  std::vector<Screen> before,after;
  for(auto point:points){auto a=Project(point,camera,rotation,fov,aspect);auto b=Project(origin+yaw.Apply(point*scale),worldCamera,worldRotation,fov,aspect);
   Check(std::isfinite(b.depth),"finite depth");Check((a.depth>0)==(b.depth>0),"front/back classification");
   if(std::abs(a.depth)<1e-3){++singular;continue;} // Projection is singular at the camera plane.
   auto error=std::max(std::abs(a.x-b.x),std::abs(a.y-b.y));
   if(a.visible)maxError=std::max(error,maxError);else maxOffscreenError=std::max(error,maxOffscreenError);
   // Absolute subpixel bound for visible landmarks. Far outside the screen,
   // perspective coordinates can approach infinity; use a relative FP bound.
   Check(error<(a.visible?1e-7:1e-8*std::max({1.,std::abs(a.x),std::abs(a.y)})),"projected landmark XY invariant");Check(a.visible==b.visible,"no newly missing or newly revealed landmark");
   if(a.visible){++visibleCount;if(real)++realVisible;}else ++hiddenCount;
   before.push_back(a);after.push_back(b);
  }
  if(before.size()>1){double ax=0,ay=0,bx=0,by=0;for(size_t i=1;i<before.size();++i){ax=std::max(ax,std::abs(before[i].x-before[0].x));ay=std::max(ay,std::abs(before[i].y-before[0].y));bx=std::max(bx,std::abs(after[i].x-after[0].x));by=std::max(by,std::abs(after[i].y-after[0].y));}
   Check(std::abs(ax-bx)<1e-8*std::max(1.,ax)&&std::abs(ay-by)<1e-8*std::max(1.,ay),"projected body dimensions invariant");}
 }
}
int wmain(int argc,wchar_t**argv){try{
 std::filesystem::path output=argv[1];std::filesystem::create_directories(output);
 Vec a{80,-240,150},b{-120,60,200},origin{14000,-3000,85};auto yaw=Yaw(.7);
 std::vector<Vec> body{{0,0,170},{-22,0,145},{22,0,145},{0,0,135},{0,0,90},{-9,5,3},{9,5,3}};
 for(double scale:{.5,.75,1.,1.25,1.5,2.}){
  auto start=CorrectCameraFraming(a,a,scale,0),finish=CorrectCameraFraming(b,a,scale,0);
  Check(Near(start,a*scale)&&Near(finish,b*scale),"full XYZ including baseline");
  Check(Near(CorrectCameraFraming(a,a,scale,0),start),"A B A returns exactly to A prime");
  for(int repeat=0;repeat<1000;++repeat)Check(Near(CorrectCameraFraming(a,b,scale,0),start),"no frame accumulation or anchor dependence at defaults");
  for(double height:{-30.,0.,30.})Check(Near(CorrectCameraFraming(b,a,scale,height)-finish,Vec{0,0,height}),"one unscaled manual offset");
  Check(Near(CorrectCameraFraming(b,a,scale,0,{125,100}),b*scale),"legacy distance cannot alter default source framing");
  Check(Near(CorrectCameraFraming(b,a,scale,0,{100,50}),(a+(b-a)*.5)*scale),"independent movement strength");
  Vec playerNow{-61000,170000,2000};auto previous=StageCameraPosition(origin,yaw,finish);Check(Near(previous+(playerNow-origin),StageCameraPosition(playerNow,yaw,finish)),"instant player/teleport follow once");
 }
 for(double bad:{0.,-1.,101.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})Check(Near(CorrectCameraFraming(b,a,bad,10),b+Vec{0,0,10}),"invalid scale fallback");
 Check(ResolveHeightScale(.75,.75,.75,.75).value==.75,"already combined world scale is not multiplied twice");
 for(double fov:{30.,45.,70.})for(double aspect:{4./3,16./9,21./9}){
  for(auto [camera,target]:{std::pair{Vec{0,-350,100},Vec{0,0,90}},std::pair{Vec{0,-55,170},Vec{0,0,170}},std::pair{Vec{0,-110,140},Vec{0,0,140}}}){
   auto rot=Look(target-camera,{0,0,1});Compare(camera,rot,body,origin,yaw,fov,aspect);
   if(camera.y==-55){auto feet=Project(body.back(),camera,rot,fov,aspect);Check(!feet.visible,"intentional face close-up excludes feet");++closeupCount;}
  }
  for(unsigned i=0;i<=72;++i){double t=i*3.141592653589793/36.;Vec camera{250*std::sin(t),-250*std::cos(t),100+20*std::sin(t)};Compare(camera,Look(Vec{0,0,110}-camera,{0,0,1}),body,origin,yaw,fov,aspect);}
 }
 // Runtime interpolation, music-time access, pause/reseek and close-spaced cut samples.
 CameraTrack track;track.duration=3;track.fov=45;track.samples={{0,a,Look(Vec{0,0,100}-a,{0,0,1}),45},{.999999,a,Look(Vec{0,0,100}-a,{0,0,1}),45},{1,b,Look(Vec{0,0,100}-b,{0,0,1}),45},{2,a,Look(Vec{0,0,100}-a,{0,0,1}),45},{3,b,Look(Vec{0,0,100}-b,{0,0,1}),45}};
 for(double t:{0.,.5,.999998,.999999,1.,1.000001,1.5,2.,2.5,3.,1.5,.5,2.,0.}){auto sample=track.At(t);Compare(sample.pos,track.Direction(sample.rot),body,origin,yaw,sample.fov,16./9);}
 auto source=ReadJson(std::filesystem::path(argv[2]));unsigned realFrames=0;
 for(auto&row:source.at("frames")){auto camera=ReadVec(row.at("camera"));auto rot=ReadQuat(row.at("rotation"));std::vector<Vec> points;for(auto&point:row.at("points"))points.push_back(ReadVec(point));Compare(camera,rot,points,origin,yaw,45,16./9,true);++realFrames;}
 // Persistence failures preserve active values and never rewrite unrelated settings.
 auto settings=output/"CameraFraming.json",other=output/"UserSettings.json";{std::ofstream f(other);f<<"user-sentinel";}
 CameraFramingStore store(settings);Check(store.Load().empty()&&store.Current()==CameraFramingTuning{},"missing settings defaults");std::string error;
 Check(store.Save({125,75},error),"save independent settings");CameraFramingStore loaded(settings);Check(loaded.Load().empty()&&loaded.Current()==CameraFramingTuning{100,75},"saved settings roundtrip");
 Check(!loaded.Save({0,100},error)&&loaded.Current()==CameraFramingTuning{100,75},"invalid save keeps active values");
 for(auto bytes:{"{\"schemaVersion\":2}","{\"schemaVersion\":1,\"distancePercent\":true,\"motionPercent\":100}","{\"schemaVersion\":1,\"distancePercent\":100,\"motionPercent\":-1}","broken"}){std::ofstream(settings)<<bytes;Check(!loaded.Load().empty()&&loaded.Current()==CameraFramingTuning{},"bad settings default without rewriting file");}
 auto failedPath=output/"directory";std::filesystem::create_directories(failedPath);CameraFramingStore failed(failedPath);Check(!failed.Save({125,75},error)&&failed.Current()==CameraFramingTuning{},"failed atomic save preserves state");
 std::ifstream sentinel(other);std::string untouched;sentinel>>untouched;Check(untouched=="user-sentinel","hotkeys are not overwritten");
 Check(visibleCount>0&&hiddenCount>0&&closeupCount>0&&realFrames>0,"visible full-body and deliberately cropped/real fixtures exercised");
 Json report={{"passed",true},{"checks",checks},{"scales",{.5,.75,1.,1.25,1.5,2.}},{"maxVisibleNdcError",maxError},{"maxVisibleErrorPixelsAt4K",maxError*1920},{"maxOffscreenNdcError",maxOffscreenError},{"nearSingularProjectionsSkipped",singular},{"visibleLandmarkChecks",visibleCount},{"preservedHiddenLandmarkChecks",hiddenCount},{"realHkxFrames",realFrames},{"realHkxVisibleLandmarkChecks",realVisible},{"projection","ideal perspective; unchanged source rotation/FOV"},{"gameTested",false},{"nearFarClippingTested",false}};
 std::ofstream(output/"validation.json")<<report.dump(2);std::cout<<"PASS "<<checks<<" source framing, six scales, motion, cuts and settings checks; max NDC error="<<maxError<<"\n";
 }catch(const std::exception&e){std::cerr<<"FAILED: "<<e.what();return 1;}}
