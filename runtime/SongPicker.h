#pragma once
#include "ProductInfo.h"
#include "TrackData.h"
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
 void(*width)(float){};Window* window{};Begin begin{};End end{};Text text{};Button button{};Combo combo{};
 std::atomic_bool ready{},refresh{};std::atomic<std::uint64_t> cycle{};std::atomic<std::shared_ptr<const PickerList>> list;
 std::atomic<std::shared_ptr<const PickerChoice>> choice;std::atomic<std::shared_ptr<const std::string>> preferred;
 std::shared_ptr<const PickerList> displayed;std::vector<std::string> labels,cameraLabels;std::vector<const char*> items,cameraItems;int selected{},selectedCamera{};std::string rememberedDance,rememberedCamera;
 static inline SongPicker* instance{};
 template<class T>static T Fn(HMODULE m,const char*n){return reinterpret_cast<T>(GetProcAddress(m,n));}
 static void __stdcall Draw()noexcept{if(instance)instance->Render();}
 void Render()noexcept{
  bool began=false;try{
   auto session=cycle.load();bool open=window->IsOpen.load();bool visible=begin("LDA:R - 곡 선택##DanceStudioPicker",&open,64);began=true;
   if(!open){window->IsOpen=false;choice.store(nullptr);}
   if(visible&&open){auto snapshot=list.load();if(snapshot!=displayed){
     std::string old=displayed&&selected>0&&selected<=static_cast<int>(displayed->dances.size())?displayed->dances[selected-1].id:rememberedDance;std::string oldCam=displayed&&selectedCamera>0&&selectedCamera<=static_cast<int>(displayed->cameras.size())?displayed->cameras[selectedCamera-1].id:rememberedCamera;
     rememberedDance=old;rememberedCamera=oldCam;displayed=snapshot;labels={"없음"};cameraLabels={"자유시점"};items.clear();cameraItems.clear();selected=selectedCamera=0;
     if(displayed){for(auto&s:displayed->dances){labels.push_back(s.title+" ["+s.id+"]");if(s.id==old)selected=static_cast<int>(labels.size()-1);}for(auto&s:displayed->cameras){cameraLabels.push_back(s.title+" ["+s.id+"]");if(s.id==oldCam)selectedCamera=static_cast<int>(cameraLabels.size()-1);}}
     for(auto&s:labels)items.push_back(s.c_str());for(auto&s:cameraLabels)cameraItems.push_back(s.c_str());
    }
    if(displayed&&!displayed->loading)if(auto wanted=preferred.exchange(nullptr)){for(size_t i=0;i<displayed->dances.size();++i)if(displayed->dances[i].id==*wanted)selected=static_cast<int>(i+1);}
    text("음악·재생 길이: 춤 우선 / 춤 없음이면 카메라 기준",nullptr);text("짧은 카메라는 끝 자세 유지, 긴 카메라는 음악 끝에서 종료",nullptr);
    if(!displayed||displayed->loading)text("목록을 읽고 있습니다...",nullptr);else if(!displayed->error.empty())text(displayed->error.c_str(),nullptr);else{
     if(width)width(520);if(combo("춤##DanceStudioSongs",&selected,items.data(),static_cast<int>(items.size()),16))rememberedDance=selected>0?displayed->dances[selected-1].id:"";if(width)width(520);if(combo("카메라##DanceStudioCameras",&selectedCamera,cameraItems.data(),static_cast<int>(cameraItems.size()),16))rememberedCamera=selectedCamera>0?displayed->cameras[selectedCamera-1].id:"";
     if(selected>0||selectedCamera>0){const auto&master=selected>0?displayed->dances[selected-1]:displayed->cameras[selectedCamera-1];auto info="기준 음악: "+master.title+" / "+std::to_string(master.duration)+"초";text(info.c_str(),nullptr);}
     if(selected==0&&selectedCamera==0)text("춤 또는 카메라를 하나 이상 선택하세요.",nullptr);
     if(button("선택한 트랙 준비##DanceStudioPick")&&window->IsOpen.load()&&cycle.load()==session&&(selected>0||selectedCamera>0)&&selected>=0&&selectedCamera>=0&&selected<=static_cast<int>(displayed->dances.size())&&selectedCamera<=static_cast<int>(displayed->cameras.size())){PickerChoice result;result.generation=displayed->generation;result.cycle=session;if(selected>0){result.id=displayed->dances[selected-1].id;result.token=displayed->dances[selected-1].token;}if(selectedCamera>0)result.cameraId=displayed->cameras[selectedCamera-1].id;choice.store(std::make_shared<PickerChoice>(std::move(result)));window->IsOpen=false;}
    }
    if(button("목록 다시 검색##DanceStudioRefresh"))refresh=true;
    if(button("취소##DanceStudioCancelPicker")){window->IsOpen=false;choice.store(nullptr);}
   }
   end();began=false;
  }catch(...){if(began)end();}
 }
public:
 void Publish(std::shared_ptr<const PickerList> value){auto current=list.load();while(!current||value->generation>=current->generation){if(list.compare_exchange_weak(current,value))return;}}
 std::shared_ptr<const PickerChoice> Take(){auto value=choice.exchange(nullptr);return value&&value->cycle==cycle.load()?value:nullptr;}
 bool TakeRefresh(){return refresh.exchange(false);}
 bool Open(const std::string& id={}){if(!ready.load(std::memory_order_acquire))return false;cycle.fetch_add(1);choice.store(nullptr);preferred.store(id.empty()?nullptr:std::make_shared<const std::string>(id));window->BlockUserInput=true;window->IsOpen=true;return true;}
 void Cancel(){cycle.fetch_add(1);choice.store(nullptr);preferred.store(nullptr);refresh=false;if(ready.load(std::memory_order_acquire))window->IsOpen=false;}
 bool Register(std::string&error){
  if(ready.load())return true;auto m=GetModuleHandleW(L"SKSEMenuFramework.dll");if(!m){error="Song picker requires SKSE Menu Framework";return false;}
  auto api=Fn<unsigned(*)()>(m,"GetMenuFrameworkAPIVersion");
  if(!api||api()!=1){error="Song picker requires SKSE Menu Framework API 1";return false;}
  width=Fn<void(*)(float)>(m,"igSetNextItemWidth");auto add=Fn<Window*(*)(Callback)>(m,"AddWindow");begin=Fn<Begin>(m,"igBegin");end=Fn<End>(m,"igEnd");text=Fn<Text>(m,"igTextUnformatted");button=Fn<Button>(m,"igSmallButton");combo=Fn<Combo>(m,"igCombo_Str_arr");
  if(!add||!begin||!end||!text||!button||!combo){error="Song picker exports unavailable";return false;}
  instance=this;window=add(Draw);if(!window){error="Song picker window creation failed";return false;}ready.store(true,std::memory_order_release);return true;
 }
};
}
