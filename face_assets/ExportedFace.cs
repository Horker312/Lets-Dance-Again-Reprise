using System.IO;
using System.Numerics;
namespace FaceAssets;

public static class ExportedFace {
 public static List<string> Candidates(string slot,string root){
  var folders=new HashSet<string>(StringComparer.OrdinalIgnoreCase){Path.GetDirectoryName(Path.GetFullPath(slot))!,Path.Combine(root,"SKSE","Plugins","CharGen")};
  var parent=Directory.GetParent(Path.GetDirectoryName(Path.GetFullPath(slot))!);if(parent!=null)folders.Add(parent.FullName);
  var files=new List<string>();foreach(string directory in folders)if(Directory.Exists(directory))files.AddRange(Directory.EnumerateFiles(directory,"*.nif",SearchOption.TopDirectoryOnly).Take(65));
  return files.Distinct(StringComparer.OrdinalIgnoreCase).OrderBy(p=>Path.GetFileNameWithoutExtension(p).Equals(Path.GetFileNameWithoutExtension(slot),StringComparison.OrdinalIgnoreCase)?0:1).ThenBy(p=>p,StringComparer.OrdinalIgnoreCase).Take(64).ToList();
 }
 public static FaceScene Inspect(string path,GameResources resources)=>FaceScene.Load(path,resources.Root,resources);
 public static void Apply(FaceScene scene,string path,List<string> notes){
  var exported=Inspect(path,scene.Resources??new GameResources(scene.TextureRoot!));var staged=new List<(FacePart part,Vector3[] neutral)>();
  foreach(var part in scene.Parts){var same=exported.Parts.Where(p=>(p.Name==part.Name||p.Name==part.DeclaredEditorId||part.ConfiguredExtra)&&FaceScene.SameIndexedTopology(p.Indices,part.Indices)&&p.Reference.Length==part.Reference.Length).ToList();if(same.Count!=1){notes.Add("Exported NIF part has no unique name + indexed topology match: "+part.Name+" / "+part.DeclaredEditorId);continue;}
   var source=same[0];if(source.Mesh.TextureCoordinates.Count!=part.Mesh.TextureCoordinates.Count||source.Mesh.TextureCoordinates.Where((uv,i)=>(uv-part.Mesh.TextureCoordinates[i]).Length>0.0001).Any()){notes.Add("Exported NIF UV correspondence rejected: "+part.Name);continue;}
   if(!Matrix4x4.Invert(part.ToHead,out var inverse))throw new InvalidDataException("Exported face target transform singular");var neutral=source.Reference.Select(v=>Vector3.Transform(Vector3.Transform(v,source.ToHead),inverse)).ToArray();if(neutral.Any(v=>!FaceScene.Finite(v)))throw new InvalidDataException("Exported face coordinates invalid");staged.Add((part,neutral));
  }
  if(!staged.Any(p=>p.part.HeadPartType==1))throw new InvalidDataException("Exported face rejected: face HDPT name, indexed topology and UV correspondence are not verified. Previous preview is retained.");
  foreach(var (part,neutral) in staged){part.IdentityReference??=(Vector3[])part.Reference.Clone();part.Reference=neutral;part.LastWeights=null;}
  scene.ExportedNifPath=Path.GetFullPath(path);notes.Add("Explicitly confirmed exported face NIF supplies final neutral geometry for "+staged.Count+" parts. Computed slider/Sculpt geometry is replaced, not added again. File name alone does not prove preset identity.");scene.InvalidateGeometry();scene.Apply(null);
 }
}
