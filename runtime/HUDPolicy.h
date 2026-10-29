#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
namespace DanceStudio {
struct HUDPolicy {
 std::vector<std::string> movies{"hudmenu.swf","truehud.swf","widgetmenu.swf","durabilitymenu.swf"};
 static std::string FileName(std::string_view url){auto i=url.find_last_of("/\\");auto name=std::string(url.substr(i==url.npos?0:i+1));for(char&c:name)if(c>='A'&&c<='Z')c+=32;return name;}
 bool Matches(std::string_view url)const noexcept{auto i=url.find_last_of("/\\");auto name=url.substr(i==url.npos?0:i+1);for(auto&candidate:movies){if(candidate.size()!=name.size())continue;bool same=true;for(size_t j=0;j<name.size();++j){char c=name[j];if(c>='A'&&c<='Z')c+=32;if(c!=candidate[j]){same=false;break;}}if(same)return true;}return false;}
};
}
