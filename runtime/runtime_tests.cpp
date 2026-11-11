#include "TrackData.h"
#include "AudioDevice.h"
#include <iostream>
#include <thread>
#include <chrono>
using namespace DanceStudio;
unsigned checks{};
void Check(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(what);}
template<class F>void Reject(F f,const char* what){bool threw=false;try{f();}catch(...){threw=true;}Check(threw,what);}
int main(int argc,char**argv){try{
 for(int i=0;i<500;++i){double t=i*.013;Quat q=Yaw(t);Vec p{2,5,7};Check(Dot(q.Inv().Apply(q.Apply(p))-p,q.Inv().Apply(q.Apply(p))-p)<1e-20,"Quaternion inverse");auto r=Look(q.Apply({0,1,0}),q.Apply({0,0,1}));Check(Dot(r.Apply({0,1,0})-q.Apply({0,1,0}),r.Apply({0,1,0})-q.Apply({0,1,0}))<1e-20,"Camera +Y basis");auto mid=Slerp(Yaw(t),Yaw(t+.2),.5);Check(std::abs(Dot(mid.Apply({0,1,0}),Yaw(t+.1).Apply({0,1,0}))-1)<1e-10,"Slerp time interpolation");}
 Json c={{"schemaVersion",1},{"duration",2},{"fov",45},{"yawDegrees",0},{"forward","+Y"},{"samples",Json::array({{{"time",0},{"position",{0,1,2}},{"rotation",{0,0,0,1}}},{{"time",2},{"position",{2,3,4}},{"rotation",{0,0,0,-1}}}})}};
 auto track=CameraTrack::Read(c,2);Check(track.At(-1).pos.x==0&&track.At(100).pos.x==2,"Camera clamps endpoints");Check(track.At(1).pos.y==2,"Linear camera position");Check(std::abs(track.At(1).rot.w)==1,"Antipodal quaternion shortest path");
 for(std::string axis:{"+Y","-Y","+X","-X","+Z","-Z"}){auto v=c;v["forward"]=axis;auto t=CameraTrack::Read(v,2);auto direction=t.Direction(t.At(1).rot).Apply({0,1,0});Check(std::abs(Dot(direction,t.forward)-1)<1e-9,"Six source camera axes");}
 {auto j=c;j["samples"][1]["time"]=0;Reject([&]{CameraTrack::Read(j,2);},"Duplicate sample timestamp rejected");}
 {auto j=c;j["samples"][0]["rotation"]={0,0,0,0};Reject([&]{CameraTrack::Read(j,2);},"Zero quaternion rejected");}
 {auto j=c;j["samples"][0]["position"]={1,2};Reject([&]{CameraTrack::Read(j,2);},"Malformed position rejected");}
 {auto j=c;j["samples"][1]["time"]=1.9;Reject([&]{CameraTrack::Read(j,2);},"Camera coverage rejected");}
 {auto j=c;j["fov"]=180;Reject([&]{CameraTrack::Read(j,2);},"Bad FOV rejected");}
 {auto j=c;j["forward"]="bad";Reject([&]{CameraTrack::Read(j,2);},"Bad optical axis rejected");}
 auto base=std::filesystem::current_path();Reject([&]{RelativeFile(base,"../escape.wav");},"Parent traversal rejected");Reject([&]{RelativeFile(base,"C:/escape.wav");},"Absolute path rejected");Reject([&]{RelativeFile(base,"\\escape.wav");},"Root relative path rejected");Check(RelativeFile(base,"safe/file.wav").lexically_relative(base)==std::filesystem::path("safe/file.wav"),"Relative file accepted");
 std::vector<char> b(44+48000,0);auto put16=[&](size_t n,std::uint16_t v){b[n]=char(v);b[n+1]=char(v>>8);};auto put32=[&](size_t n,std::uint32_t v){put16(n,v&65535);put16(n+2,v>>16);};std::memcpy(b.data(),"RIFF",4);put32(4,static_cast<std::uint32_t>(b.size()-8));std::memcpy(b.data()+8,"WAVEfmt ",8);put32(16,16);put16(20,1);put16(22,1);put32(24,48000);put32(28,96000);put16(32,2);put16(34,16);std::memcpy(b.data()+36,"data",4);put32(40,48000);
 auto wave=ReadWave(b,.5);Check(wave.Frames()==24000&&wave.rate==48000,"PCM sample count");Reject([&]{ReadWave(b,.6);},"WAV duration mismatch rejected");{auto bad=b;bad[34]=24;Reject([&]{ReadWave(bad,.5);},"Non-PCM16 rejected");}{auto bad=b;bad.pop_back();Reject([&]{ReadWave(bad,.5);},"Truncated RIFF rejected");}{auto bad=b;bad[32]=4;Reject([&]{ReadWave(bad,.5);},"Block align mismatch rejected");}
 AudioDevice device;ApplyVolume(wave,0);device.Open(std::move(wave));std::this_thread::sleep_for(std::chrono::milliseconds(100));Check(device.Frames()<100,"Prepared audio stays paused");device.Pause(false);std::this_thread::sleep_for(std::chrono::milliseconds(130));auto first=device.Frames();Check(first>1000&&first<20000,"Native hardware sample clock advances");device.Pause(true);std::this_thread::sleep_for(std::chrono::milliseconds(40));auto held=device.Frames();std::this_thread::sleep_for(std::chrono::milliseconds(100));Check(device.Frames()<=held+480,"Menu pause holds the hardware clock");device.Pause(false);std::this_thread::sleep_for(std::chrono::milliseconds(90));Check(device.Frames()>held,"Audio resumes from held sample");device.Close();device.Close();Reject([&]{device.Frames();},"Closed device rejects clock requests");
 if(argc>1){auto song=ReadSong(std::filesystem::path(argv[1]));auto camera=CameraTrack::Read(ReadJson(RelativeFile(song.root,song.manifest.at("cameraSamples"))),song.duration);auto audio=ReadWave(ReadBytes(RelativeFile(song.root,song.manifest.at("music")),384ull*1024*1024),song.duration);Check(audio.Frames()>0,"Packaged Booo audio parsed by production code");for(int i=0;i<=1000;++i){auto sample=camera.At(song.duration*i/1000.);auto rot=camera.Direction(sample.rot);Check(std::isfinite(sample.pos.x+sample.pos.y+sample.pos.z+rot.w),"Packaged Booo camera sampling finite");}}
 std::cout<<"PASS "<<checks<<" production track/math/WAV validations and muted native audio-device clock/pause checks\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
