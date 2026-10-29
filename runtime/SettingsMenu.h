#pragma once
#include "Localization.h"
#include "HotkeySettings.h"
#include "ProductInfo.h"
#include "Version.h"
#include "CameraFramingSettings.h"
#include <mutex>

// Public C ABI of SKSE Menu Framework 3.8/API 1. No ImGui context/layout access.
// https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API
namespace DanceStudio::Hotkeys {
class SettingsMenu {
    struct Window { std::atomic<bool> IsOpen{false}; std::atomic<bool> BlockUserInput{true}; };
    using Callback=void(__stdcall*)();
    using Add=void(*)(const char*,Callback);
    using Text=void(*)(const char*,const char*);
    using Button=bool(*)(const char*);
    using Checkbox=bool(*)(const char*,bool*);
    using Combo=bool(*)(const char*,int*,const char*const[],int,int);
    using GetWindow=Window*(*)();
    using Blocking=bool(*)();
    Text text{}; Button button{}; Combo combo{};Checkbox checkbox{};
    void(*pushWrap)(float){};void(*popWrap)(){};
    GetWindow window{}; Blocking blocking{};
    std::atomic<bool> ready{};
    Store& store; bool(*idle)();
    bool(*hasPhoto)(){};
    CameraFramingStore* framing{};
    std::vector<std::string> percentLabels;std::vector<const char*> percentItems;
    Bindings draft{};bool draftNative{}; unsigned menuKey{65};
    std::vector<unsigned> codes; std::vector<std::string> labels; std::vector<const char*> items;
    std::string status; std::mutex mutex;
    static inline SettingsMenu* instance{}; // Registered once; process lifetime.
    template<class T> static T Export(HMODULE mod,const char* name) { return reinterpret_cast<T>(GetProcAddress(mod,name)); }
    static void __stdcall Draw() noexcept { if (instance) instance->Render(); }
    static std::string Label(unsigned scan) {
        if (!scan) return L10n::T("r0001");
        wchar_t name[128]{};
        LONG bits=static_cast<LONG>((scan & 127)<<16); if (scan & 128) bits|=1<<24;
        if (!GetKeyNameTextW(bits,name,128)) return "Scan "+std::to_string(scan);
        char utf8[512]{}; WideCharToMultiByte(CP_UTF8,0,name,-1,utf8,512,nullptr,nullptr);
        return std::string(utf8)+" ("+std::to_string(scan)+")";
    }
    void Choices() {
        codes.clear(); labels.clear(); items.clear();
        for (unsigned i=0;i<256;++i) if (Allowed(i,menuKey)) { codes.push_back(i); labels.push_back(Label(i)); }
        for (auto& s:labels) items.push_back(s.c_str());
    }
    void Select(const char* name,unsigned& key) {
        int current=-1; for (size_t i=0;i<codes.size();++i) if (codes[i]==key) current=static_cast<int>(i);
        if (combo(name,&current,items.data(),static_cast<int>(items.size()),12) && current>=0 && current<static_cast<int>(codes.size())) key=codes[current];
    }
    void Line(const char* s) { if(pushWrap&&popWrap){pushWrap(0);text(s,nullptr);popWrap();}else text(s,nullptr); }
    void FramingControls(){
        if(!framing)return;
        if(percentLabels.empty()){for(unsigned i=0;i<=400;++i)percentLabels.push_back(std::to_string(i)+"%");for(auto&label:percentLabels)percentItems.push_back(label.c_str());}
        Line(L10n::T("r0152"));Line(L10n::T("r0153"));auto t=framing->Current();
        int motion=static_cast<int>(t.motionPercent);bool changed=false;
        if(combo(L10n::T("r0155"),&motion,percentItems.data(),401,12)){t.motionPercent=static_cast<unsigned>(motion);changed=true;}
        if(button(L10n::T("r0156"))){t={};changed=true;}
        if(changed){std::string error;status=framing->Save(t,error)?L10n::T("r0157"):std::string(L10n::T("r0018"))+error;}
    }
    void Render() noexcept {
        try {
            std::scoped_lock lock(mutex);
            Line((std::string(ProductName)+" "+RuntimeVersion).c_str());
            int selectedLanguage=static_cast<int>(L10n::language.load());const char*languages[]={"한국어","English"};
            if(combo("한국어 / English##DanceStudioLanguage",&selectedLanguage,languages,2,2)){std::string error;if(L10n::Save(selectedLanguage,error)){Choices();status=L10n::T("r0017");}else status=error;}

            Line(L10n::T("r0002"));
            Line(L10n::T("r0003"));
            Line(L10n::T("r0004"));
            if (!idle()) { Line(L10n::T("r0005")); return; }
            Line(L10n::T("r0006"));
            FramingControls();
            checkbox(L10n::T("r0007"),&draftNative);
            Line(L10n::T("r0008"));
            Line(L10n::T("r0009"));
            Select(L10n::T("r0010"),draft.start);
            Select(L10n::T("r0011"),draft.stop);
            Select(L10n::T("r0012"),draft.hud);
            Line(L10n::T("r0013"));
            if(hasPhoto&&hasPhoto()){Select(L10n::T("r0014"),draft.photo);Line(L10n::T("r0015"));}
            if (button(L10n::T("r0016"))) {
                auto currentMenu=MenuKey();
                std::string error;
                if (store.Save(draft,currentMenu,error,false,draftNative)) status=L10n::T("r0017");
                else status=L10n::T("r0018")+error;
                if (menuKey!=currentMenu) { menuKey=currentMenu; Choices(); }
            }
            if (button(L10n::T("r0019"))) { menuKey=MenuKey(); Choices(); draft=Defaults(menuKey);draftNative=false; status=L10n::T("r0020"); }
            if (button(L10n::T("r0021"))) { draft=store.Current();draftNative=store.NativePlayback(); status=L10n::T("r0022"); }
            auto error=Validate(draft,menuKey); if (!error.empty()) Line(error.c_str());
            if (!status.empty()) Line(status.c_str());
        } catch (...) { /* Never unwind through a foreign render callback. */ }
    }
public:
    SettingsMenu(Store& s,bool(*isIdle)(),bool(*photoAvailable)()=nullptr,CameraFramingStore* cameraFraming=nullptr) : store(s),idle(isIdle),hasPhoto(photoAvailable),framing(cameraFraming) {}
    bool Open() const {
        if (!ready.load(std::memory_order_acquire)) return false;
        auto* main=window();
        return (main && main->IsOpen.load()) || blocking();
    }
    bool Register(std::string& error) {
        if (ready.load()) return true;
        auto mod=GetModuleHandleW(L"SKSEMenuFramework.dll");
        if (!mod) { error=L10n::T("r0142"); return false; }
        auto api=Export<unsigned(*)()>(mod,"GetMenuFrameworkAPIVersion");
        if (!api || api()!=1) { error=L10n::T("r0143"); return false; }
        auto add=Export<Add>(mod,"AddSectionItem"); window=Export<GetWindow>(mod,"GetMainWindow");
        text=Export<Text>(mod,"igTextUnformatted"); button=Export<Button>(mod,"igSmallButton");checkbox=Export<Checkbox>(mod,"igCheckbox"); combo=Export<Combo>(mod,"igCombo_Str_arr"); blocking=Export<Blocking>(mod,"IsAnyBlockingWindowOpened");
        pushWrap=Export<void(*)(float)>(mod,"igPushTextWrapPos");popWrap=Export<void(*)()>(mod,"igPopTextWrapPos");
        if (!add || !window || !text || !button || !combo || !checkbox || !blocking) { error=L10n::T("r0144"); return false; }
        menuKey=MenuKey(); draft=store.Current();draftNative=store.NativePlayback(); Choices(); instance=this;
        add(SettingsPath,Draw);
        ready.store(true,std::memory_order_release);return true;
    }
};
}
