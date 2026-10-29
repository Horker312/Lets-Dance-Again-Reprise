#pragma once
#include <Windows.h>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <string>
#include <fstream>
#include <filesystem>
#include <atomic>
#include "LocalizationDefaults.h"
namespace DanceStudio::L10n {
inline std::atomic<unsigned> language{},revision{};
inline std::unordered_map<std::string,std::string> ko=fallback_ko,en=fallback_en;
inline std::filesystem::path settingsPath=L"Data/SKSE/Plugins/DanceStudio/Language.json";
inline const char* T(const char*key){const auto&map=language.load()==1?en:ko;auto it=map.find(key);return it==map.end()?key:it->second.c_str();}
inline void Init(){
 for(auto entry:{std::pair{"ko",&ko},std::pair{"en",&en}})try{auto path=std::filesystem::path(L"Data/SKSE/Plugins/DanceStudio/Locales")/(std::string(entry.first)+".json");if(std::filesystem::file_size(path)>1024*1024)continue;std::ifstream f(path);auto j=nlohmann::json::parse(f);for(auto&[key,value]:j.items())if(value.is_string()&&entry.second->contains(key))(*entry.second)[key]=value.get<std::string>();}catch(...){}
 try{if(std::filesystem::file_size(settingsPath)>4096)return;std::ifstream f(settingsPath);auto j=nlohmann::json::parse(f);language=j.value("language",std::string("ko"))=="en"?1:0;}catch(...){}
}
inline bool Save(unsigned value,std::string&error){if(value>1){error="Unsupported language";return false;}std::filesystem::path temp;try{std::filesystem::create_directories(settingsPath.parent_path());temp=settingsPath;temp+=L"."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64())+L".tmp";auto bytes=nlohmann::json{{"language",value?"en":"ko"}}.dump(2);{std::ofstream f(temp,std::ios::binary|std::ios::trunc);if(!f.write(bytes.data(),bytes.size()))throw std::runtime_error(T("r0049"));f.flush();if(!f)throw std::runtime_error(T("r0049"));}if(!MoveFileExW(temp.c_str(),settingsPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error(T("r0050"));language=value;revision.fetch_add(1);return true;}catch(const std::exception&e){if(!temp.empty()){std::error_code ec;std::filesystem::remove(temp,ec);}error=e.what();return false;}}
}
