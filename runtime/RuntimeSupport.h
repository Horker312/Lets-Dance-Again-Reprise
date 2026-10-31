#pragma once
#include <cstdint>
namespace DanceStudio {
struct RuntimeHooks {std::uint64_t updateId,inputId;std::uint32_t updateOffset,inputOffset;};
inline constexpr RuntimeHooks Hooks1597{35565,67315,0x748,0x7B};
inline constexpr RuntimeHooks Hooks16640{36564,68617,0xC26,0x7B};
inline constexpr RuntimeHooks Hooks161170{36564,68617,0xC26,0x7B};
inline constexpr bool SupportedRuntime(unsigned major,unsigned minor,unsigned patch,unsigned build){return major==1&&build==0&&((minor==5&&patch==97)||(minor==6&&(patch==640||patch==1170)));}
inline constexpr RuntimeHooks HooksFor(unsigned minor,unsigned patch){return minor==5?Hooks1597:patch==640?Hooks16640:Hooks161170;}
}
