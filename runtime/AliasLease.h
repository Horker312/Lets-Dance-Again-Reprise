#pragma once
#include <array>
#include <mutex>
#include <cstdint>
#include <limits>
namespace DanceStudio {
enum class AliasResult{Ready,Pending,Failed};
// Only positive observation releases a slot. Missing/deleted references stay quarantined.
enum class AliasDetachState{Detached,Attached,Unresolved,Invalid};
inline bool AliasDetachConfirmed(AliasDetachState state){return state==AliasDetachState::Detached;}
enum class AliasPackageState{Unknown,Performance,Other,Reevaluating};
inline AliasPackageState ObserveAliasPackage(bool known,bool matches,bool reevaluating){return !known?AliasPackageState::Unknown:matches?AliasPackageState::Performance:reevaluating?AliasPackageState::Reevaluating:AliasPackageState::Other;}
struct AliasRetryBudget{unsigned attempts{};double next{};bool Permit(double now){if(attempts>=60||now<next)return false;next=now+1;++attempts;return true;}};
// Production ownership protocol. Port supplies engine operations; the same
// protocol is exercised with delayed assignment/clear operations in tests.
template<class Port>class AliasLeaseTable {
 struct Entry{std::uint32_t actor{},token{};unsigned participant{};bool clearing{},queued{};std::uint64_t emptyFrame{};};
 std::array<Entry,5>entries{};mutable std::mutex mutex;std::uint32_t nonce{};
public:
 std::uint32_t ActorOf(std::uint64_t token)const{std::scoped_lock lock(mutex);for(auto&e:entries)if(e.token==token)return e.actor;return 0;}
 int SlotOf(std::uint64_t token)const{std::scoped_lock lock(mutex);for(unsigned i=0;i<5;++i)if(entries[i].token==token)return static_cast<int>(i);return -1;}
 std::uint64_t Orphan(unsigned slot,std::uint32_t actor){std::scoped_lock lock(mutex);if(slot>=5||entries[slot].token||nonce==std::numeric_limits<std::int32_t>::max())return 0;entries[slot]={actor,++nonce,slot,true};return nonce;}
 void Reset(){std::scoped_lock lock(mutex);entries={};} // nonce never reused across saves
 AliasResult Acquire(typename Port::Actor*a,std::uint64_t&ticket,unsigned participant){if(!Port::Ready())return AliasResult::Pending;auto actor=Port::Identity(a);if(!actor)return AliasResult::Failed;unsigned slot=5;bool request=false;
  {std::scoped_lock lock(mutex);if(ticket){for(unsigned i=0;i<5;++i)if(entries[i].token==ticket&&entries[i].actor==actor){slot=i;break;}if(slot==5)return AliasResult::Failed;}
   else{for(auto&e:entries)if(e.actor==actor)return AliasResult::Failed;for(unsigned i=0;i<5;++i)if(!entries[i].token&&Port::Available(i,Port::Read(i))){slot=i;break;}if(slot==5||nonce==std::numeric_limits<std::int32_t>::max())return AliasResult::Failed;ticket=++nonce;entries[slot]={actor,static_cast<std::uint32_t>(ticket),participant};request=true;}
   if(entries[slot].clearing)return AliasResult::Pending;}
  if(request)Port::Force(slot,a,static_cast<std::uint32_t>(ticket),participant);
  auto actual=Port::Read(slot);return actual==actor?AliasResult::Ready:Port::Available(slot,actual)?AliasResult::Pending:AliasResult::Failed;
 }
 bool Owned(std::uint64_t ticket,std::uint32_t actor)const{std::scoped_lock lock(mutex);for(unsigned i=0;i<5;++i)if(entries[i].token==ticket&&entries[i].actor==actor)return !entries[i].clearing&&Port::Read(i)==actor;return false;}
 AliasResult Release(std::uint64_t ticket,std::uint32_t actor,bool allowRequest=true){unsigned slot=5;Entry snapshot;{std::scoped_lock lock(mutex);for(unsigned i=0;i<5;++i)if(entries[i].token==ticket){slot=i;break;}if(slot==5)return AliasResult::Ready;auto&e=entries[slot];if(e.actor!=actor)return AliasResult::Failed;e.clearing=true;snapshot=e;
   if(e.queued&&Port::Parked(slot,Port::Read(slot))&&AliasDetachConfirmed(Port::Detached(slot,actor))){auto frame=Port::Frame();if(!e.emptyFrame)e.emptyFrame=frame;if(frame>=e.emptyFrame+2){entries[slot]={};return AliasResult::Ready;}}else e.emptyFrame=0;
   if(e.queued||!allowRequest)return AliasResult::Pending;if(Port::Read(slot)!=actor&&!Port::Available(slot,Port::Read(slot)))return AliasResult::Pending;e.queued=true;}
  if(!Port::Evict(slot,snapshot.token,snapshot.actor,snapshot.participant)){std::scoped_lock lock(mutex);if(entries[slot].token==ticket)entries[slot].queued=false;}
  return AliasResult::Pending;
 }
};
}
