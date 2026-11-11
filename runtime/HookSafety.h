#pragma once
#include <Windows.h>
#include <array>
#include <cstdint>
#include <cstring>
namespace DanceStudio {
inline bool PatchMemory(std::uintptr_t at,const void*bytes,size_t size){DWORD protect{};if(!VirtualProtect(reinterpret_cast<void*>(at),size,PAGE_EXECUTE_READWRITE,&protect))return false;SIZE_T written{};bool ok=WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),bytes,size,&written)&&written==size;FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(at),size);DWORD unused{};return VirtualProtect(reinterpret_cast<void*>(at),size,protect,&unused)&&ok;}
inline bool ReadableRange(std::uintptr_t at,size_t size){MEMORY_BASIC_INFORMATION m{};return at&&size&&at+size>=at&&VirtualQuery(reinterpret_cast<void*>(at),&m,sizeof(m))&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&at+size<=reinterpret_cast<std::uintptr_t>(m.BaseAddress)+m.RegionSize;}
struct CallPatch {std::uintptr_t site{},old{};std::array<unsigned char,5> before{},after{};
 bool Read(){if(!ReadableRange(site,5))return false;std::memcpy(before.data(),reinterpret_cast<void*>(site),5);if(before[0]!=0xE8)return false;std::int32_t displacement{};std::memcpy(&displacement,before.data()+1,4);old=site+5+displacement;return true;}
 bool Prepare(std::uintptr_t stub,std::uintptr_t hook){auto difference=static_cast<std::int64_t>(stub)-static_cast<std::int64_t>(site+5);if(difference<INT32_MIN||difference>INT32_MAX)return false;unsigned char code[14]={0xFF,0x25,0,0,0,0};std::memcpy(code+6,&hook,8);std::memcpy(reinterpret_cast<void*>(stub),code,14);FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(stub),14);after[0]=0xE8;auto relative=static_cast<std::int32_t>(difference);std::memcpy(after.data()+1,&relative,4);return true;}
 bool Unchanged()const{return ReadableRange(site,5)&&!std::memcmp(reinterpret_cast<void*>(site),before.data(),5);}
};
inline bool InstallCallPair(CallPatch&a,CallPatch&b){if(!a.Unchanged()||!b.Unchanged())return false;DWORD pa{},pb{};if(!VirtualProtect(reinterpret_cast<void*>(a.site),5,PAGE_EXECUTE_READWRITE,&pa))return false;if(!VirtualProtect(reinterpret_cast<void*>(b.site),5,PAGE_EXECUTE_READWRITE,&pb)){DWORD unused{};VirtualProtect(reinterpret_cast<void*>(a.site),5,pa,&unused);return false;}bool ok=PatchMemory(a.site,a.after.data(),5)&&PatchMemory(b.site,b.after.data(),5);if(!ok){PatchMemory(a.site,a.before.data(),5);PatchMemory(b.site,b.before.data(),5);}DWORD unused{};bool restoredB=VirtualProtect(reinterpret_cast<void*>(b.site),5,pb,&unused);bool restoredA=VirtualProtect(reinterpret_cast<void*>(a.site),5,pa,&unused);if(ok&&(!restoredA||!restoredB)){PatchMemory(a.site,a.before.data(),5);PatchMemory(b.site,b.before.data(),5);}return ok&&restoredA&&restoredB;}
}
