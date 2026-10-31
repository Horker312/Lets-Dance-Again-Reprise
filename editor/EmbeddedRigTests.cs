using System.IO;
using System.Security.Cryptography;
using System.Text.Json;
namespace DanceStudio;
public static class EmbeddedRigTests {
 public static void Run(string example,string output){
  Directory.CreateDirectory(output);int checks=0;void Check(bool value,string text){checks++;if(!value)throw new Exception(text);}
  var bytes=Rig.SourceBytes(Rig.BuiltinId);Check(bytes.Length==87840,"Embedded skeleton complete");
  Check(Convert.ToHexStringLower(SHA256.HashData(bytes))=="438730b8c891c9aa5db0eb9b8e54e1ec550ddf4f36fcefdd895ca4feb746f0d9","Embedded skeleton identical to verified rig");
  var embedded=Rig.Load(Rig.BuiltinId);Check(embedded.Names.Length==126&&embedded.Names.Contains("Camera3rd [Cam3]"),"Embedded skeleton and camera bone load from memory");
  Check(Rig.Load("").Names.SequenceEqual(embedded.Names),"Empty skeleton uses embedded default");
  string missing=Path.Combine(output,"missing","skeleton.hkx");Check(Rig.ResolveReference(missing)==Path.GetFullPath(missing),"Missing external skeleton requires explicit selection");
  var project=SongProject.Load(example);project.Skeleton=missing;project.CameraBone=999;
  // Write a legacy-style broken reference exactly as a user might receive it.
  string legacy=Path.Combine(output,"legacy-missing-rig.danceproj");File.WriteAllText(legacy,JsonSerializer.Serialize(project,SongProject.Json));
  var recovered=SongProject.Load(legacy);Check(recovered.Skeleton==Path.GetFullPath(missing)&&recovered.CameraBone==999,"Missing path and manual camera choice are not silently replaced");recovered.Skeleton=Rig.BuiltinId;recovered.CameraBone=-1;recovered.CameraBoneManual=false;
  recovered.Save(Path.Combine(output,"saved-builtin.danceproj"));var restored=SongProject.Load(Path.Combine(output,"saved-builtin.danceproj"));Check(restored.Skeleton==Rig.BuiltinId,"Built-in reference roundtrips without filesystem URI conversion");
  string custom=Path.Combine(output,"custom-skeleton.hkx");File.WriteAllBytes(custom,bytes);project.Skeleton=custom;project.CameraBone=4;project.Save(Path.Combine(output,"custom.danceproj"));
  var chosen=SongProject.Load(Path.Combine(output,"custom.danceproj"));Check(chosen.Skeleton==Path.GetFullPath(custom)&&chosen.CameraBone==4,"Existing user-selected custom skeleton preserved");
  Check(Rig.Load(chosen.Skeleton).Names.SequenceEqual(embedded.Names),"Custom skeleton path still supported");
  string invalid=Path.Combine(output,"invalid.hkx");File.WriteAllText(invalid,"not a skeleton");bool rejected=false;try{Rig.Load(invalid);}catch{rejected=true;}Check(rejected,"Existing invalid custom file is not silently replaced");
  recovered.ExportOut=2;var rig=Rig.Load(recovered.Skeleton);var game=GameExporter.Export(output,recovered,MotionClip.Load(recovered.Dance.Path),MotionClip.Load(recovered.Camera.Path),WaveAudio.Load(recovered.Audio.Path),rig);
  var exported=SongProject.Load(Path.Combine(game,"source.danceproj"));Check(exported.Skeleton==Rig.BuiltinId,"Game source project keeps portable embedded reference");
  File.WriteAllText(Path.Combine(output,"validation.json"),JsonSerializer.Serialize(new{passed=true,checks,embeddedBytes=bytes.Length,bones=embedded.Names.Length,needsExternalSkeleton=false,gameTested=false},SongProject.Json));
 }
}
