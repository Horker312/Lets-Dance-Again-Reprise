#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace DanceStudio {
// Uses each category's dynamic multiplier, never the saved menu/master volume.
template<class Category>class ScopedCategoryMute {
 struct Saved {Category* category;float value;};std::vector<Saved> saved;
public:
 bool Restore()noexcept{
  std::size_t remaining=0;
  for(auto s:saved)try{if(s.category->GetCategoryVolume()==0)s.category->SetCategoryVolume(s.value);}catch(...){saved[remaining++]=s;}
  saved.resize(remaining);return saved.empty();
 }
 void Begin(const std::vector<Category*>&categories){
  if(!Restore())throw std::runtime_error("Previous background volume restoration is pending");
  // Capture all values before muting a parent or child category.
  for(auto*c:categories){if(!c||std::any_of(saved.begin(),saved.end(),[&](auto&s){return s.category==c;}))continue;float value=c->GetCategoryVolume();if(!std::isfinite(value)||value<0)throw std::runtime_error("Invalid background volume");saved.push_back({c,value});}
  try{for(auto&s:saved)s.category->SetCategoryVolume(0);}catch(...){Restore();throw;}
 }
 std::size_t Pending()const noexcept{return saved.size();}
};
}
