#pragma once
#include <string_view>
namespace DanceStudio {
inline constexpr char ProductName[]="LDA:R";
inline constexpr char SettingsPath[]="LDA:R/단축키";
inline bool IsDanceMod(std::string_view name){return name==ProductName||name=="Let's Dance Again: Reprise"||name=="Dance Studio";}
}
