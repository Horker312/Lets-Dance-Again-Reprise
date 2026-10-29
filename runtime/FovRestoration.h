#pragma once
#include <cmath>
#include <optional>
namespace DanceStudio {
struct FovRestoration {
 float before{},owned{};
 std::optional<float> Value(float current)const{
  if(!std::isfinite(before)||before<=0||before>=180||!std::isfinite(owned)||!std::isfinite(current)||std::abs(current-owned)>=.01f)return std::nullopt;
  return before;
 }
};
}
