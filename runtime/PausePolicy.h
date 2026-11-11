#pragma once
#include <cstdint>
namespace DanceStudio {
enum PauseReason:std::uint32_t { PauseUser=1,PauseFreeze=2,PauseGame=4,PauseMenu=8,PauseSettings=16,PauseFocus=32,PauseGraph=64,PauseNoSession=128 };
struct PauseInputs {bool user{},freeze{},game{},menu{},settings{},unfocused{},graphReady{true},playing{true};};
inline std::uint32_t PauseMask(PauseInputs p){return(p.user?PauseUser:0)|(p.freeze?PauseFreeze:0)|(p.game?PauseGame:0)|(p.menu?PauseMenu:0)|(p.settings?PauseSettings:0)|(p.unfocused?PauseFocus:0)|(!p.graphReady?PauseGraph:0)|(!p.playing?PauseNoSession:0);}
inline bool NativeMayAdvance(bool publishedAdvance,bool freezeNow,std::uint32_t reasons){return publishedAdvance&&!freezeNow&&!reasons;}
inline bool ReclaimPhotoCamera(bool recorded,bool photoActive,bool returnPending,bool recoveryDue){return recorded&&!photoActive&&(returnPending||recoveryDue);}
enum class PauseAction {None,Hold,Resume};
inline PauseAction DecidePause(bool alreadyPaused,std::uint32_t reasons,bool allowResume){if(reasons)return alreadyPaused?PauseAction::None:PauseAction::Hold;return allowResume&&alreadyPaused?PauseAction::Resume:PauseAction::None;}
}
