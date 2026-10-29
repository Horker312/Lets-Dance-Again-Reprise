#pragma once
#include "HUDPolicy.h"
#include <array>
#include <mutex>
#include <atomic>
namespace DanceStudio {
// Filter drawing only. Never write movie visibility, alpha, menu flags or HUD settings.
// A mod may change its own HUD state while hidden; it resumes in that current state.
class HUDGate {
 using DisplayFn=void(*)(RE::GFxMovieView*);
 struct Hook {std::atomic<std::uintptr_t> table{};std::atomic<DisplayFn> original{};};
 std::array<Hook,8> hooks;std::mutex installLock;HUDPolicy policy;std::atomic_bool hidden{};
 static inline HUDGate* instance{};
 static void Display(RE::GFxMovieView* movie){
  auto*self=instance;if(!self||!movie)return;auto table=*reinterpret_cast<std::uintptr_t*>(movie);DisplayFn original{};
  for(auto&h:self->hooks)if(h.table.load(std::memory_order_acquire)==table){original=h.original.load();break;}
  if(!original)return;
  if(self->hidden.load()){auto*def=movie->GetMovieDef();auto*url=def?def->GetFileURL():nullptr;if(url&&self->policy.Matches(url))return;}
  original(movie);
 }
 static bool Loaded(RE::GFxMovieView* movie,RE::GFxValue*)noexcept{try{if(instance&&movie)instance->Install(movie);}catch(...){SKSE::log::error("HUD render filter installation failed");}return true;}
 void Install(RE::GFxMovieView* movie){std::scoped_lock guard(installLock);auto table=*reinterpret_cast<std::uintptr_t*>(movie);for(auto&h:hooks)if(h.table.load()==table)return;
  for(auto&h:hooks)if(!h.table.load()){REL::Relocation<std::uintptr_t> vtable{table};auto original=reinterpret_cast<DisplayFn>(reinterpret_cast<std::uintptr_t*>(table)[0x26]);if(original==Display)return;h.original=original;h.table.store(table,std::memory_order_release);vtable.write_vfunc(0x26,Display);return;}
  SKSE::log::warn("HUD filter: movie implementation limit reached; this movie remains visible");
 }
public:
 void Configure(HUDPolicy value){policy=std::move(value);}
 bool Register(){instance=this;auto*api=SKSE::GetScaleformInterface();return api&&api->Register(Loaded,"DanceStudioHUD");}
 void Toggle()noexcept{hidden.store(!hidden.load());}
 void Restore()noexcept{hidden=false;}
 bool Hidden()const noexcept{return hidden.load();}
};
}
