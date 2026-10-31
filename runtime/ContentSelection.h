#pragma once
#include "TrackData.h"
#include <optional>
namespace DanceStudio {
struct ContentSelection{std::optional<Song> dance,camera;const Song& Master()const{return dance?*dance:*camera;}};
inline std::optional<ContentSelection> ResolveContent(const std::vector<Song>&songs,const std::string&danceId,const std::string&cameraId){ContentSelection selection;for(auto&s:songs){if(!danceId.empty()&&s.id==danceId&&s.HasDance())selection.dance=s;if(!cameraId.empty()&&s.id==cameraId&&s.HasCamera())selection.camera=s;}if((!danceId.empty()&&!selection.dance)||(!cameraId.empty()&&!selection.camera)||(!selection.dance&&!selection.camera))return{};return selection;}
}
