#pragma once
#include "CameraStage.h"
#include <atomic>
namespace DanceStudio {
// Independent of key bindings, packages and source HKX. No file reads in playback.
class CameraFramingStore {
 std::filesystem::path path;
 std::atomic<std::uint64_t> active{Pack({})};
 static constexpr std::uint64_t Pack(CameraFramingTuning t){return t.distancePercent|(std::uint64_t(t.motionPercent)<<32)|(t.scaleEnabled?0:1ull<<63);}
public:
 explicit CameraFramingStore(std::filesystem::path p=L"Data/SKSE/Plugins/DanceStudio/CameraFraming.json"):path(std::move(p)){}
 CameraFramingTuning Current()const{auto v=active.load();return{unsigned(v&0xffffffff),unsigned((v>>32)&0x7fffffff),(v&(1ull<<63))==0};}
 std::string Load(){active=Pack({});try{
  if(!std::filesystem::exists(path))return{};
  auto j=ReadJson(path,4096);
  if(!j.is_object()||!j.at("schemaVersion").is_number_integer()||(j.at("schemaVersion")!=1&&j.at("schemaVersion")!=2))throw std::runtime_error(L10n::T("r0158"));
  auto percent=[&](const char*key){const auto&v=j.at(key);if(!v.is_number_integer()||v<0||v>400)throw std::runtime_error(L10n::T("r0159"));return v.get<unsigned>();};
  CameraFramingTuning t{percent("distancePercent"),percent("motionPercent")};if(j.contains("scaleEnabled")){if(!j.at("scaleEnabled").is_boolean())throw std::runtime_error(L10n::T("r0159"));t.scaleEnabled=j.at("scaleEnabled").get<bool>();}if(!ValidCameraFraming(t))throw std::runtime_error(L10n::T("r0159"));bool migrated=t.distancePercent!=100||(j.at("schemaVersion")==1&&t.motionPercent!=100);if(migrated){t.distancePercent=t.motionPercent=100;}active=Pack(t);return migrated?L10n::T("r0167"):"";
 }catch(const std::exception&e){return std::string(L10n::T("r0160"))+e.what();}}
 bool Save(CameraFramingTuning t,std::string&error){
  error.clear();if(!ValidCameraFraming(t)){error=L10n::T("r0159");return false;}
  t.distancePercent=100;
  std::filesystem::path temp;HANDLE file=INVALID_HANDLE_VALUE;bool owned=false;
  try{
   if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
   auto bytes=Json{{"schemaVersion",2},{"distancePercent",t.distancePercent},{"motionPercent",t.motionPercent},{"scaleEnabled",t.scaleEnabled}}.dump(2)+"\n";
   static std::atomic<unsigned> serial{};
   for(unsigned attempt=0;attempt<32;++attempt){temp=path;temp+=L".tmp."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(serial.fetch_add(1));file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file!=INVALID_HANDLE_VALUE){owned=true;break;}if(GetLastError()!=ERROR_FILE_EXISTS)throw std::runtime_error(L10n::T("r0047"));}
   if(file==INVALID_HANDLE_VALUE)throw std::runtime_error(L10n::T("r0048"));
   DWORD wrote{};if(!WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&wrote,nullptr)||wrote!=bytes.size()||!FlushFileBuffers(file))throw std::runtime_error(L10n::T("r0049"));
   CloseHandle(file);file=INVALID_HANDLE_VALUE;
   if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error(L10n::T("r0050"));
   owned=false;active=Pack(t);return true;
  }catch(const std::exception&e){if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);if(owned)DeleteFileW(temp.c_str());error=e.what();return false;}
 }
};
}
