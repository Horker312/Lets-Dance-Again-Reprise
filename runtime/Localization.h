#pragma once
#include <Windows.h>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <memory>
#include <algorithm>
#include "LocalizationDefaults.h"
namespace DanceStudio::L10n {
using Strings=std::unordered_map<std::string,std::string>;
struct Pack {std::string id,name;Strings strings;};
struct Catalog {std::vector<Pack> packs;std::vector<const char*> labels;std::vector<std::string> warnings;};
inline std::atomic<unsigned> language{},revision{};
inline std::filesystem::path settingsPath=L"Data/SKSE/Plugins/DanceStudio/Language.json";
inline std::filesystem::path externalPath=L"Data/SKSE/Plugins/DanceStudio/lang";
inline std::filesystem::path legacyPath=L"Data/SKSE/Plugins/DanceStudio/Locales";
inline Catalog initial{{{"ko","한국어",fallback_ko},{"en","English",fallback_en}},{}, {}};
inline std::atomic<const Catalog*> catalog{&initial};
inline std::unique_ptr<const Catalog> published;
inline std::once_flag initialized;
inline bool ValidText(const std::string&s,size_t maximum=32768){return !s.empty()&&s.size()<=maximum&&s.find('\0')==std::string::npos&&s.find_first_not_of(" \t\r\n")!=std::string::npos;}
inline bool ValidName(const std::string&s){return ValidText(s,128)&&std::none_of(s.begin(),s.end(),[](unsigned char c){return c<0x20||c==0x7f;});}
inline bool ValidId(const std::string&s){return s.size()>=2&&s.size()<=32&&s[0]>='a'&&s[0]<='z'&&std::all_of(s.begin(),s.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_';});}
inline bool JsonFile(const std::filesystem::path&path){auto ext=path.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),[](wchar_t c){return c>=L'A'&&c<=L'Z'?static_cast<wchar_t>(c+32):c;});return ext==L".json";}
inline std::string DisplayFile(const std::filesystem::path&path){try{auto bytes=path.filename().u8string();return std::string(bytes.begin(),bytes.end());}catch(...){return "<unrepresentable filename>";}}
inline bool Known(const std::string&key){return fallback_en.contains(key)||fallback_ko.contains(key);}
inline bool SafeTranslation(const std::string&key,const std::string&value){if(!Known(key)||!ValidText(value))return false;auto it=fallback_en.find(key);auto koIt=fallback_ko.find(key);const auto&source=it!=fallback_en.end()?it->second:koIt->second;auto id=source.find("##"),translated=value.find("##");return id==std::string::npos?translated==std::string::npos:translated!=std::string::npos&&source.substr(id)==value.substr(translated);}
inline nlohmann::json ReadBounded(const std::filesystem::path&path,size_t maximum){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Language file unavailable");std::string bytes(maximum+1,'\0');f.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));bytes.resize(static_cast<size_t>(f.gcount()));if(bytes.empty()||bytes.size()>maximum)throw std::runtime_error("Language file size limit");std::vector<std::unordered_set<std::string>> objects;return nlohmann::json::parse(bytes,[&](int depth,nlohmann::json::parse_event_t event,nlohmann::json& value){if(depth>24)throw std::runtime_error("Language JSON depth limit");using E=nlohmann::json::parse_event_t;if(event==E::object_start)objects.emplace_back();else if(event==E::object_end)objects.pop_back();else if(event==E::key&&!objects.back().insert(value.get<std::string>()).second)throw std::runtime_error("Duplicate language JSON key");return true;});}
inline const char* T(const char*key){const auto*data=catalog.load(std::memory_order_acquire);unsigned selected=language.load();if(selected>=data->packs.size())selected=0;for(unsigned index:{selected,1u,0u}){const auto&map=data->packs[index].strings;auto it=map.find(key);if(it!=map.end())return it->second.c_str();}return key;}
inline const std::vector<const char*>& LanguageItems(){return catalog.load(std::memory_order_acquire)->labels;}
inline std::string LanguageId(){auto*data=catalog.load(std::memory_order_acquire);auto n=language.load();return data->packs[n<data->packs.size()?n:0].id;}
inline void Init(){std::call_once(initialized,[]{
 auto data=std::make_unique<Catalog>();data->packs={{"ko","한국어",fallback_ko},{"en","English",fallback_en}};
 for(unsigned i=0;i<2;++i)try{auto j=ReadBounded(legacyPath/(data->packs[i].id+".json"),1024*1024);if(!j.is_object())continue;for(auto&[key,value]:j.items())if(Known(key)&&value.is_string()){auto text=value.get<std::string>();if(SafeTranslation(key,text))data->packs[i].strings[key]=std::move(text);}}catch(...){}
 // Reject an overfull directory as a whole; never select an enumeration-order-dependent subset.
 std::vector<std::filesystem::path> files;bool overflow=false;std::error_code ec;size_t entries=0;
 if(std::filesystem::exists(externalPath,ec))for(std::filesystem::directory_iterator it(externalPath,ec),end;it!=end&&!ec;it.increment(ec)){if(++entries>256){overflow=true;break;}if(JsonFile(it->path())&&it->is_regular_file(ec)){files.push_back(it->path());if(files.size()>64){overflow=true;break;}}}
 std::map<std::string,std::vector<Pack>> candidates;
 if(overflow||ec)data->warnings.push_back("External language directory skipped: entry/file limit or enumeration failure");else{
  std::sort(files.begin(),files.end());for(auto&path:files)try{auto j=ReadBounded(path,1024*1024);if(!j.is_object()||!j.contains("schemaVersion")||!j["schemaVersion"].is_number_integer()||j["schemaVersion"].get<int>()!=1||!j.contains("id")||!j["id"].is_string()||!j.contains("name")||!j["name"].is_string()||!j.contains("strings")||!j["strings"].is_object())throw std::runtime_error("Invalid language schema");Pack pack{j["id"].get<std::string>(),j["name"].get<std::string>(),{}};if(!ValidId(pack.id)||pack.id=="ko"||pack.id=="en"||!ValidName(pack.name))throw std::runtime_error("Invalid/reserved language identity");for(auto&[key,value]:j["strings"].items())if(Known(key)&&value.is_string()){auto text=value.get<std::string>();if(SafeTranslation(key,text))pack.strings.emplace(key,std::move(text));}candidates[pack.id].push_back(std::move(pack));}catch(...){data->warnings.push_back("External language file rejected: "+DisplayFile(path));}
  for(auto&[id,packs]:candidates)if(packs.size()==1)data->packs.push_back(std::move(packs.front()));else data->warnings.push_back("Duplicate external language ID rejected: "+id);
 }
 for(auto&pack:data->packs)data->labels.push_back(pack.name.c_str());
 std::string requested="ko";try{auto j=ReadBounded(settingsPath,4096);if(j.is_object()&&j.contains("language")&&j["language"].is_string())requested=j["language"].get<std::string>();}catch(...){}
 unsigned selected=0;for(unsigned i=0;i<data->packs.size();++i)if(data->packs[i].id==requested){selected=i;break;}
 published=std::move(data);catalog.store(published.get(),std::memory_order_release);language.store(selected);
 });}
inline bool Save(unsigned value,std::string&error){const auto*data=catalog.load(std::memory_order_acquire);if(value>=data->packs.size()){error="Unsupported language";return false;}std::filesystem::path temp;try{std::filesystem::create_directories(settingsPath.parent_path());temp=settingsPath;temp+=L"."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64())+L".tmp";auto bytes=nlohmann::json{{"language",data->packs[value].id}}.dump(2);{std::ofstream f(temp,std::ios::binary|std::ios::trunc);if(!f.write(bytes.data(),bytes.size()))throw std::runtime_error(T("r0049"));f.flush();if(!f)throw std::runtime_error(T("r0049"));}if(!MoveFileExW(temp.c_str(),settingsPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error(T("r0050"));language=value;revision.fetch_add(1);error.clear();return true;}catch(const std::exception&e){if(!temp.empty()){std::error_code ec;std::filesystem::remove(temp,ec);}error=e.what();return false;}}
}
