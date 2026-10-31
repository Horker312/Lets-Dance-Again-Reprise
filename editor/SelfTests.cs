using System.IO;
using System.Text.Json;
using System.Security.Cryptography;
namespace DanceStudio;
public static class SelfTests {
 public static void Run(string output) {
  Directory.CreateDirectory(output);output=Path.GetFullPath(output);int checks=0;
  void Check(bool ok,string text){checks++;if(!ok)throw new Exception(text);}
  string mods=@"C:\Modding\MO2\mods\Lets Dance Again K Girl Edtion";
  string folder=Path.Combine(mods,@"Meshes\actors\character\animations\Lets_dance_again");
  string dancePath=Path.Combine(folder,"Booo.hkx"),camPath=Path.Combine(folder,"c61.hkx"),audioPath=Path.Combine(mods,@"sound\fx\LDA\Booo.wav");
  string[] inputs=[dancePath,camPath,audioPath,MainWindow.DefaultSkeleton];
  var hashes=inputs.ToDictionary(x=>x,x=>Convert.ToHexStringLower(SHA256.HashData((Rig.IsBuiltin(x)?Rig.SourceBytes(x):File.ReadAllBytes(x)))));
  var dance=MotionClip.Load(dancePath);var camera=MotionClip.Load(camPath);var audio=WaveAudio.Load(audioPath);var rig=Rig.Load(MainWindow.DefaultSkeleton);
  Check(dance.Frames==4801&&camera.Frames==4801,"Actual LDA frame count");
  Check(Math.Abs(dance.Duration-200)<.001,"Actual HKX duration");
  var project=new SongProject{Title="Booo · 싱크 편집 시험",Skeleton=MainWindow.DefaultSkeleton,Dance=new(){Path=dancePath,Out=dance.Duration},Camera=new(){Path=camPath,Out=camera.Duration},Audio=new(){Path=audioPath,Out=audio.Duration},ExportOut=Math.Min(200,audio.Duration)};
  project.CameraBone=Array.FindIndex(rig.Names,x=>x.Contains("Camera3rd"));
  Check(project.CameraBone>=0,"Camera3rd node exists");
  var cameraPose=camera.World(rig,0)[project.CameraBone];var target=System.Numerics.Vector3.Normalize(new System.Numerics.Vector3(0,0,85)-cameraPose.T);
  project.CameraForward=new Dictionary<string,System.Numerics.Vector3>{{"+Y",System.Numerics.Vector3.UnitY},{"-Y",-System.Numerics.Vector3.UnitY},{"+X",System.Numerics.Vector3.UnitX},{"-X",-System.Numerics.Vector3.UnitX}}.MaxBy(x=>System.Numerics.Vector3.Dot(System.Numerics.Vector3.Transform(x.Value,cameraPose.R),target)).Key;
  project.Save(Path.Combine(output,"Booo-preview.danceproj"));var loaded=SongProject.Load(Path.Combine(output,"Booo-preview.danceproj"));
  Check(loaded.Dance.Path==dancePath&&loaded.Camera.Path==camPath,"Relative project paths roundtrip");
  Check(loaded.Id==project.Id,"Stable song identifier");
  Check(audio.Duration>1&&audio.Peaks.Max()>0,"Real WAV waveform decoded");
  for(double t=0;t<200;t+=.417){var w=dance.World(rig,t);Check(w.Length==rig.Names.Length&&w.All(p=>float.IsFinite(p.T.X+p.T.Y+p.T.Z)),"Finite interpolated skeleton pose");}
  for(double t=-1;t<5;t+=.05){var e=new TrackEdit{In=.5,Out=3,Start=.25};Check(Math.Abs(e.SourceTime(t)-(t+.25))<1e-10,"Timeline sign and mapping");}
  var trim=SongProject.Load(Path.Combine(output,"Booo-preview.danceproj"));trim.ExportIn=1.125;trim.ExportOut=3.137;trim.Dance.In=.375;trim.Dance.Start=.125;trim.Camera.Start=.25;trim.Audio.Start=.5;
  string package=Exporter.Export(output,trim,dance,camera,audio,rig);
  var exported=MotionClip.Load(Path.Combine(package,"tracks/dance.hkx"));var expCam=MotionClip.Load(Path.Combine(package,"tracks/camera.hkx"));var expAudio=WaveAudio.Load(Path.Combine(package,"tracks/music.wav"));
  double duration=trim.ExportOut-trim.ExportIn;
  Check(Math.Abs(exported.Duration-duration)<1e-5,"Trimmed HKX duration preserved");
  Check(Math.Abs(expCam.Duration-exported.Duration)<1e-6,"Camera and dance durations aligned");
  Check(Math.Abs(expAudio.Duration-duration)<=1.0/audio.SampleRate,"Audio duration within one sample");
  var first=exported.At(0);var expected=dance.At(trim.Dance.SourceTime(trim.ExportIn));
  for(int i=0;i<first.Length;i++)Check(System.Numerics.Vector3.Distance(first[i].T,expected[i].T)<.002&&Math.Abs(System.Numerics.Quaternion.Dot(first[i].R,expected[i].R))>.9999,"Trimmed first pose matches source");
  var last=exported.At(duration);var lastExpected=dance.At(trim.Dance.SourceTime(trim.ExportOut));
  for(int i=0;i<last.Length;i++)Check(System.Numerics.Vector3.Distance(last[i].T,lastExpected[i].T)<.002,"Trimmed final pose matches source");
  int srcFrame=(int)Math.Round(trim.Audio.SourceTime(trim.ExportIn)*audio.SampleRate);
  Check(expAudio.Pcm.Take(128).SequenceEqual(audio.Pcm.Skip(srcFrame*audio.BlockAlign).Take(128)),"Audio offset samples match");
  var silence=new TrackEdit{In=0,Out=audio.Duration,Start=1};string silencePath=Path.Combine(output,"silence-padding.wav");audio.Bake(silencePath,silence,0,1.5);var padded=WaveAudio.Load(silencePath);
  Check(padded.Pcm.Take(audio.SampleRate*audio.BlockAlign).All(x=>x==0),"Delayed audio starts with silence");
  Check(padded.Pcm.Skip(audio.SampleRate*audio.BlockAlign).Take(128).SequenceEqual(audio.Pcm.Take(128)),"Delayed source begins at expected sample");
  foreach(var x in hashes)Check(Convert.ToHexStringLower(SHA256.HashData((Rig.IsBuiltin(x.Key)?Rig.SourceBytes(x.Key):File.ReadAllBytes(x.Key))))==x.Value,"Original asset unchanged");
  bool rejected=false;try{new TrackEdit{In=3,Out=2}.Validate(10);}catch(InvalidDataException){rejected=true;}Check(rejected,"Invalid trim rejected");
  rejected=false;try{new TrackEdit{In=0,Out=double.NaN}.Validate(10);}catch(InvalidDataException){rejected=true;}Check(rejected,"Nonfinite trim rejected");
  var legacy=MotionClip.Load(Path.Combine(folder,"c3.hkx"));
  Check(legacy.Animation.numberOfFloatTracks==4&&legacy.FloatSamples.Length==legacy.Frames,"Legacy multi-block float channels decoded");
  string floatPath=Path.Combine(output,"legacy-float-trim.hkx");legacy.Bake(floatPath,new(){In=0,Out=legacy.Duration},.17,1.43,24);var floatResult=MotionClip.Load(floatPath);
  Check(floatResult.Animation.numberOfFloatTracks==4,"Float channel count preserved");
  Check(floatResult.FloatsAt(0).Zip(legacy.FloatsAt(.17)).All(pair=>Math.Abs(pair.First-pair.Second)<1e-6),"Float channel values preserved");
  Check(floatResult.Container.bindings!.Single().floatTrackToFloatSlotIndices.SequenceEqual(legacy.Container.bindings!.Single().floatTrackToFloatSlotIndices),"Float binding preserved");
  string game=GameExporter.Export(output,trim,dance,camera,audio,rig);string gameManifest=Directory.GetFiles(game,"song.json",SearchOption.AllDirectories).Single();using(var doc=JsonDocument.Parse(File.ReadAllText(gameManifest))){var root=doc.RootElement;Check(root.GetProperty("schemaVersion").GetInt32()==3,"Game runtime schema version");Check(root.GetProperty("id").GetString()==trim.Id&&root.GetProperty("token").GetUInt32() is >0 and <=16777215,"Stable song ID and float-exact revision selector");string body=Path.Combine(game,root.GetProperty("dance").GetString()!);string female=Path.Combine(game,root.GetProperty("danceFemale").GetString()!);Check(File.ReadAllBytes(body).SequenceEqual(File.ReadAllBytes(female)),"Both vanilla idle routes contain the same dance");Check(Math.Abs(MotionClip.Load(body).Duration-duration)<1e-5,"Game HKX shares common timeline");using var config=JsonDocument.Parse(File.ReadAllText(Path.Combine(Path.GetDirectoryName(body)!,"config.json")));Check(config.RootElement.GetProperty("conditions")[1].GetProperty("Value A").GetProperty("form").GetProperty("pluginName").GetString()=="DanceStudioData.esl","OAR condition references runtime selector");var source=SongProject.Load(Path.Combine(Path.GetDirectoryName(gameManifest)!,"source.danceproj"));Check(source.Dance.Path==trim.Dance.Path&&source.Camera.Path==trim.Camera.Path,"Game package project still resolves original sources");foreach(var hash in root.GetProperty("hashes").EnumerateObject()){string path=Path.Combine(hash.Name.StartsWith("meshes/")?game:Path.GetDirectoryName(gameManifest)!,hash.Name);Check(Convert.ToHexStringLower(SHA256.HashData(File.ReadAllBytes(path)))==hash.Value.GetString(),"Game manifest hashes match actual output");}}
  var validPose=dance.Samples[0][0];bool invalidPoseRejected=false;try{dance.Samples[0][0]=new(new(float.NaN,0,0),validPose.R,validPose.S);try{GameExporter.Export(output,trim,dance,camera,audio,rig);}catch(InvalidDataException){invalidPoseRejected=true;}}finally{dance.Samples[0][0]=validPose;}Check(invalidPoseRejected,"Nonfinite dance rejected before any game files are created");
  foreach(var x in hashes)Check(Convert.ToHexStringLower(SHA256.HashData((Rig.IsBuiltin(x.Key)?Rig.SourceBytes(x.Key):File.ReadAllBytes(x.Key))))==x.Value,"Game export preserves original assets");
  File.WriteAllText(Path.Combine(output,"validation.json"),JsonSerializer.Serialize(new{passed=true,checks,sourceHashes=hashes,danceFrames=dance.Frames,cameraFrames=camera.Frames,duration=exported.Duration,audioDuration=expAudio.Duration,cameraBone=rig.Names[project.CameraBone],forward=project.CameraForward,skeletonBones=rig.Names.Length,package,gameRuntimeTested=false},SongProject.Json));
 }
 public static void Batch(string input,string output){
  Directory.CreateDirectory(output);using var doc=JsonDocument.Parse(File.ReadAllText(input));var rig=Rig.Load(MainWindow.DefaultSkeleton);var report=new List<object>();
  foreach(var row in doc.RootElement.EnumerateArray()){
   int index=row.GetProperty("index").GetInt32();var clips=new List<object>();
   foreach(string track in new[]{"dance","camera"}){
    string path=row.GetProperty(track).GetString()!;string before=Convert.ToHexStringLower(SHA256.HashData(File.ReadAllBytes(path)));
    var clip=MotionClip.Load(path);var pose=clip.World(rig,Math.Min(clip.Duration,.371));
    if(pose.Any(p=>!float.IsFinite(p.T.X+p.T.Y+p.T.Z)))throw new Exception("Nonfinite skeleton: "+path);
    double begin=Math.Min(.05,clip.Duration*.1),end=Math.Min(begin+.333,clip.Duration);
    string cut=Path.Combine(output,$"{index:000}-{track}.hkx");clip.Bake(cut,new(){Out=clip.Duration},begin,end,24);var read=MotionClip.Load(cut);
    if(Math.Abs(read.Duration-(end-begin))>1e-5||read.Tracks!=clip.Tracks)throw new Exception("Trim duration/tracks: "+path);
    var expected=clip.At(begin);var actual=read.At(0);
    for(int i=0;i<actual.Length;i++)if(System.Numerics.Vector3.Distance(expected[i].T,actual[i].T)>.002||Math.Abs(System.Numerics.Quaternion.Dot(expected[i].R,actual[i].R))<.9999)throw new Exception("First pose changed: "+path);
    if(!clip.Container.bindings!.Single().transformTrackToBoneIndices.SequenceEqual(read.Container.bindings!.Single().transformTrackToBoneIndices))throw new Exception("Bone binding changed: "+path);
    if(Convert.ToHexStringLower(SHA256.HashData(File.ReadAllBytes(path)))!=before)throw new Exception("Original changed: "+path);
    clips.Add(new{track,path,frames=clip.Frames,duration=clip.Duration,trimDuration=read.Duration,passed=true});
   }
   report.Add(new{index,clips});
  }
  File.WriteAllText(Path.Combine(output,"batch-validation.json"),JsonSerializer.Serialize(new{passed=true,entries=report.Count,clips=report.Count*2,report,originalsUnchanged=true},SongProject.Json));
 }
}
