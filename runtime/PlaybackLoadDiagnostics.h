#pragma once
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <array>
#include <string>

namespace DanceStudio {
// Read-only snapshot taken in the clip's animation callback. No engine pointers
// escape the callback. Never request/retry a load or change the binding here.
struct ClipLoadObservation {
 std::uint64_t userData{};
 std::uint16_t index{};
 unsigned mode{},flags{};
 int names{},bindings{};
 bool entry{};
 std::string registeredPath;
};
inline ClipLoadObservation ObserveClipLoad(RE::hkbClipGenerator* clip,const RE::hkbContext& context){
 ClipLoadObservation result;
 result.userData=clip->userData;result.index=clip->animationBindingIndex;
 result.mode=static_cast<unsigned>(*clip->mode);result.flags=clip->flags;
 auto* character=context.character;
 if(!character)return result;
 if(auto setup=character->setup;setup&&setup->data&&setup->data->stringData){
  const auto& names=setup->data->stringData->animationNames;
  result.names=names.size();
  if(result.names>0&&result.names<=65536&&result.index<result.names){
   if(const auto* path=names[result.index].data())result.registeredPath=path;
  }
 }
 // Same accessor/IDs used by OAR's CreateSynchronizedClips. It selects the
 // per-character binding set or setup set; do not assume setup alone is live.
 using GetBindingSet=RE::hkbAnimationBindingSet*(*)(RE::hkbCharacter*);
 static REL::Relocation<GetBindingSet> getBindingSet{RELOCATION_ID(57886,58459)};
 if(const auto* set=getBindingSet(character)){
  result.bindings=set->bindings.size();
  if(result.bindings>0&&result.bindings<=65536&&result.index<result.bindings)
   result.entry=set->bindings[result.index]!=nullptr;
 }
 return result;
}

// BSResource uses Data-relative paths. An ordinary filesystem/hash check is
// not evidence that Skyrim's resource manager can open the same loose file.
// Probe a header once per preparation, on the main thread, never per frame.
inline void ProbeAnimationResource(std::string path){
 if(path.empty()||path.size()>512)return;
 for(auto& c:path)if(c=='/')c='\\';
 if(path.size()>5&&_strnicmp(path.c_str(),"data\\",5)==0)path.erase(0,5);
 if(path.size()<7||_strnicmp(path.c_str(),"meshes\\",7)!=0)return;
 RE::BSResourceNiBinaryStream stream(path);
 const bool opened=stream.good();
 std::array<unsigned char,64> header{};
 const bool read=opened&&stream.read(header.data(),static_cast<std::uint32_t>(header.size()));
 const bool magic=read&&header[0]==0x57&&header[1]==0xE0&&header[2]==0xE0&&header[3]==0x57&&
  header[4]==0x10&&header[5]==0xC0&&header[6]==0xC0&&header[7]==0x10;
 SKSE::log::info("Loader resource probe: path='{}' opened={} headerRead={} packfileMagic={} pointerSize={} littleEndian={} resourceError={}",
  path,opened,read,magic,read?header[16]:0,read?header[17]:0,static_cast<int>(stream.lastError));
}
}
