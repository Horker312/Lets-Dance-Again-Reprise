#pragma once
#include "Localization.h"
#include "ActorWindowLayout.h"
#include <atomic>
#include <memory>
#include <array>
namespace DanceStudio {
enum class ActorAction{Open,Assign,Place,Remove,Camera,Player,Back};
struct ActorRequest{ActorAction action{};unsigned slot{};std::uint64_t revision{};};
struct ActorList {std::array<std::string,5> labels;std::array<bool,5> assigned{};bool hasPlayer{};unsigned camera{};std::uint64_t revision{};};
// Render callbacks never touch live game objects. Requests are validated and
// resolved on the engine update thread against the published roster revision.
class ActorManager {
 struct Window{std::atomic_bool IsOpen{false},BlockUserInput{true};};
 std::atomic_bool desiredOpen{};Window*window{};bool(*begin)(const char*,bool*,int){};void(*end)(){};void(*text)(const char*,const char*){};bool(*button)(const char*){};bool(*combo)(const char*,int*,const char*const[],int,int){};
 struct Vec2{float x,y;};
 void(*nextSize)(Vec2,int){};void(*nextPos)(Vec2,int,Vec2){};bool initialWidthSet{};
 static BOOL CALLBACK ClientWindow(HWND window,LPARAM data){DWORD pid{};GetWindowThreadProcessId(window,&pid);RECT client{};if(pid==GetCurrentProcessId()&&IsWindowVisible(window)&&GetClientRect(window,&client)){auto& width=*reinterpret_cast<float*>(data);width=std::max(width,float(client.right-client.left));}return TRUE;}
 void InitialWidth(){if(initialWidthSet||!nextSize||!nextPos)return;float clientWidth=0;EnumWindows(ClientWindow,reinterpret_cast<LPARAM>(&clientWidth));float width=ActorManagerInitialWidth(clientWidth);if(width<=0)return;nextSize({width,0},1);nextPos({16,32},1,{0,0});initialWidthSet=true;}
 std::atomic<std::shared_ptr<const ActorList>> list;std::atomic<std::shared_ptr<const ActorRequest>> request;int selected{};static inline ActorManager*instance{};
 static void __stdcall Draw()noexcept{if(instance)instance->Render();}
 void Render()noexcept{bool began=false;try{if(!desiredOpen.load()){window->IsOpen=false;return;}InitialWidth();bool open=true;bool visible=begin(L10n::T("aManage"),&open,0);began=true;if(!open)Hide();if(visible&&open&&desiredOpen.load())if(auto l=list.load()){
  std::array<const char*,5> labels;for(unsigned i=0;i<5;++i)labels[i]=l->labels[i].c_str();combo(L10n::T("aSlot"),&selected,labels.data(),5,5);selected=std::clamp(selected,0,4);
  auto send=[&](ActorAction a,unsigned slot){request.store(std::make_shared<ActorRequest>(ActorRequest{a,slot,l->revision}));};
  if(button(L10n::T("aAssign"))){send(ActorAction::Assign,selected);Hide();}
  if(!l->assigned[selected]&&!l->hasPlayer&&button(L10n::T("aPlayer")))send(ActorAction::Player,selected);
  if(l->assigned[selected]){if(button(L10n::T("aPlace"))){send(ActorAction::Place,selected);Hide();}if(button(L10n::T("aRemove")))send(ActorAction::Remove,selected);}
  std::vector<std::string> names;std::vector<unsigned> ids;for(unsigned i=0;i<5;++i)if(l->assigned[i]){names.push_back(l->labels[i]);ids.push_back(i);}std::vector<const char*> items;int anchor=0;for(unsigned i=0;i<ids.size();++i){items.push_back(names[i].c_str());if(ids[i]==l->camera)anchor=i;}
  if(!items.empty()&&combo(L10n::T("aCamera"),&anchor,items.data(),static_cast<int>(items.size()),5)&&anchor>=0&&anchor<static_cast<int>(ids.size()))send(ActorAction::Camera,ids[anchor]);
  if(button(L10n::T("aBack"))){send(ActorAction::Back,selected);Hide();}
 }end();began=false;}catch(...){if(began)try{end();}catch(...){}}}
public:
 void Publish(std::shared_ptr<const ActorList>l){list.store(std::move(l));}
 std::shared_ptr<const ActorRequest>Take(){return request.exchange(nullptr);}
 bool Open(){if(!window)return false;desiredOpen=true;window->BlockUserInput=true;window->IsOpen=true;return true;}
 void Hide(){desiredOpen=false;if(window){window->BlockUserInput=false;window->IsOpen=false;}}
 void Close(){Hide();request.store(nullptr);}
 bool Register(){if(window)return true;auto m=GetModuleHandleW(L"SKSEMenuFramework.dll");if(!m)return false;auto get=[&](const char*n){return GetProcAddress(m,n);};auto api=reinterpret_cast<unsigned(*)()>(get("GetMenuFrameworkAPIVersion"));if(!api||api()!=1)return false;
  nextSize=reinterpret_cast<decltype(nextSize)>(get("igSetNextWindowSize"));nextPos=reinterpret_cast<decltype(nextPos)>(get("igSetNextWindowPos"));

  begin=reinterpret_cast<decltype(begin)>(get("igBegin"));end=reinterpret_cast<decltype(end)>(get("igEnd"));text=reinterpret_cast<decltype(text)>(get("igTextUnformatted"));button=reinterpret_cast<decltype(button)>(get("igSmallButton"));combo=reinterpret_cast<decltype(combo)>(get("igCombo_Str_arr"));auto add=reinterpret_cast<Window*(*)(void(__stdcall*)())>(get("AddWindow"));if(!begin||!end||!text||!button||!combo||!add)return false;instance=this;window=add(Draw);return window!=nullptr;
 }
};
}
