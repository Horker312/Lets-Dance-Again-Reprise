namespace FacialPresets;
// Matches LDA's per-channel preference and one MFG expression override group.
// Returns copies: preview filtering/rounding must never mutate authored data.
public static class MorphSelection {
 public static List<Morph> Resolve(Preset p,bool includeLegacy=true){
  var winners=new Dictionary<string,(Morph morph,int preferred,int priority,string channel)>();
  foreach(var (channel,c) in p.Channels)foreach(var m in c.Morphs){if(!m.Enabled||(!includeLegacy&&m.Backend!="MFEE"))continue;if(m.Backend=="MFG"&&m.Id.StartsWith("modifier:",StringComparison.Ordinal)&&int.Parse(m.Id.Split(':')[1])>=14)continue;bool expression=m.Backend=="MFG"&&m.Id.StartsWith("expression:",StringComparison.Ordinal);string slot=expression?"expression-group":m.Backend+":"+(m.Backend=="MFEE"?m.Name.ToLowerInvariant():m.Id);
   int preferred=m.Backend=="MFG"&&((expression&&channel=="emotion")||(m.Id.StartsWith("modifier:",StringComparison.Ordinal)&&channel=="eyes")||(m.Id.StartsWith("phoneme:",StringComparison.Ordinal)&&channel=="mouth"))?1:0;
   bool win=!winners.TryGetValue(slot,out var old);if(!win){int order=preferred.CompareTo(old.preferred);if(order==0)order=c.Priority.CompareTo(old.priority);if(order==0)order=string.CompareOrdinal(channel,old.channel);if(order==0&&expression)order=int.Parse(old.morph.Id.Split(':')[1]).CompareTo(int.Parse(m.Id.Split(':')[1]));win=order>0;}
   if(win)winners[slot]=(m,preferred,c.Priority,channel);
  }
  return winners.OrderBy(x=>x.Key,StringComparer.Ordinal).Select(x=>new Morph{Backend=x.Value.morph.Backend,Id=x.Value.morph.Id,Name=x.Value.morph.Name,Strength=Math.Round(x.Value.morph.Strength,MidpointRounding.AwayFromZero)}).ToList();
 }
}
