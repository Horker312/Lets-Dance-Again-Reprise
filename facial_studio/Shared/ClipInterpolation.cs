namespace FacialPresets;

// Linear intervals own the union of enabled endpoint morphs, including zero targets.
// A disabled/missing endpoint contributes zero; Hold never acquires incoming morphs.
public static class ClipInterpolation {
 public static bool Expression(Morph m)=>m.Backend=="MFG"&&m.Id.StartsWith("expression:",StringComparison.Ordinal);
 static Morph Copy(Morph m,double value)=>new(){Backend=m.Backend,Id=m.Id,Name=m.Name,Enabled=true,Strength=value};
 public static Preset Blend(Preset a,Preset b,double alpha){
  alpha=Math.Clamp(alpha,0,1);var result=new Preset{Profile=a.Profile,Name=a.Name,Channels=new(),RequiredResources=a.RequiredResources.Concat(b.RequiredResources).Distinct().ToList()};
  foreach(var name in a.Channels.Keys.Union(b.Channels.Keys)){
   a.Channels.TryGetValue(name,out var ac);b.Channels.TryGetValue(name,out var bc);
   var left=(ac?.Morphs??[]).Where(m=>m.Enabled).ToDictionary(m=>(m.Backend,m.Id));var right=(bc?.Morphs??[]).Where(m=>m.Enabled).ToDictionary(m=>(m.Backend,m.Id));
   var ch=new Channel{Priority=ac?.Priority??bc?.Priority??0};
   foreach(var id in left.Keys.Union(right.Keys)){left.TryGetValue(id,out var l);right.TryGetValue(id,out var r);if(Expression(l??r!))continue;ch.Morphs.Add(Copy(l??r!, (l?.Strength??0)*(1-alpha)+(r?.Strength??0)*alpha));}
   // One engine expression slot: deterministic smallest ID at each endpoint.
   var x=left.Values.Where(Expression).OrderBy(m=>int.Parse(m.Id.Split(':')[1])).FirstOrDefault();var y=right.Values.Where(Expression).OrderBy(m=>int.Parse(m.Id.Split(':')[1])).FirstOrDefault();
   if(x!=null&&y!=null&&x.Id!=y.Id){var m=alpha<.5?x:y;ch.Morphs.Add(Copy(m,alpha<.5?x.Strength*(1-2*alpha):y.Strength*(2*alpha-1)));}
   else if(x!=null||y!=null)ch.Morphs.Add(Copy(x??y!, (x?.Strength??0)*(1-alpha)+(y?.Strength??0)*alpha));
   if(ch.Morphs.Count>0)result.Channels[name]=ch;
  }return result;
 }
 public static Preset ActiveCopy(Preset p){var result=FacialTimeline.Copy(p);foreach(var ch in result.Channels.Values){ch.Morphs.RemoveAll(m=>!m.Enabled);var expressions=ch.Morphs.Where(Expression).OrderBy(m=>int.Parse(m.Id.Split(':')[1])).ToArray();foreach(var m in expressions.Skip(1))ch.Morphs.Remove(m);}return result;}
}
