using System.IO;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
namespace DanceStudio;
public static class TransformTests {
 public static void Run(string projectPath,string output){
  Directory.CreateDirectory(output);int count=0;float largest=0;
  void Check(bool b,string message){count++;if(!b)throw new Exception(message);}
  void Near(Vector3 a,Vector3 b,string text){float error=Vector3.Distance(a,b);largest=Math.Max(largest,error);Check(error<.03f,text+" error="+error);}
  void Reject(Action a,string text){bool threw=false;try{a();}catch{threw=true;}Check(threw,text);}
  var p=SongProject.Load(projectPath);var rig=Rig.Load(p.Skeleton);var dance=MotionClip.Load(p.Dance.Path);var camera=MotionClip.Load(p.Camera.Path);var audio=WaveAudio.Load(p.Audio.Path);
  if(p.CameraBone<0)p.CameraBone=Array.FindIndex(rig.Names,x=>x.Contains("Camera3rd"));Check(p.CameraBone>=0,"Actual camera bone found");
  var files=new[]{p.Dance.Path,p.Camera.Path,p.Audio.Path,p.Skeleton};var hashes=files.ToDictionary(x=>x,x=>Convert.ToHexStringLower(SHA256.HashData((Rig.IsBuiltin(x)?Rig.SourceBytes(x):File.ReadAllBytes(x)))));
  Check(!p.MirrorPlayer&&!p.MirrorCamera&&p.PlayerYaw==0,"Legacy project defaults");
  var edits=dance.Edits(rig);Check(edits.PairedBones>40,"Actual XPMSE left/right pairs found");
  for(int b=0;b<rig.Names.Length;b++){string opposite=PoseEdits.Counterpart(rig.Names[b]);Check(PoseEdits.Counterpart(opposite)==rig.Names[b],"Bone name mapping is involutive");}
  string[] body={"NPC L Hand [LHnd]","NPC L Foot [Lft ]","NPC R Hand [RHnd]","NPC R Foot [Rft ]","NPC Head [Head]"};
  foreach(double time in new[]{0.0,1.5,30.0,70.0,170.0}){
   var raw=dance.World(rig,time);var mirrored=edits.World(edits.Dance(time,true,0));var rotated=edits.World(edits.Dance(time,false,90));
   foreach(string name in body){int b=Array.IndexOf(rig.Names,name);int opposite=Array.IndexOf(rig.Names,PoseEdits.Counterpart(name));if(opposite<0)opposite=b;
    Near(mirrored[b].T,PoseEdits.Reflect(raw[opposite].T),"Opposite body joint is reflected");
    Near(rotated[b].T,Vector3.Transform(raw[b].T,PoseEdits.Yaw(90)),"Yaw is about world Z");
    Check(Math.Abs(rotated[b].T.Z-raw[b].T.Z)<.001,"Rotation preserves vertical height");
   }
   var tracks=edits.Dance(time,true,0);var twice=new MotionClip{Duration=1,Tracks=dance.Tracks,BoneMap=dance.BoneMap.ToArray(),Samples=[tracks,tracks]};
   var restored=twice.Edits(rig).World(twice.Edits(rig).Dance(0,true,0));
   foreach(string name in body){int b=Array.IndexOf(rig.Names,name);Near(restored[b].T,raw[b].T,"Double mirror restores joint position");Check(Math.Abs(Quaternion.Dot(restored[b].R,raw[b].R))>.99999,"Double mirror restores joint orientation");}
   var combined=edits.World(edits.Dance(time,true,90));foreach(string name in body){int b=Array.IndexOf(rig.Names,name);Near(combined[b].T,Vector3.Transform(mirrored[b].T,PoseEdits.Yaw(90)),"Mirror precedes yaw");}
  }
  foreach(string axis in new[]{"+X","-X","+Y","-Y","+Z","-Z"}){
   var source=new Pose(new(12,70,100),Quaternion.Normalize(Quaternion.CreateFromYawPitchRoll(.4f,.3f,.2f)),Vector3.One);
   var normal=PoseEdits.CameraView(source,23,axis,false);var mirror=PoseEdits.CameraView(source,23,axis,true);
   Near(mirror.T,PoseEdits.Reflect(normal.T),"Camera position mirror");
   Near(Vector3.Transform(Vector3.UnitY,mirror.R),PoseEdits.Reflect(Vector3.Transform(Vector3.UnitY,normal.R)),"Camera forward mirror");
   Near(Vector3.Transform(Vector3.UnitZ,mirror.R),PoseEdits.Reflect(Vector3.Transform(Vector3.UnitZ,normal.R)),"Camera roll/up mirror");
   Check(Math.Abs(mirror.R.Length()-1)<1e-5,"Camera remains a valid proper rotation");
  }
  foreach(double yaw in new[]{-180.0,-90.0,0.0,37.25,90.0,180.0})foreach(bool mirror in new[]{false,true}){
   var cameraSource=camera.World(rig,30)[p.CameraBone];var previewCamera=PoseEdits.CameraView(cameraSource,p.CameraYaw,p.CameraForward,mirror);var exportedCamera=PoseEdits.StageCamera(cameraSource,p.CameraYaw,p.CameraForward,mirror,yaw);
   var previewDance=dance.EditedWorld(rig,30,mirror,yaw);var gameDance=dance.EditedWorld(rig,30,mirror,0);
   foreach(string name in body){int b=Array.IndexOf(rig.Names,name);var before=Vector3.Transform(previewDance[b].T-previewCamera.T,Quaternion.Inverse(previewCamera.R));var after=Vector3.Transform(gameDance[b].T-exportedCamera.T,Quaternion.Inverse(exportedCamera.R));Near(before,after,"Stage rebase preserves camera-relative joint coordinates");}
  }
  Reject(()=>PoseEdits.Relative(new(Vector3.Zero,Quaternion.Identity,Vector3.Zero),new(Vector3.Zero,Quaternion.Identity,Vector3.One)),"Zero scale rejected");
  p.PlayerYaw=double.PositiveInfinity;Reject(()=>PoseEdits.Validate(p),"Nonfinite yaw rejected");p.PlayerYaw=90;p.MirrorPlayer=p.MirrorCamera=true;
  p.ExportIn=30;p.ExportOut=32;p.Save(Path.Combine(output,"mirrored.danceproj"));
  var loaded=SongProject.Load(Path.Combine(output,"mirrored.danceproj"));Check(loaded.MirrorPlayer&&loaded.MirrorCamera&&loaded.PlayerYaw==90,"Transform save/reload");
  var asset=Exporter.Export(output,p,dance,camera,audio,rig);var bakedDance=MotionClip.Load(Path.Combine(asset,"tracks/dance.hkx"));var bakedCamera=MotionClip.Load(Path.Combine(asset,"tracks/camera.hkx"));
  var cameraJson=JsonDocument.Parse(File.ReadAllText(Path.Combine(asset,"tracks/camera.samples.json"))).RootElement;
  Check(cameraJson.GetProperty("forward").GetString()=="+Y"&&cameraJson.GetProperty("yawDegrees").GetDouble()==0,"Camera mirror baked to canonical basis exactly once");
  for(int frame=0;frame<=48;frame+=12){double t=frame/24.0;var expected=dance.EditedWorld(rig,30+t,true,0);var actual=bakedDance.World(rig,t);foreach(string name in body){int b=Array.IndexOf(rig.Names,name);Near(actual[b].T,expected[b].T,"Baked dance matches preview");Check(Math.Abs(Quaternion.Dot(actual[b].R,expected[b].R))>.99999,"Baked orientation matches preview");}
   var raw=camera.World(rig,30+t)[p.CameraBone];var view=PoseEdits.StageCamera(raw,p.CameraYaw,p.CameraForward,true,p.PlayerYaw);var baked=bakedCamera.World(rig,t)[p.CameraBone];Near(baked.T,view.T,"Baked camera matches preview position");Near(Vector3.Transform(Vector3.UnitY,baked.R),Vector3.Transform(Vector3.UnitY,view.R),"Baked camera matches preview forward");
   var sample=cameraJson.GetProperty("samples")[frame];var pos=sample.GetProperty("position");Near(new(pos[0].GetSingle(),pos[1].GetSingle(),pos[2].GetSingle()),view.T,"Native camera samples match preview");
  }
  var game=GameExporter.Export(output,p,dance,camera,audio,rig);var manifest=Path.Combine(game,"song.json");
  var song=JsonDocument.Parse(File.ReadAllText(manifest)).RootElement;foreach(var hash in song.GetProperty("hashes").EnumerateObject()){string path=Path.Combine(hash.Name.StartsWith("meshes/")?game:Path.GetDirectoryName(manifest)!,hash.Name);Check(Convert.ToHexStringLower(SHA256.HashData((Rig.IsBuiltin(path)?Rig.SourceBytes(path):File.ReadAllBytes(path))))==hash.Value.GetString(),"Game track hash valid");}
  Check(song.GetProperty("schemaVersion").GetInt32()==3,"Self-contained song schema");Check(Directory.Exists(Path.Combine(game,"female"))&&!Directory.Exists(Path.Combine(game,"SKSE")),"One folder contains all song assets");
  File.Copy(Path.Combine(game,"source.danceproj"),Path.Combine(output,"moved-source.danceproj"),true);var moved=SongProject.Load(Path.Combine(output,"moved-source.danceproj"));Check(moved.Dance.Path==dance.Path&&moved.Camera.Path==camera.Path,"Source project still resolves originals after moving song folder");
  var config=JsonDocument.Parse(File.ReadAllText(Path.Combine(game,"config.json"))).RootElement;Check(config.GetProperty("priority").GetInt32()==int.MaxValue,"OAR priority hotfix preserved");
  foreach(var path in files)Check(hashes[path]==Convert.ToHexStringLower(SHA256.HashData((Rig.IsBuiltin(path)?Rig.SourceBytes(path):File.ReadAllBytes(path)))),"Original asset unchanged");
  Check(GameExporter.FolderName("CON: 한글 / 춤 😀",123).StartsWith("DS_123 - CON_ 한글 _ 춤 😀"),"Safe readable Korean/emoji folder name");
  var custom=SongProject.Load(projectPath);custom.Title=new string('춤',200);custom.ExportOut=2;var customGame=GameExporter.Export(output,custom,dance,camera,audio,rig);Check(JsonDocument.Parse(File.ReadAllText(Path.Combine(customGame,"song.json"))).RootElement.GetProperty("title").GetString()==custom.Title,"Custom Korean title kept in full");File.WriteAllText(Path.Combine(output,"korean-manifest.txt"),Path.Combine(customGame,"song.json"));
  var views=Enumerable.Range(0,97).Select(frame=>{var pose=camera.World(rig,30+frame/48.0)[p.CameraBone];var view=PoseEdits.StageCamera(pose,p.CameraYaw,p.CameraForward,true,p.PlayerYaw);var f=Vector3.Transform(Vector3.UnitY,view.R);return new{time=frame/48.0,position=new[]{view.T.X,view.T.Y,view.T.Z},forward=new[]{f.X,f.Y,f.Z}};});File.WriteAllText(Path.Combine(output,"expected-camera.json"),JsonSerializer.Serialize(views));
  File.WriteAllText(Path.Combine(output,"game-manifest.txt"),manifest);
  File.WriteAllText(Path.Combine(output,"validation.json"),JsonSerializer.Serialize(new{passed=true,checks=count,pairedBones=edits.PairedBones,largestPositionError=largest,actualBooo=true,actualHkxReimport=true,sourceAssetsUnchanged=true,gameTested=false},SongProject.Json));
 }
}

