#pragma once
#include <algorithm>
#include <cmath>
namespace DanceStudio {
inline float ActorManagerInitialWidth(float viewportWidth){
 if(!std::isfinite(viewportWidth)||viewportWidth<=32)return 0;
 const float available=viewportWidth-32;
 return std::min(available,std::max(1024.f,available*.75f));
}
}
