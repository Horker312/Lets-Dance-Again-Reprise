#pragma once
#include "Localization.h"
#include <Windows.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace DanceStudio::Hotkeys {
struct Bindings { unsigned start{68}, stop{66},hud{87},photo{0},pause{57}; bool operator==(const Bindings&) const = default; };
constexpr std::uint64_t Pack(Bindings k) { return (std::uint64_t(k.start) << 8) | (std::uint64_t(k.stop) << 16) | (std::uint64_t(k.hud)<<24) | (std::uint64_t(k.photo)<<40) | (std::uint64_t(k.pause)<<48); }
constexpr Bindings Unpack(std::uint64_t k) { return {unsigned((k >> 8) & 255), unsigned((k >> 16) & 255),unsigned((k>>24)&255),unsigned((k>>40)&255),unsigned((k>>48)&255)}; }
inline bool Allowed(unsigned k, unsigned menu) {
    if (!k) return true;
    if (k >= 2 && k <= 88) {
        for (unsigned blocked : {14u,15u,28u,29u,41u,42u,54u,56u,58u,63u,67u,69u,70u,84u,85u,86u})
            if (k == blocked) return false;
        return true;
    }
    for (unsigned extended : {156u,181u,199u,200u,201u,203u,205u,207u,208u,209u,210u,211u})
        if (k == extended) return true;
    return false;
}
inline std::string Validate(Bindings k, unsigned menu) {
    if(k.pause!=57)return L10n::T("r0037");
    if (!k.stop) return L10n::T("r0038");
    std::array<unsigned,5> keys{k.start,k.stop,k.hud,k.photo,k.pause};
    for(size_t i=0;i<keys.size();++i){if(!Allowed(keys[i],menu))return L10n::T("r0039");for(size_t j=0;j<i;++j)if(keys[i]&&keys[i]==keys[j])return L10n::T("r0040");}
    return {};
}
inline Bindings Defaults(unsigned menu) {
    return Bindings{};
}
inline Bindings Legacy(Bindings k, unsigned menu) {

    return Validate(k,menu).empty()?k:Defaults(menu);
}
// No assumed framework key and no fixed F7 reservation.
inline unsigned MenuKey(){return 0;}
class Store {
    std::filesystem::path path;
    static constexpr std::uint64_t CameraBit=1ull<<32,NativeBit=1ull<<33;
    std::atomic<std::uint64_t> active{Pack(Bindings{})|CameraBit};
public:
    explicit Store(std::filesystem::path p=L"Data\\SKSE\\Plugins\\DanceStudio\\UserSettings.json") : path(std::move(p)) {}
    Bindings Current() const { return Unpack(active.load()); }
    bool UseCamera() const { return (active.load()&CameraBit)!=0; }
    bool NativePlayback()const{return (active.load()&NativeBit)!=0;}
    // Initialization only, before registering the render callback/input sink.
    std::string Load(Bindings legacy, unsigned menu) {
        auto fallback=Legacy(legacy,menu); active=Pack(fallback)|CameraBit;
        try {
            if (!std::filesystem::exists(path)) return {};
            if (std::filesystem::file_size(path)>16384) throw std::runtime_error(L10n::T("r0041"));
            std::ifstream in(path); auto j=nlohmann::json::parse(in);
            if (!j.is_object() || !j.contains("schemaVersion") || !j["schemaVersion"].is_number_integer() || (j["schemaVersion"] != 1&&j["schemaVersion"] != 2&&j["schemaVersion"] != 3&&j["schemaVersion"] != 4&&j["schemaVersion"] != 5&&j["schemaVersion"] != 6&&j["schemaVersion"] != 7&&j["schemaVersion"] != 8))
                throw std::runtime_error(L10n::T("r0042"));
            auto key=[&](const char* name) {
                const auto& v=j.at(name);
                if (!v.is_number_integer() || v<0 || v>255) throw std::runtime_error(L10n::T("r0043"));
                return v.get<unsigned>();
            };
            Bindings k{key("prepareStart"),key("stop"),j["schemaVersion"]>=3?key("toggleHUD"):0,j["schemaVersion"]>=6?key("openPhotoMode"):0,57};
            if(j["schemaVersion"]<6)k=Legacy(k,menu);
            auto error=Validate(k,menu); if (!error.empty()) throw std::runtime_error(error);
            bool camera=true;if(j["schemaVersion"]>=2&&j["schemaVersion"]<=7){if(!j.at("useDedicatedCamera").is_boolean())throw std::runtime_error(L10n::T("r0044"));camera=j.at("useDedicatedCamera").get<bool>();}
            bool native=false;if(j["schemaVersion"]>=4){if(!j.at("nativePlayback").is_boolean())throw std::runtime_error(L10n::T("r0045"));native=j.at("nativePlayback").get<bool>();}
            active=Pack(k)|(camera?CameraBit:0)|(native?NativeBit:0); return {};
        } catch (const std::exception& e) { return std::string(L10n::T("r0046"))+e.what(); }
    }
    bool Save(Bindings k,unsigned menu,std::string& error){return Save(k,menu,error,UseCamera());}
    bool Save(Bindings k,unsigned menu,std::string& error,bool camera){return Save(k,menu,error,camera,NativePlayback());}
    bool Save(Bindings k, unsigned menu, std::string& error,bool camera,bool native) {
        error=Validate(k,menu); if (!error.empty()) return false;
        std::filesystem::path temp;
        HANDLE file=INVALID_HANDLE_VALUE;
        bool ownsTemp=false;
        try {
            if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
            auto bytes=nlohmann::json{{"schemaVersion",8},{"prepareStart",k.start},{"stop",k.stop},{"toggleHUD",k.hud},{"openPhotoMode",k.photo},{"pauseResume",k.pause},{"nativePlayback",native}}.dump(2)+"\n";
            static std::atomic<unsigned> serial{};
            for (unsigned attempt=0;attempt<32;++attempt) {
                temp=path; temp+=L".tmp."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(serial.fetch_add(1));
                file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
                if (file!=INVALID_HANDLE_VALUE) { ownsTemp=true; break; }
                if (GetLastError()!=ERROR_FILE_EXISTS) throw std::runtime_error(L10n::T("r0047"));
            }
            if (file==INVALID_HANDLE_VALUE) { temp.clear(); throw std::runtime_error(L10n::T("r0048")); }
            DWORD wrote{};
            if (!WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&wrote,nullptr) || wrote!=bytes.size() || !FlushFileBuffers(file))
                throw std::runtime_error(L10n::T("r0049"));
            CloseHandle(file); file=INVALID_HANDLE_VALUE;
            if (!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error(L10n::T("r0050"));
            temp.clear(); active=Pack(k)|(camera?CameraBit:0)|(native?NativeBit:0); return true;
        } catch (const std::exception& e) {
            if (file!=INVALID_HANDLE_VALUE) CloseHandle(file);
            if (ownsTemp && !temp.empty()) DeleteFileW(temp.c_str());
            error=e.what(); return false;
        }
    }
};
}
