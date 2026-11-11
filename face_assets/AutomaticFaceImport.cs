using System.IO;
using System.Numerics;
using System.Text.Json;
namespace FaceAssets;
public enum ImportSeverity {Information,Warning,NeedsConfirmation,Error}
public sealed record ImportNotice(ImportSeverity Severity,string Area,string Code,string Detail);
public sealed record FaceCandidate(string Path,bool Compatible,bool Equivalent,string Detail);
public sealed record PreparedFace(RaceMenuImport.Result Result,List<FaceCandidate> Candidates,List<ImportNotice> Notices,string? AutomaticNif,MorphResources MorphResources);
public static class AutomaticFaceImport {
 public static PreparedFace Prepare(string slot,string root,ImportContext context,string? selectedExport=null,bool confirmed=false,IReadOnlyList<FacialPresets.Morph>? previewMorphs=null){
  context.Stage("preset");var result=RaceMenuImport.Load(slot,root,confirmed,selectedExport,context);var resources=result.Scene.Resources!;context.Stage("exports");var candidates=new List<FaceCandidate>();string? automatic=selectedExport;
  if(selectedExport==null){long budget=0;foreach(string path in ExportedFace.Candidates(slot,root)){
   context.Cancellation.ThrowIfCancellationRequested();var info=new FileInfo(path);if(info.Length>32*1024*1024||budget+info.Length>128L*1024*1024||candidates.Count>=32){result.Messages.Add("Export candidate inspection budget reached.");break;}budget+=info.Length;
   try{var checkedFace=Inspect(result.Scene,path);candidates.Add(checkedFace);}catch(OperationCanceledException){throw;}catch(Exception e)when(ImportFiles.Expected(e)){candidates.Add(new(path,false,false,e.GetType().Name+": "+e.Message));}
  }
   var compatible=candidates.Where(c=>c.Compatible).ToList();var proven=compatible.Where(c=>c.Equivalent||FaceAssociations.IsTrusted(slot,c.Path,resources)).ToList();
   if(proven.Count==1){automatic=proven[0].Path;context.Stage("exported");if(proven[0].Equivalent){ExportedFace.Apply(result.Scene,automatic,result.Messages);}else{result=RaceMenuImport.Load(slot,root,true,automatic,context);resources=result.Scene.Resources!;}result.Messages.Add("Export auto-selected by geometry equivalence or prior hash-bound user confirmation; filename alone was not used.");}
  }
  var notices=new List<ImportNotice>{new(ImportSeverity.Information,"face","connected","HeadParts connected: "+result.Scene.Parts.Count)};
  if(automatic==null&&candidates.Any(c=>c.Compatible))notices.Add(new(ImportSeverity.NeedsConfirmation,"export","choose-export","Candidate identity requires confirmation."));
  foreach(var missing in result.Scene.Parts.Where(p=>p.Diffuse==null))notices.Add(new(ImportSeverity.Warning,Part(missing.HeadPartType),"texture-missing",missing.TextureReference));
  foreach(var missing in result.Scene.Parts.Where(p=>p.Tri==null&&(p.HeadPartType==1||p.ExpressionTriReference.Length>0)))notices.Add(new(ImportSeverity.Warning,Part(missing.HeadPartType),"tri-missing",missing.Name));
  if(result.Scene.ExportedNifPath==null&&result.Messages.Any(m=>m.Contains("generation evidence")))notices.Add(new(ImportSeverity.NeedsConfirmation,"generated","generated-identity","Saved runtime IDs cannot establish the current generation."));
  if(result.Scene.ExportedNifPath==null&&result.Messages.Any(m=>m.Contains("Default face sliders/presets not reconstructed")||m.Contains("Default TRI morph unavailable")||m.Contains("Slider mapping unavailable")||m.Contains("Sculpt not applied")))notices.Add(new(ImportSeverity.Warning,"shape","partial-shape","Some source geometry could not be reconstructed."));
  context.Stage("morphs");var morphResources=new MorphResources(resources);foreach(var morph in previewMorphs??[]){context.Cancellation.ThrowIfCancellationRequested();if(morph.Backend=="MFEE"&&!result.Scene.Parts.Any(p=>p.Tri?.Morphs.ContainsKey(morph.Name)==true))morphResources.Connect(result.Scene,morph.Name);}context.Stage("validation");resources.VerifyCurrent();if(ImportFiles.Digest(slot)!=result.Scene.JslotHash)throw new IOException("Preset changed while preparing preview");result.Scene.FreezeForTransfer();return new(result,candidates,notices,automatic,morphResources);
 }
 public static string Part(uint type)=>type switch{1=>"face",2=>"eyes",3=>"hair",6=>"brows",_=>"part"};
 public static FaceCandidate Inspect(FaceScene scene,string path)=>InspectScenes(scene,ExportedFace.Inspect(path,scene.Resources!),path);
 public static FaceCandidate InspectScenes(FaceScene scene,FaceScene exported,string path){bool head=false,equivalent=true;int matched=0;
  foreach(var part in scene.Parts){var same=exported.Parts.Where(p=>(p.Name==part.Name||p.Name==part.DeclaredEditorId)&&FaceScene.SameIndexedTopology(p.Indices,part.Indices)&&p.Reference.Length==part.Reference.Length).ToList();if(same.Count!=1){if(part.HeadPartType==1)return new(path,false,false,"Face identity/name/topology mismatch");continue;}var source=same[0];if(source.Mesh.TextureCoordinates.Count!=part.Mesh.TextureCoordinates.Count||source.Mesh.TextureCoordinates.Where((v,i)=>(v-part.Mesh.TextureCoordinates[i]).Length>.0001).Any())return new(path,false,false,"UV correspondence mismatch");if(!Matrix4x4.Invert(part.ToHead,out var inverse))return new(path,false,false,"Singular target bind");var target=source.Reference.Select(v=>Vector3.Transform(Vector3.Transform(v,source.ToHead),inverse)).ToArray();if(target.Any(v=>!FaceScene.Finite(v)))return new(path,false,false,"Invalid transformed geometry");if(part.HeadPartType==1){head=true;equivalent&=!target.Where((v,i)=>Vector3.Distance(v,part.Reference[i])>.0001f).Any();}matched++;}
  return new(path,head,head&&equivalent,"Matched parts: "+matched+"; equivalent evaluated head: "+equivalent);
 }
}
// Only records an explicit user's choice, with content and environment evidence.
// A matching record never claims that topology alone proves character identity.
public static class FaceAssociations {
 public static string StorePath{get;set;}=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"FacialStudio","face-associations.json");
 sealed record Entry(string Slot,string Environment,string Nif,string NifHash,Dictionary<string,string> Dependencies);
 static readonly object gate=new();
 static List<Entry> Read(){try{if(!File.Exists(StorePath)||new FileInfo(StorePath).Length>1024*1024)return [];return JsonSerializer.Deserialize<List<Entry>>(File.ReadAllText(StorePath))?.Where(e=>e!=null&&e.Slot?.Length==64&&e.Environment?.Length==64&&e.Nif!=null&&e.NifHash?.Length==64&&e.Dependencies!=null&&e.Dependencies.Count<=1024&&e.Dependencies.All(k=>k.Key!=null&&k.Value?.Length==64)).Take(128).ToList()??[];}catch(Exception e)when(ImportFiles.Expected(e)){return [];}}
 public static bool IsTrusted(string slot,string nif,GameResources resources){try{lock(gate){string hash=ImportFiles.Digest(slot);var entry=Read().FirstOrDefault(e=>e.Slot==hash&&e.Environment==resources.EnvironmentKey&&e.Nif.Equals(Path.GetFullPath(nif),StringComparison.OrdinalIgnoreCase)&&e.NifHash==ImportFiles.Digest(nif));if(entry==null)return false;return entry.Dependencies.All(kv=>resources.Find(kv.Key) is string file&&ImportFiles.Digest(file)==kv.Value);}}catch(Exception e)when(ImportFiles.Expected(e)){return false;}}
 public static void Remember(string slot,string nif,GameResources resources){lock(gate){var entry=new Entry(ImportFiles.Digest(slot),resources.EnvironmentKey,Path.GetFullPath(nif),ImportFiles.Digest(nif),resources.Origins.Where(o=>o.Sha256.Length==64).ToDictionary(o=>o.Logical,o=>o.Sha256,StringComparer.OrdinalIgnoreCase));var rows=Read();rows.RemoveAll(e=>e.Slot==entry.Slot&&e.Environment==entry.Environment);rows.Insert(0,entry);Directory.CreateDirectory(Path.GetDirectoryName(StorePath)!);string temp=StorePath+Guid.NewGuid()+".tmp";try{var selected=rows.Take(128).ToList();string json=JsonSerializer.Serialize(selected);while(System.Text.Encoding.UTF8.GetByteCount(json)>1024*1024&&selected.Count>1){selected.RemoveAt(selected.Count-1);json=JsonSerializer.Serialize(selected);}if(System.Text.Encoding.UTF8.GetByteCount(json)>1024*1024)throw new InvalidDataException("Association evidence exceeds budget");File.WriteAllText(temp,json);File.Move(temp,StorePath,true);}finally{try{File.Delete(temp);}catch(IOException){}catch(UnauthorizedAccessException){}}}}
}
