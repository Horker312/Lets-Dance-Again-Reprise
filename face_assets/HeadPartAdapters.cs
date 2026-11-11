namespace FaceAssets;
public interface IHeadPartResourceAdapter {
 bool Supports(string plugin);
 List<HeadPartRecords.Record> Resolve(string id,GameResources resources,List<string> notes,string? exportedName=null);
}
// Registered adapters, not race-name branches inside the common face loader.
public static class HeadPartAdapters {
 static readonly List<IHeadPartResourceAdapter> adapters=[new ErinHeadPartAdapter()];
 public static void Register(IHeadPartResourceAdapter adapter){lock(adapters)adapters.Add(adapter);}
 static IHeadPartResourceAdapter[] Snapshot(){lock(adapters)return adapters.ToArray();}
 public static List<HeadPartRecords.Record> Resolve(string id,GameResources resources,List<string> notes,string? exportedName=null){
  if(resources.Context.SkipGeneratedIds&&exportedName==null)return [];
  foreach(var adapter in Snapshot())if(adapter.Supports(id.Split('|')[0])){
   if(exportedName==null&&!resources.Context.AllowUnprovenGeneratedIds){notes.Add("Generated HeadPart needs generation evidence; a saved FormID alone is not a stable identity: "+id);return [];}
   return adapter.Resolve(id,resources,notes,exportedName);
  }return [];
 }
 public static List<HeadPartRecords.Record> FromExport(IEnumerable<string> references,string name,GameResources resources,List<string> notes){foreach(string id in references){var found=Resolve(id,resources,notes,name);if(found.Count>0)return found;}return [];}
}
public sealed class ErinHeadPartAdapter:IHeadPartResourceAdapter {
 // These are the Erin controller's actual plugin/log/INI ABI, not a rule for other races.
 public bool Supports(string plugin)=>plugin.Equals("ErinRace.esm",StringComparison.OrdinalIgnoreCase);
 public List<HeadPartRecords.Record> Resolve(string id,GameResources r,List<string> notes,string? exportedName=null)=>GeneratedHeadParts.Resolve(id,r,notes,exportedName);
}
