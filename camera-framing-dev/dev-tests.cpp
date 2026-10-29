#include "../runtime/CameraFramingSettings.h"
#include "../runtime/WheelFov.h"
#include <iostream>
using namespace DanceStudio;
int checks{};double screenError{};
void Check(bool v,const char*why){++checks;if(!v)throw std::runtime_error(why);}
bool Near(Vec a,Vec b){return Dot(a-b,a-b)<1e-14;}
Vec Screen(Vec point,Vec camera,Quat q,double fov){auto p=q.Inv().Apply(point-camera);auto h=std::tan(fov*3.141592653589793/360);return{p.x/(p.y*h),p.z/(p.y*h/(16./9)),p.y};}
Json Base(){return Json{{"schemaVersion",3},{"fovConvention","skyrimWorldDegrees"},{"duration",4},{"fov",45},{"yawDegrees",0},{"forward","+Y"},{"samples",Json::array()}};}
void Add(Json&j,double t,Vec p,bool cut=false){j["samples"].push_back(Json{{"time",t},{"position",{p.x,p.y,p.z}},{"rotation",{0,0,0,1}},{"cut",cut}});}
int wmain(int argc,wchar_t**argv){try{
 auto output=std::filesystem::path(argv[1]);std::filesystem::create_directories(output);
 Vec A{0,-300,100},B{200,-100,150},C{-100,-500,90};
 auto j=Base();Add(j,0,A);Add(j,1,B);Add(j,3,C);Add(j,4,A);auto track=CameraTrack::Read(j,4);
 auto legacy=j;legacy["schemaVersion"]=1;Check(CameraTrack::Read(legacy,4).samples.size()==4,"legacy camera schema remains readable without re-export");
 auto expected=((A+B)*.5+(B+C)+(C+A)*.5)*.25;Check(Near(track.movementPivot,expected),"time-weighted whole-path pivot, not first frame");Check(!Near(track.movementPivot,A),"actual trajectory mean differs from first key");
 for(double scale:{.5,.75,1.,1.25,1.5,2.}){
  SessionCameraScale session;session.Capture(scale);session.Capture(scale*1.7);Check(session.Ratio()==scale,"captured scale ignores animated changes");Check(session.Ratio(2)==scale*.5,"explicit reference scale ratio");
  auto initial=FrameCameraPosition({14000,-3000,50},Yaw(.7),A,track.movementPivot,session.Ratio(),0);
  for(auto t:{0.,1.,0.,1.,3.,4.,2.,.5,0.,4.}){auto s=track.At(t);auto relative=track.RelativePosition(s);auto world=FrameCameraPosition({14000,-3000,50},Yaw(.7),relative,track.movementPivot,session.Ratio(),0);Check(Near(world,Vec{14000,-3000,50}+Yaw(.7).Apply(s.pos*scale)),"default transform independent of history and mean pivot");if(t==0||t==4)Check(Near(world,initial),"A B C A returns to identical A prime");}
  for(unsigned strength:{0,50,100,150,200}){auto before=CorrectCameraFraming(track.At(1).pos,track.movementPivot,scale,0,{100,strength});auto after=CorrectCameraFraming(track.At(1).pos,track.movementPivot,scale,0,{100,strength});Check(Near(before,after),"non-default effect has no seek/segment anchor discontinuity");}
  auto camera=FrameCameraPosition({10,20,30},Yaw(.7),A,track.movementPivot,scale,25);auto relocated=FollowCameraPosition(camera,{10,20,30},Yaw(.7),{-50000,71000,80},Yaw(-.9));Check(Near(relocated,FrameCameraPosition({-50000,71000,80},Yaw(-.9),A,track.movementPivot,scale,25)),"translation and heading follow exactly once, world height unchanged");
  auto originalQ=Yaw(.7)*Look(Vec{0,0,100}-A,{0,0,1});auto newQ=FollowCameraRotation(originalQ,Yaw(.7),Yaw(-.9));Check(Near(newQ.Apply({0,1,0}),(Yaw(-.9)*Look(Vec{0,0,100}-A,{0,0,1})).Apply({0,1,0})),"camera orientation uses same basis delta as position");
  session.Reset();session.Capture(1.25);Check(session.Ratio()==1.25,"next session can capture a new user size");
 }
 auto explicitPivot=j;explicitPivot["movementPivot"]={4,5,6};Check(Near(CameraTrack::Read(explicitPivot,4).movementPivot,{4,5,6}),"explicit pivot metadata preferred");
 auto cuts=Base();Add(cuts,0,A);Add(cuts,1,B,true);Add(cuts,3,C);Add(cuts,4,A,true);auto cut=CameraTrack::Read(cuts,4);
 Check(Near(cut.At(.999999).pos,A)&&Near(cut.At(1).pos,B),"explicit cut has no invented bridge");Check(Near(cut.At(3.999999).pos,C)&&Near(cut.At(4).pos,A),"explicit return cut preserves timing");
 Check(!Near(track.At(.5).pos,A)&&Near(track.At(.5).pos,(A+B)*.5),"large displacement does not invent a cut");
 for(unsigned strength:{25,50,100,150}){auto x=CorrectCameraFraming(cut.At(1).pos,cut.movementPivot,1.5,0,{100,strength}),y=CorrectCameraFraming(cut.At(.99999).pos,cut.movementPivot,1.5,0,{100,strength});Check(Near(x-y,(B-A)*(1.5*strength/100.)),"non-default effect transforms cut endpoints with one stable pivot");}
 // Explicit source actor movement is subtracted only when the coordinate
 // contract says reference-world. Bending/COM movement never becomes a pivot.
 auto moving=Base();moving["coordinateSpace"]="reference-world";
 std::vector<Vec> landmarks{{0,0,170},{0,0,140},{0,0,90},{-9,5,0},{9,5,0}};
 for(int i=0;i<=4;++i){Vec reference{double(50*i),double(-30*i),5};auto rq=Yaw(i*.25);auto camera=reference+rq.Apply(A);moving["samples"].push_back(Json{{"time",i},{"position",{camera.x,camera.y,camera.z}},{"rotation",{rq.x,rq.y,rq.z,rq.w}},{"referencePosition",{reference.x,reference.y,reference.z}},{"referenceRotation",{rq.x,rq.y,rq.z,rq.w}}});}
 auto reference=CameraTrack::Read(moving,4);
 auto movingCutJson=moving;movingCutJson["samples"][1]["cut"]=true;auto movingCut=CameraTrack::Read(movingCutJson,4);Check(Near(movingCut.At(.5).pos,movingCut.At(0).pos),"camera cut holds camera before its boundary");Check(!Near(movingCut.At(.5).referencePosition,movingCut.At(0).referencePosition),"camera cut does not hold source actor movement");movingCutJson["samples"][1]["referenceCut"]=true;auto actorCut=CameraTrack::Read(movingCutJson,4);Check(Near(actorCut.At(.5).referencePosition,actorCut.At(0).referencePosition),"source actor jumps only with its own explicit cut marker");
 for(double t:{0.,.25,.5,1.,1.5,2.,2.75,3.,4.})for(double scale:{.5,.75,1.,1.25,1.5,2.}){
  auto s=reference.At(t);Vec actual{31000+t*17,-13000+t*40,80};auto basis=Yaw(-.4+t*.1);auto camera=FrameCameraPosition(actual,basis,reference.RelativePosition(s),reference.movementPivot,scale,0);auto q=Unit(basis*reference.Direction(s));
  for(auto bone:landmarks){auto authored=s.referencePosition+s.referenceRotation.Apply(bone);auto before=Screen(authored,s.pos,reference.Direction(s.rot),45);auto after=Screen(actual+basis.Apply(bone*scale),camera,q,45);auto error=std::max(std::abs(before.x-after.x),std::abs(before.y-after.y));screenError=std::max(screenError,error);Check(error<1e-8,"moving reference actor screen framing: no double translation/rotation");Check((std::abs(before.x)<=1&&std::abs(before.y)<=1&&before.z>0)==(std::abs(after.x)<=1&&std::abs(after.y)<=1&&after.z>0),"original crop retained for moving reference");}
 }
 auto malformed=moving;malformed["coordinateSpace"]="unknown";bool rejected=false;try{CameraTrack::Read(malformed,4);}catch(...){rejected=true;}Check(rejected,"ambiguous coordinate space rejected");
 malformed=moving;malformed["samples"][0].erase("referencePosition");rejected=false;try{CameraTrack::Read(malformed,4);}catch(...){rejected=true;}Check(rejected,"missing explicit source actor pose is not guessed");
 auto settings=output/"CameraFraming.json";{std::ofstream f(settings);f<<R"({"schemaVersion":1,"distancePercent":125,"motionPercent":50,"scaleEnabled":false})";}
 CameraFramingStore store(settings);Check(!store.Load().empty()&&store.Current()==CameraFramingTuning{100,100,false},"legacy non-default first-frame effects reset explicitly; checkbox retained");
 std::string error;Check(store.Save({125,75,false},error),"new effect settings save");CameraFramingStore reopened(settings);Check(reopened.Load().empty()&&reopened.Current()==CameraFramingTuning{100,75,false},"schema 2 retains movement strength and discards retired distance");
 Check(ReadJson(settings).at("schemaVersion")==2,"new effect policy recorded separately from product version");
 std::ofstream(output/"validation.json")<<Json{{"passed",true},{"checks",checks},{"maxMovingReferenceScreenError",screenError},{"gameTested",false},{"versionsChanged",false}}.dump(2);
 std::cout<<"PASS "<<checks<<" coordinate contracts, cut/pivot policy, capture, follow, migration and screen checks\n";
 }catch(const std::exception&e){std::cerr<<"FAILED: "<<e.what();return 1;}}
