using System.IO;
using FacialPresets;
namespace FaceAssets;

// Reads installed MFEE ExtensionFile declarations, never talks to an in-game API.
// Traversal is limited to this known folder and each part's own mesh directory.
public sealed class MorphResources {
 public sealed record Extension(string Main,string Extra,string Config);
 public readonly List<Extension> Extensions=[];
 public readonly List<(string Category,string Name)> Declared=[];
 public readonly List<string> Notes=[];
 public MorphResources(GameResources resources){
  long bytes=0;foreach(string rel in resources.EnumerateBounded("SKSE/Plugins/MuFacialExpressionExtended",".ini",2,128)){
   try{string? file=resources.Find(rel);if(file==null)continue;long size=new FileInfo(file).Length;if(size>2*1024*1024||bytes+size>8*1024*1024){Notes.Add("MFEE configuration budget: "+rel);continue;}bytes+=size;
    foreach(string line in File.ReadLines(file).Take(16384)){string t=line.Trim();if(t.Length==0||t.StartsWith('#')||t.StartsWith(';'))continue;int eq=t.IndexOf('=');
     if(eq>0&&t[..eq].Trim().Equals("ExtensionFile",StringComparison.OrdinalIgnoreCase)){var v=t[(eq+1)..].Split(',').Select(x=>x.Trim()).ToArray();if(v.Length<2||v.Length>33)continue;foreach(var extra in v.Skip(1))if(Extensions.Count<4096&&Safe(v[0])&&Safe(extra))Extensions.Add(new(N(v[0]),N(extra),rel));}
     else{var v=t.Split('|');if(v.Length==2&&v[0].Length is >0 and <129&&v[1].Length is >0 and <513&&Declared.Count<4096)Declared.Add((v[0].Trim(),v[1].Trim()));}
    }
   }catch(Exception e){Notes.Add(rel+": "+e.Message);}
  }
 }
 static bool Safe(string rel)=>!Path.IsPathRooted(rel)&&!rel.Replace('\\','/').Split('/').Any(p=>p==".."||p.Contains(':'));
 static string N(string path){path=path.Replace('\\','/');return path.StartsWith("meshes/",StringComparison.OrdinalIgnoreCase)?path[7..]:path;}
 public sealed record SearchResult(int Attached,List<string> Details){public override string ToString()=>string.Join("\n",Details);}
 public SearchResult Connect(FaceScene scene,string morph){
  int attached=0,reads=0;long bytes=0,cells=scene.Parts.Sum(p=>(long)(p.Tri?.Morphs.Count??0)*p.Reference.Length);var messages=new List<string>();var resources=scene.Resources;
  long decoded=0;var cache=new Dictionary<string,TriFace?>(StringComparer.OrdinalIgnoreCase);
  TriFace? Read(string p){if(cache.TryGetValue(p,out var cached))return cached;if(++reads>96||new FileInfo(p).Length>64*1024*1024||bytes+new FileInfo(p).Length>128*1024*1024){messages.Add("TRI search budget reached.");return null;}bytes+=new FileInfo(p).Length;try{var tri=TriFace.Read(p);decoded+=(long)tri.Vertices.Length*tri.Morphs.Count;if(decoded>4000000){messages.Add("Decoded TRI budget reached.");return cache[p]=null;}return cache[p]=tri;}catch(Exception e){messages.Add(Path.GetFileName(p)+": "+e.Message);return cache[p]=null;}}
  foreach(var part in scene.Parts){if(part.Tri?.Morphs.ContainsKey(morph)==true)continue;
   var candidates=new HashSet<string>(StringComparer.OrdinalIgnoreCase);var related=new HashSet<string>(StringComparer.OrdinalIgnoreCase);string declared=N(part.ExpressionTriReference);
   // Only declarations for this HeadPart (not global same-name matches).
   foreach(var ext in Extensions){bool same=declared.Length>0?declared.Equals(ext.Main,StringComparison.OrdinalIgnoreCase):N(Path.ChangeExtension(part.LogicalModel.Length>0?part.LogicalModel:GameResources.CachedLogicalPath(part.Source)??part.Source,".tri")).Equals(ext.Main,StringComparison.OrdinalIgnoreCase);
    if(!same)continue;if(resources?.Mesh(ext.Extra) is string p){candidates.Add(p);related.Add(p);}else messages.Add(part.Name+": extension not found: "+ext.Extra);
   }
   string directory=Path.GetDirectoryName(part.Source)!;foreach(string stem in new[]{Path.GetFileNameWithoutExtension(part.Source),Path.GetFileNameWithoutExtension(part.LogicalModel),Path.GetFileNameWithoutExtension(declared)}.Distinct(StringComparer.OrdinalIgnoreCase)){
    if(stem.Length==0||stem.IndexOfAny(Path.GetInvalidFileNameChars())>=0)continue;string sidecar=Path.Combine(directory,stem+"_Extra.tri");if(File.Exists(sidecar))candidates.Add(sidecar);
    if(resources!=null){string logical=part.LogicalModel.Length>0?part.LogicalModel:GameResources.CachedLogicalPath(part.Source)??"";if(logical.Length>0){string rel=(Path.GetDirectoryName(logical.Replace('/',Path.DirectorySeparatorChar))??"")+"/"+stem+"_Extra.tri";if(resources.Mesh(rel) is string p)candidates.Add(p);}}
   }
   var accepted=new List<(string Path,TriFace Tri)>();foreach(string path in candidates.Take(32))try{
    var tri=Read(path);if(tri==null||!tri.Morphs.ContainsKey(morph))continue;string check=scene.Compatibility(part,tri);
    if(!check.StartsWith("EXACT")||part.Tri!=null&&(!part.Tri.SameTopology(tri)||!part.Tri.SameReference(tri))){messages.Add(part.Name+": "+Path.GetFileName(path)+" · "+check);continue;}
    // Probe a clone so conflicts cannot remove an already working preview morph.
    var merged=part.Tri?.Clone()??tri.Clone();if(part.Tri!=null)merged.AddMorphs(tri);if(merged.Conflicts.Count>(part.Tri?.Conflicts.Count??0)||!merged.Morphs.ContainsKey(morph)){messages.Add(part.Name+": conflicting morphs rejected: "+path);continue;}accepted.Add((path,tri));
   }catch(Exception e){messages.Add(part.Name+": "+e.Message);}
   if(accepted.Count>1){messages.Add(part.Name+": multiple compatible TRI candidates; choose a TRI manually. "+string.Join(" | ",accepted.Select(a=>a.Path)));continue;}
   if(accepted.Count==1){var a=accepted[0];long added=(long)a.Tri.Vertices.Length*a.Tri.Morphs.Keys.Count(n=>part.Tri?.Morphs.ContainsKey(n)!=true);if(cells+added>4000000){messages.Add("Combined TRI morph budget reached.");continue;}scene.AttachTri(part,a.Tri);part.AutoTriPaths.Add(a.Path);cells+=added;attached++;messages.Add(part.Name+": connected "+a.Path);}
  }
  if(attached==0&&messages.Count==0)messages.Add("No related compatible extension TRI found.");return new(attached,messages);
 }
}
