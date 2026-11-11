#pragma once
// Uses the game's freeze flag and Photo Mode's published active global.
// No private DLL addresses, hooks into Photo Mode, or synthetic key presses.
namespace DanceStudio::PhotoMode {
inline std::atomic<bool> available{};
inline RE::TESGlobal* activeGlobal{}; // Resolved after DataLoaded; plugin-owned persistent form.
inline void Resolve(){
 auto*data=RE::TESDataHandler::GetSingleton();
 if(!GetModuleHandleW(L"po3_PhotoMode.dll")||!data||!data->LookupModByName("PhotoMode.esp")){available=false;activeGlobal=nullptr;return;}
 activeGlobal=RE::TESForm::LookupByEditorID<RE::TESGlobal>("PhotoMode_IsActive");
 // Local form in the installed PhotoMode.esp, verified from its EDID.
 if(!activeGlobal)activeGlobal=data->LookupForm<RE::TESGlobal>(0x802,"PhotoMode.esp");
 available=activeGlobal!=nullptr;
}
inline bool Open(){
 if(!available.load())return false;
 auto*vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();if(!vm)return false;
 RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
 return vm->DispatchStaticCall("po3_photomode","TogglePhotoMode",RE::MakeFunctionArguments(true),callback);
}
inline bool Active(){return available.load()&&activeGlobal&&activeGlobal->value>0;}
inline bool Frozen(){auto*main=RE::Main::GetSingleton();return main&&main->GetRuntimeData().freezeTime;}
}
