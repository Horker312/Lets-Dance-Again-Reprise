#pragma once
#include "HotkeySettings.h"
#include "ProductInfo.h"
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
    GetWindow window{}; Blocking blocking{};
    std::atomic<bool> ready{};
    Store& store; bool(*idle)();
    bool(*hasPhoto)(){};
    Bindings draft{};bool draftNative{}; unsigned menuKey{65};
    std::vector<unsigned> codes; std::vector<std::string> labels; std::vector<const char*> items;
    std::string status; std::mutex mutex;
    static inline SettingsMenu* instance{}; // Registered once; process lifetime.
    template<class T> static T Export(HMODULE mod,const char* name) { return reinterpret_cast<T>(GetProcAddress(mod,name)); }
    static void __stdcall Draw() noexcept { if (instance) instance->Render(); }
    static std::string Label(unsigned scan) {
        if (!scan) return "사용 안 함";
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
    void Line(const char* s) { text(s,nullptr); }
    void Render() noexcept {
        try {
            std::scoped_lock lock(mutex);
            Line(ProductName);
            Line("재생 / 단축키 설정");
            Line("기본: F10 준비·시작·정지 / F8 중단 / F11 HUD 표시 전환 / Space 일시정지·재개");
            Line("다른 모드의 단축키와 겹치지 않는 키를 선택하세요.");
            if (!idle()) { Line("재생/준비를 중단한 다음 설정할 수 있습니다. 메뉴를 닫고 중단 키를 누르세요."); return; }
            Line("준비 키를 누르면 곡 목록이 열립니다.");
            checkbox("일반 모션 재생으로 비교 (진단)##DanceStudioNative",&draftNative);
            Line("끄면 기존 음악 동기화. 켜면 재생 중 시간 강제 지정을 끕니다.");
            Line("진단 모드는 음악·카메라와 싱크가 달라질 수 있습니다.");
            Select("준비 / 시작 / 정지##DanceStudioStart",draft.start);
            Select("중단##DanceStudioStop",draft.stop);
            Select("HUD 숨김 / 복원##DanceStudioHUD",draft.hud);
            Line("Space: 활성 상태 입력 차단 / 재생 중 일시정지·재개");
            if(hasPhoto&&hasPhoto()){Select("Photo Mode 열기##DanceStudioPhoto",draft.photo);Line("Photo Mode 열기 키는 기본적으로 사용 안 함입니다.");}
            if (button("저장 및 적용##DanceStudioSave")) {
                auto currentMenu=MenuKey();
                std::string error;
                if (store.Save(draft,currentMenu,error,false,draftNative)) status="저장했습니다. 메뉴를 닫으면 새 키가 적용됩니다.";
                else status="저장 실패: "+error;
                if (menuKey!=currentMenu) { menuKey=currentMenu; Choices(); }
            }
            if (button("기본값 불러오기##DanceStudioDefaults")) { menuKey=MenuKey(); Choices(); draft=Defaults(menuKey);draftNative=false; status="기본값을 불러왔습니다. 저장 및 적용을 눌러 확정하세요."; }
            if (button("변경 취소##DanceStudioCancel")) { draft=store.Current();draftNative=store.NativePlayback(); status="저장하지 않은 변경을 취소했습니다."; }
            auto error=Validate(draft,menuKey); if (!error.empty()) Line(error.c_str());
            if (!status.empty()) Line(status.c_str());
        } catch (...) { /* Never unwind through a foreign render callback. */ }
    }
public:
    SettingsMenu(Store& s,bool(*isIdle)(),bool(*photoAvailable)()=nullptr) : store(s),idle(isIdle),hasPhoto(photoAvailable) {}
    bool Open() const {
        if (!ready.load(std::memory_order_acquire)) return false;
        auto* main=window();
        return (main && main->IsOpen.load()) || blocking();
    }
    bool Register(std::string& error) {
        if (ready.load()) return true;
        auto mod=GetModuleHandleW(L"SKSEMenuFramework.dll");
        if (!mod) { error="SKSE Menu Framework missing; INI/default hotkeys remain active"; return false; }
        auto api=Export<unsigned(*)()>(mod,"GetMenuFrameworkAPIVersion");
        if (!api || api()!=1) { error="SKSE Menu Framework API 1 is required"; return false; }
        auto add=Export<Add>(mod,"AddSectionItem"); window=Export<GetWindow>(mod,"GetMainWindow");
        text=Export<Text>(mod,"igTextUnformatted"); button=Export<Button>(mod,"igSmallButton");checkbox=Export<Checkbox>(mod,"igCheckbox"); combo=Export<Combo>(mod,"igCombo_Str_arr"); blocking=Export<Blocking>(mod,"IsAnyBlockingWindowOpened");
        if (!add || !window || !text || !button || !combo || !checkbox || !blocking) { error="SKSE Menu Framework exports unavailable"; return false; }
        menuKey=MenuKey(); draft=store.Current();draftNative=store.NativePlayback(); Choices(); instance=this;
        add(SettingsPath,Draw);
        ready.store(true,std::memory_order_release);return true;
    }
};
}
