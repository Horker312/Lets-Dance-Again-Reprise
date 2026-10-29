#pragma once
#include "Localization.h"
#include "ProductInfo.h"
#include "TrackData.h"
#include "CameraFramingSettings.h"
#include <Windows.h>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace DanceStudio {
struct PickerSong {std::string id,title;unsigned token{};double duration{};};
struct PickerList {std::uint64_t generation{};std::vector<PickerSong> dances,cameras;bool loading{};std::string error;};
inline void FillPickerTracks(PickerList&out,const std::vector<Song>&songs){out.dances.clear();out.cameras.clear();for(auto&s:songs){if(s.HasDance())out.dances.push_back({s.id,s.title,s.token,s.duration});if(s.HasCamera())out.cameras.push_back({s.id,s.title,s.token,s.duration});}auto order=[](const PickerSong&a,const PickerSong&b){if(a.title!=b.title)return a.title<b.title;return a.id<b.id;};std::sort(out.dances.begin(),out.dances.end(),order);std::sort(out.cameras.begin(),out.cameras.end(),order);}
struct PickerChoice {std::uint64_t generation{},cycle{};std::string id;unsigned token{};std::string cameraId;};
class SongPicker {
 struct Window {std::atomic_bool IsOpen{false},BlockUserInput{true};};
 using Callback=void(__stdcall*)();using Begin=bool(*)(const char*,bool*,int);using End=void(*)();
 using Text=void(*)(const char*,const char*);using Button=bool(*)(const char*);using Combo=bool(*)(const char*,int*,const char*const[],int,int);
 using Checkbox=bool(*)(const char*,bool*);
 void(*width)(float){};Window* window{};Begin begin{};End end{};Text text{};Button button{};Combo combo{};Checkbox checkbox{};
 CameraFramingStore* framing{};std::string framingError;
 std::atomic_bool ready{},refresh{};std::atomic<std::uint64_t> cycle{};std::atomic<std::shared_ptr<const PickerList>> list;
 std::atomic<std::shared_ptr<const PickerChoice>> choice;std::atomic<std::shared_ptr<const std::string>> preferred;
 std::shared_ptr<const PickerList> displayed;std::vector<std::string> labels,cameraLabels;std::vector<const char*> items,cameraItems;int selected{},selectedCamera{};std::string rememberedDance,rememberedCamera;unsigned localeRevision=~0u;
 static inline SongPicker* instance{};
 template<class T>static T Fn(HMODULE m,const char*n){return reinterpret_cast<T>(GetProcAddress(m,n));}
 static void __stdcall Draw()noexcept{if(instance)instance->Render();}
 void Render()noexcept{
  bool began=false;try{
   auto session=cycle.load();bool open=window->IsOpen.load();bool visible=begin(L10n::T("r0023"),&open,64);began=true;
   if(!open){window->IsOpen=false;choice.store(nullptr);}
   if(visible&&open){auto snapshot=list.load();if(snapshot!=displayed||localeRevision!=L10n::revision.load()){localeRevision=L10n::revision.load();
     std::string old=displayed&&selected>0&&selected<=static_cast<int>(displayed->dances.size())?displayed->dances[selected-1].id:rememberedDance;std::string oldCam=displayed&&selectedCamera>0&&selectedCamera<=static_cast<int>(displayed->cameras.size())?displayed->cameras[selectedCamera-1].id:rememberedCamera;
     rememberedDance=old;rememberedCamera=oldCam;displayed=snapshot;labels={L10n::T("r0024")};cameraLabels={L10n::T("r0025")};items.clear();cameraItems.clear();selected=selectedCamera=0;
     if(displayed){for(auto&s:displayed->dances){labels.push_back(s.title+" ["+s.id+"]");if(s.id==old)selected=static_cast<int>(labels.size()-1);}for(auto&s:displayed->cameras){cameraLabels.push_back(s.title+" ["+s.id+"]");if(s.id==oldCam)selectedCamera=static_cast<int>(cameraLabels.size()-1);}}
     for(auto&s:labels)items.push_back(s.c_str());for(auto&s:cameraLabels)cameraItems.push_back(s.c_str());
    }
    if(displayed&&!displayed->loading)if(auto wanted=preferred.exchange(nullptr)){for(size_t i=0;i<displayed->dances.size();++i)if(displayed->dances[i].id==*wanted)selected=static_cast<int>(i+1);}
    text(L10n::T("r0026"),nullptr);text(L10n::T("r0027"),nullptr);
    if(!displayed||displayed->loading)text(L10n::T("r0028"),nullptr);else if(!displayed->error.empty())text(displayed->error.c_str(),nullptr);else{
     if(width)width(520);if(combo(L10n::T("r0029"),&selected,items.data(),static_cast<int>(items.size()),16))rememberedDance=selected>0?displayed->dances[selected-1].id:"";if(width)width(520);if(combo(L10n::T("r0030"),&selectedCamera,cameraItems.data(),static_cast<int>(cameraItems.size()),16))rememberedCamera=selectedCamera>0?displayed->cameras[selectedCamera-1].id:"";
     if(framing){auto tuning=framing->Current();bool enabled=tuning.scaleEnabled;if(checkbox(L10n::T("r0161"),&enabled)){tuning.scaleEnabled=enabled;framing->Save(tuning,framingError);}text(L10n::T("r0162"),nullptr);if(!framingError.empty())text(framingError.c_str(),nullptr);}
     text(L10n::T("r0163"),nullptr);
     if(selected>0||selectedCamera>0){const auto&master=selected>0?displayed->dances[selected-1]:displayed->cameras[selectedCamera-1];auto info=L10n::T("r0031")+master.title+" / "+std::to_string(master.duration)+L10n::T("r0032");text(info.c_str(),nullptr);}
     if(selected==0&&selectedCamera==0)text(L10n::T("r0033"),nullptr);
     if(button(L10n::T("r0034"))&&window->IsOpen.load()&&cycle.load()==session&&(selected>0||selectedCamera>0)&&selected>=0&&selectedCamera>=0&&selected<=static_cast<int>(displayed->dances.size())&&selectedCamera<=static_cast<int>(displayed->cameras.size())){PickerChoice result;result.generation=displayed->generation;result.cycle=session;if(selected>0){result.id=displayed->dances[selected-1].id;result.token=displayed->dances[selected-1].token;}if(selectedCamera>0)result.cameraId=displayed->cameras[selectedCamera-1].id;choice.store(std::make_shared<PickerChoice>(std::move(result)));window->IsOpen=false;}
    }
    if(button(L10n::T("r0035")))refresh=true;
    if(button(L10n::T("r0036"))){window->IsOpen=false;choice.store(nullptr);}
   }
   end();began=false;
  }catch(...){if(began)end();}
 }
public:
 explicit SongPicker(CameraFramingStore* store=nullptr):framing(store){}
 void Publish(std::shared_ptr<const PickerList> value){auto current=list.load();while(!current||value->generation>=current->generation){if(list.compare_exchange_weak(current,value))return;}}
 std::shared_ptr<const PickerChoice> Take(){auto value=choice.exchange(nullptr);return value&&value->cycle==cycle.load()?value:nullptr;}
 bool TakeRefresh(){return refresh.exchange(false);}
 bool Open(const std::string& id={}){if(!ready.load(std::memory_order_acquire))return false;cycle.fetch_add(1);choice.store(nullptr);preferred.store(id.empty()?nullptr:std::make_shared<const std::string>(id));window->BlockUserInput=true;window->IsOpen=true;return true;}
 void Cancel(){cycle.fetch_add(1);choice.store(nullptr);preferred.store(nullptr);refresh=false;if(ready.load(std::memory_order_acquire))window->IsOpen=false;}
 bool Register(std::string&error){
  if(ready.load())return true;auto m=GetModuleHandleW(L"SKSEMenuFramework.dll");if(!m){error=L10n::T("r0145");return false;}
  auto api=Fn<unsigned(*)()>(m,"GetMenuFrameworkAPIVersion");
  if(!api||api()!=1){error=L10n::T("r0146");return false;}
  width=Fn<void(*)(float)>(m,"igSetNextItemWidth");auto add=Fn<Window*(*)(Callback)>(m,"AddWindow");begin=Fn<Begin>(m,"igBegin");end=Fn<End>(m,"igEnd");text=Fn<Text>(m,"igTextUnformatted");button=Fn<Button>(m,"igSmallButton");combo=Fn<Combo>(m,"igCombo_Str_arr");
  checkbox=Fn<Checkbox>(m,"igCheckbox");
  if(!add||!begin||!end||!text||!button||!combo||!checkbox){error=L10n::T("r0147");return false;}
  instance=this;window=add(Draw);if(!window){error=L10n::T("r0148");return false;}ready.store(true,std::memory_order_release);return true;
 }
};
}
