#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace DanceStudio::Facial {
enum class Kind { Expression, Modifier, Phoneme, Extended };
// Only MFG Fix's documented modifier indices are writable; frame capacity can differ.
inline bool MfgSingleFrameSupports(Kind kind,unsigned count,unsigned id){return kind==Kind::Modifier?(count>=14&&count<=17&&id<14):(kind==Kind::Phoneme&&count==16&&id<16);}
struct Key { Kind kind{};unsigned id{};std::string name;bool operator==(const Key&)const=default; };
using Value=std::vector<float>;
inline bool Equal(const Value&a,const Value&b){if(a.size()!=b.size()||a.empty())return false;for(size_t i=0;i<a.size();++i)if(!std::isfinite(a[i])||!std::isfinite(b[i])||std::abs(a[i]-b[i])>0.000001f)return false;return true;}
enum class Observation { Stable, Pending, Conflict, Unavailable };
inline Observation Classify(const Key&k,const std::optional<Value>&v,const Value&last){
 if(!v)return Observation::Unavailable;
 if(k.kind==Kind::Extended&&v->size()==2&&last.size()==2){
  if((*v)[1]!=last[1])return Observation::Conflict;
  return (*v)[0]==(*v)[1]?Observation::Stable:Observation::Pending;
 }
 return Equal(*v,last)?Observation::Stable:Observation::Conflict;
}
struct Entry {Key key;Value before,last;bool conflict{};};
// Main-thread only. No actor or face pointers. Expression is one override group.
class Ledger {
 struct Release {Entry entry;unsigned attempts{};};
 std::vector<Entry> entries;std::vector<Release> releases;
public:
 bool Empty()const{return entries.empty()&&releases.empty();}
 template<class Read> Observation Status(const Key&key,Read read)const{if(std::any_of(releases.begin(),releases.end(),[&](auto&r){return r.entry.key==key;}))return Observation::Pending;auto it=std::find_if(entries.begin(),entries.end(),[&](auto&e){return e.key==key;});if(it==entries.end()||it->conflict)return Observation::Unavailable;return Classify(key,read(key),it->last);}
 template<class Read> void ObserveExtended(Read read){for(auto&e:entries)if(e.key.kind==Kind::Extended&&Classify(e.key,read(e.key),e.last)==Observation::Conflict)e.conflict=true;}
 bool PendingRelease()const{return !releases.empty();}
 void Discard(){entries.clear();releases.clear();}
 template<class Read,class Write,class Log> bool Apply(const Key&key,const Value&desired,Read read,Write write,Log log){
  if(desired.empty()||std::any_of(desired.begin(),desired.end(),[](auto v){return !std::isfinite(v);})){log("FAIL invalid value",key,Value{},desired,Value{});return false;}
  if(std::any_of(releases.begin(),releases.end(),[&](auto&r){return r.entry.key==key;})){log("PARTIAL awaiting deferred release",key,Value{},desired,Value{});return false;}
  auto current=read(key);if(!current||current->size()!=desired.size()){log("UNSUPPORTED",key,Value{},desired,Value{});return false;}
  auto it=std::find_if(entries.begin(),entries.end(),[&](auto&e){return e.key==key;});
  if(it!=entries.end()&&Classify(key,current,it->last)==Observation::Pending){log("PARTIAL PENDING; write skipped",key,it->before,it->last,*current);return false;}
  if(it!=entries.end()&&(it->conflict||!Equal(*current,it->last))){it->conflict=true;log("PARTIAL external conflict; write skipped",key,it->before,it->last,*current);return false;}
  if(it==entries.end()){entries.push_back({key,*current,desired,false});it=entries.end()-1;}else it->last=desired;
  if(!write(key,desired)){it->conflict=true;log("FAIL write rejected",key,it->before,desired,*current);return false;}
  auto after=read(key);log(after&&Equal(*after,desired)?"PASS numeric write/readback; visual NOT TESTED":"PARTIAL readback differs/pending",key,it->before,desired,after.value_or(Value{}));return true;
 }
 template<class Read,class Write,class Log> void End(std::optional<Kind>kind,Read read,Write write,Log log,const Key*only=nullptr,bool deferPending=false){
  for(auto it=entries.rbegin();it!=entries.rend();++it){auto&e=*it;if((kind&&e.key.kind!=*kind)||(only&&!(e.key==*only)))continue;auto current=read(e.key);
   log("NOT TESTED restore precheck",e.key,e.before,e.last,current.value_or(Value{}));
   if(Classify(e.key,current,e.last)==Observation::Pending){if(deferPending&&!e.conflict){releases.push_back({e});log("PARTIAL PENDING; bounded restore queued",e.key,e.before,e.last,*current);}else log("PARTIAL PENDING; restore skipped",e.key,e.before,e.last,*current);continue;}
   if(!current||e.conflict||!Equal(*current,e.last)){log("PARTIAL external conflict/unavailable; restore skipped",e.key,e.before,e.last,current.value_or(Value{}));continue;}
   bool ok=write(e.key,e.before);auto after=ok?read(e.key):std::nullopt;
   log(after&&Equal(*after,e.before)?"PASS numeric restore/readback; visual NOT TESTED":Classify(e.key,after,e.before)==Observation::Pending?"PARTIAL restore requested; PENDING":"FAIL restore/readback",e.key,e.before,e.last,after.value_or(Value{}));
  }
  std::erase_if(entries,[&](auto&e){return (!kind||e.key.kind==*kind)&&(!only||e.key==*only);});
 }
 // Cleanup only: at most eight observations; never overwrite an external target.
 template<class Read,class Write,class Log> void RetryReleases(Read read,Write write,Log log){
  for(auto it=releases.begin();it!=releases.end();){auto&e=it->entry;auto current=read(e.key);auto status=Classify(e.key,current,e.last);
   if(!e.conflict&&status==Observation::Pending&&++it->attempts<8){++it;continue;}
   if(!e.conflict&&status==Observation::Stable){bool ok=write(e.key,e.before);auto after=ok?read(e.key):std::nullopt;
    log(!ok?"FAIL deferred restore rejected":after&&Equal(*after,e.before)?"PASS deferred restore":Classify(e.key,after,e.before)==Observation::Pending?"PARTIAL deferred restore requested; PENDING":"PARTIAL external/unavailable after deferred restore",e.key,e.before,e.last,after.value_or(Value{}));
   }else log(status==Observation::Pending?"PARTIAL deferred restore budget exhausted":"PARTIAL deferred restore yielded",e.key,e.before,e.last,current.value_or(Value{}));
   it=releases.erase(it);
  }
 }

};
}
