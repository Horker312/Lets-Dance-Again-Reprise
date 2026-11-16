#pragma once
#include "ProductInfo.h"
#include <string>
#include <string_view>
namespace DanceStudio {
inline std::string HudNotification(std::string_view text) {
 if(text.empty())return std::string(ProductName);
 std::string out(ProductName);
 out.append(" · ");
 out.append(text);
 return out;
}
}
