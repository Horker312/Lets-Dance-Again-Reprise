#pragma once
#include "SessionCore.h"
#include <mutex>
namespace DanceStudio {
inline unsigned PlayCommandFor(SessionState state){switch(state){case SessionState::Idle:return 2;case SessionState::Preparing:return 8;case SessionState::Armed:return 16;case SessionState::Playing:return 4;default:return 0;}}
class SpaceGate {
 bool held{},owned{};
public:
 bool Owned()const{return owned;}
 struct Result{bool consume,toggle;};
 Result Event(SessionState state,bool down,bool up){bool active=state!=SessionState::Idle;bool consume=active||owned,toggle=false;if(up){held=false;owned=false;}else{toggle=!held&&down&&active&&state==SessionState::Playing;held=true;if(active)owned=true;}return{consume,toggle};}
};
class ConcurrentSpaceGate {
 std::mutex mutex;SpaceGate gate;
public:
 bool Owned(){std::scoped_lock lock(mutex);return gate.Owned();}
 SpaceGate::Result Event(SessionState state,bool down,bool up){std::scoped_lock lock(mutex);return gate.Event(state,down,up);}
};
}
