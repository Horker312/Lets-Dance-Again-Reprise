#pragma once
#include <map>
#include <string>
#include <unordered_map>
namespace DanceStudio::Facial {
inline std::string MfeeName(std::string name){for(char&c:name)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return name;}
class PendingBudget {
 std::map<std::string,unsigned> attempts;
public:
 bool Waiting(const std::string&id)const{return attempts.contains(id);}
 bool Suppressed(const std::string&id)const{auto it=attempts.find(id);return it!=attempts.end()&&it->second>=8;}
 bool Pending(const std::string&id){auto&n=attempts[id];if(n<8)++n;return n<8;}
 void Applied(const std::string&id){attempts.erase(id);}void Gap(const std::string&id){attempts.erase(id);}void Clear(){attempts.clear();}
};
class MorphTargetBatch {
 bool valid{},wrote{};
public:
 std::unordered_map<std::string,int> values;
 void Reset(){valid=wrote=false;values.clear();}
 template<class Fetch>void Ensure(Fetch fetch){if(!valid){values=fetch();valid=true;}}
 void Written(const std::string&name,int value){values[name]=value;wrote=true;}
 template<class Fetch,class Observe>void Finish(Fetch fetch,Observe observe){try{if(wrote){values=fetch();valid=true;observe();}}catch(...){Reset();throw;}Reset();}
};
}
