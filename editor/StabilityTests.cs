using System.IO;
using System.Security.Cryptography;
using System.Text.Json;
namespace DanceStudio;

// No user/game resources are required. DanceStudio.exe --stability-test <scratch-folder>
public static class StabilityTests {
 public static void Run(string output) {
  Directory.CreateDirectory(output);
  string root=Path.Combine(Path.GetFullPath(output),"stability-"+Guid.NewGuid().ToString("N"));
  Directory.CreateDirectory(root);
  int checks=0;
  void Check(bool valid,string description){checks++;if(!valid)throw new Exception("Stability test failed: "+description);}
  void Reject(Action operation,string description){bool rejected=false;try{operation();}catch(Exception ex)when(ex is IOException or UnauthorizedAccessException or JsonException or ArgumentException){rejected=true;}Check(rejected,description);}
  try {
   string project=Path.Combine(root,"project.danceproj");var original=new SongProject();
   original.Save(project);var loaded=SongProject.Load(project);
   Check(loaded.Id==original.Id&&loaded.Title==original.Title,"Project round trip");
   byte[] before=File.ReadAllBytes(project);
   loaded.ExportOut=double.NaN;
   Reject(()=>loaded.Save(project),"Nonfinite project values rejected");
   Check(before.SequenceEqual(File.ReadAllBytes(project)),"Invalid save leaves original bytes intact");
   loaded.ExportOut=12;loaded.Title="Recovery test";loaded.Save(project);
   Check(SongProject.Load(project).Title=="Recovery test","Existing project safely replaced");
   string invalid=Path.Combine(root,"invalid.danceproj");File.WriteAllText(invalid,"{");
   Reject(()=>SongProject.Load(invalid),"Malformed JSON rejected");
   string oversized=Path.Combine(root,"oversized.danceproj");
   using(var f=new FileStream(oversized,FileMode.CreateNew,FileAccess.Write,FileShare.None))f.SetLength(32L*1024*1024+1);
   Reject(()=>SongProject.Load(oversized),"Oversized project rejected before parsing");
   string folder=Path.Combine(root,"package");Directory.CreateDirectory(folder);
   string music=Path.Combine(folder,"music.wav"),camera=Path.Combine(folder,"camera.samples.json");
   File.WriteAllBytes(music,[1,2,3]);File.WriteAllText(camera,"{}");
   string Hash(string file)=>Convert.ToHexStringLower(SHA256.HashData(File.ReadAllBytes(file)));
   var hashes=new Dictionary<string,string>{{"music.wav",Hash(music)},{"camera.samples.json",Hash(camera)}};
   object Manifest(Dictionary<string,string> values)=>new {schemaVersion=4,id=Guid.NewGuid().ToString("N"),projectId=original.Id,exportId=Guid.NewGuid().ToString("N"),packageKind="camera",duration=1.0,token=0,oarSubmod="",music="music.wav",cameraSamples="camera.samples.json",hashes=values};
   string manifest=Path.Combine(folder,"song.json");
   File.WriteAllText(manifest,JsonSerializer.Serialize(Manifest(hashes),SongProject.Json));
   PackageUpdates.Validate(folder);Check(true,"Valid camera-only package accepted");
   var traversal=new Dictionary<string,string>(hashes){{"../escape.wav","00"}};
   File.WriteAllText(manifest,JsonSerializer.Serialize(Manifest(traversal),SongProject.Json));
   Reject(()=>PackageUpdates.Validate(folder),"Package hash traversal rejected");
   File.WriteAllText(manifest,JsonSerializer.Serialize(Manifest(hashes),SongProject.Json));
   File.AppendAllText(music,"corruption");
   Reject(()=>PackageUpdates.Validate(folder),"Package hash corruption rejected");
   Reject(()=>PackageUpdates.SafeTree(Path.Combine(root,"..","outside"),root),"Output directory traversal rejected");
   PackageUpdates.SafeTree(folder,root);Check(true,"Contained output directory accepted");
  } finally {FileCleanup.DeleteDirectory(root);}
  File.WriteAllText(Path.Combine(output,"stability-validation.json"),JsonSerializer.Serialize(new {passed=true,checks,gameTested=false,uiTested=false},SongProject.Json));
 }
}
