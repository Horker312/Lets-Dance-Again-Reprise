#pragma once
// ABI declarations for the public interfaces. See CREDITS.md for upstream URLs.
// These classes have no virtual destructor because neither public API has one.
namespace interop {
using Handle=SKSE::PluginHandle;
enum class Result:std::uint8_t{OK,NotOwner,MustKeep,AlreadyGiven,AlreadyTaken,BadThread};
inline bool Granted(Result r){return r==Result::OK||r==Result::AlreadyGiven;}
struct TDM1 {
 virtual unsigned long Thread()const noexcept=0;
 virtual bool Directional()const noexcept=0;
 virtual bool Locked()const noexcept=0;
 virtual RE::ActorHandle Target()const noexcept=0;
 virtual Result DisableDirectional(Handle)noexcept=0;
 virtual Result DisableHead(Handle)noexcept=0;
 virtual Handle DirectionalOwner()const noexcept=0;
 virtual Handle HeadOwner()const noexcept=0;
 virtual Result ReleaseDirectional(Handle)noexcept=0;
 virtual Result ReleaseHead(Handle)noexcept=0;
};
struct SmoothCam1 {
 virtual unsigned long Thread()const noexcept=0;
 virtual Result RequestCamera(Handle)noexcept=0;
 virtual Result RequestCrosshair(Handle,bool=true)noexcept=0;
 virtual Result RequestStealth(Handle,bool=true)noexcept=0;
 virtual Handle CameraOwner()const noexcept=0;
 virtual Handle CrosshairOwner()const noexcept=0;
 virtual Handle StealthOwner()const noexcept=0;
 virtual Result ReleaseCamera(Handle)noexcept=0;
};
inline TDM1* tdm=nullptr;
inline SmoothCam1* camera=nullptr;
inline bool ownMovement=false,ownHead=false,ownCamera=false;
inline bool cameraPresent=false,tdmPresent=false;
inline void Release(){auto h=SKSE::GetPluginHandle();
 if(ownCamera&&camera)camera->ReleaseCamera(h);
 if(ownHead&&tdm)tdm->ReleaseHead(h);
 if(ownMovement&&tdm)tdm->ReleaseDirectional(h);
 ownCamera=ownHead=ownMovement=false;
}
inline bool Acquire(bool useCamera=true){auto h=SKSE::GetPluginHandle();
 if((useCamera&&cameraPresent&&!camera)||(tdmPresent&&!tdm))return false;
 if((tdm&&tdm->Thread()!=GetCurrentThreadId())||(useCamera&&camera&&camera->Thread()!=GetCurrentThreadId())){
  SKSE::log::warn("Camera/movement ownership request rejected on non-owner thread");return false;}
 if(tdm){if(tdm->Locked())return false;
  ownMovement=Granted(tdm->DisableDirectional(h));if(!ownMovement)return false;
  ownHead=Granted(tdm->DisableHead(h));if(!ownHead){Release();return false;}}
 if(useCamera&&camera){ownCamera=Granted(camera->RequestCamera(h));if(!ownCamera){Release();return false;}}
 return true;
}
inline void InitTDM(){auto m=GetModuleHandleW(L"TrueDirectionalMovement.dll");tdmPresent=m!=nullptr;if(!m)return;
 using Request=void*(*)(std::uint8_t);auto fn=reinterpret_cast<Request>(GetProcAddress(m,"RequestPluginAPI"));if(fn)tdm=static_cast<TDM1*>(fn(0));
}
inline void RegisterCamera(){cameraPresent=GetModuleHandleW(L"SmoothCam.dll")!=nullptr;if(!cameraPresent)return;
 SKSE::GetMessagingInterface()->RegisterListener("SmoothCam",[](SKSE::MessagingInterface::Message*m){
  struct Response{std::uint8_t type;void*data;};struct Container{void*instance;std::uint8_t version;};
  if(m->type!=0||m->dataLen!=sizeof(Response)||!m->data)return;
  auto*r=static_cast<Response*>(m->data);if(r->type!=1||!r->data)return;
  auto*c=static_cast<Container*>(r->data);if(c->version==0)camera=static_cast<SmoothCam1*>(c->instance);
 });
}
inline void RequestCamera(){if(!cameraPresent)return;
 struct Request{std::uint8_t version=0;}request;
 struct Command{std::uint32_t header=0x9007CA50;std::uint8_t type=0;void*data;}command;
 command.data=&request;SKSE::GetMessagingInterface()->Dispatch(0,&command,sizeof(command),"SmoothCam");
}
static_assert(sizeof(Result)==1);
static_assert(sizeof(Handle)==4);
}
