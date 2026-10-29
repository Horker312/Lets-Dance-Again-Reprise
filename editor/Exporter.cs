using System.IO;
using System.Security.Cryptography;
using System.Text.Json;
namespace DanceStudio;
public static class Exporter {
 public static string Export(string parent,SongProject p,MotionClip? dance,MotionClip? camera,WaveAudio audio,Rig rig,bool gameAudio=false) {
  ProjectRules.Validate(p,true);
  if(dance==null&&camera==null)throw new InvalidDataException("춤 또는 카메라 중 하나는 필요합니다.");
  rig.EnsureSourceUnchanged();if(dance!=null){dance.EnsureSourceUnchanged();dance.ValidateRig(rig);p.Dance.Validate(dance.Duration);}if(camera!=null){FovPolicy.Value(p,camera,p.ExportIn);camera.EnsureSourceUnchanged();camera.ValidateRig(rig);CameraAnalysis.Validate(camera,rig,p.CameraBone);p.Camera.Validate(camera.Duration);if(camera.Animation.extractedMotion!=null)throw new NotSupportedException("카메라 루트 모션 출력은 미지원입니다.");}p.Audio.Validate(audio.Duration);
  parent=System.IO.Path.GetFullPath(parent);
  string folder=System.IO.Path.Combine(parent,"DanceStudio-"+DateTime.Now.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N")[..6]);
  Directory.CreateDirectory(folder);bool success=false;
  try {
   string tracks=System.IO.Path.Combine(folder,"tracks");Directory.CreateDirectory(tracks);
   double danceFps=p.DanceOutputFps.Resolve(dance,p.Dance),cameraFps=p.CameraSampleFps.Resolve(camera,p.Camera);
   bool preserveDance=dance!=null&&p.DanceOutputFps.Mode=="Auto"&&OutputRate.Unedited(dance,p.Dance,p.ExportIn,p.ExportOut,p.MirrorPlayer);
   if(dance!=null)dance.Bake(System.IO.Path.Combine(tracks,"dance.hkx"),p.Dance,p.ExportIn,p.ExportOut,danceFps,p.MirrorPlayer?t=>dance.Edits(rig).Dance(t,true,0):null,rig,p.Verification,preserveDance);
   if(camera!=null)camera.Bake(System.IO.Path.Combine(tracks,"camera.hkx"),p.Camera,p.ExportIn,p.ExportOut,cameraFps,p.MirrorCamera||p.PlayerYaw!=0?t=>camera.Edits(rig).Camera(t,p.CameraBone,p.CameraYaw,p.CameraForward,p.MirrorCamera,p.PlayerYaw):null,rig,p.Verification);
   audio.Bake(System.IO.Path.Combine(tracks,"music.wav"),p.Audio,p.ExportIn,p.ExportOut,gameAudio&&p.ApplyVolumeInGame?p.PreviewVolume:1);
   if(camera!=null){
   var samples=new List<object>();int count=Math.Max(2,(int)Math.Ceiling((p.ExportOut-p.ExportIn)*cameraFps)+1);
   double duration=p.ExportOut-p.ExportIn;var stamps=Enumerable.Range(0,count).Select(i=>i/cameraFps).Where(t=>t<duration-1e-10).Append(duration).ToArray();count=stamps.Length;
   for(int i=0;i<count;i++) {
    double t=stamps[i];var pose=camera.World(rig,Math.Clamp(p.Camera.SourceTime(p.ExportIn+t),p.Camera.In,p.Camera.Out))[p.CameraBone];
    pose=PoseEdits.StageCamera(pose,p.CameraYaw,p.CameraForward,p.MirrorCamera,p.PlayerYaw);
    samples.Add(new{time=t,position=new[]{pose.T.X,pose.T.Y,pose.T.Z},rotation=new[]{pose.R.X,pose.R.Y,pose.R.Z,pose.R.W}});
   }
   File.WriteAllText(System.IO.Path.Combine(tracks,"camera.samples.json"),JsonSerializer.Serialize(new{schemaVersion=1,coordinates="Skyrim Z-up, source rig world space",bone=rig.Names[p.CameraBone],forward="+Y",heightOffset=p.CameraHeightOffset,fov=p.Fov,yawDegrees=0,duration=p.ExportOut-p.ExportIn,samples},SongProject.Json));
   File.WriteAllText(System.IO.Path.Combine(tracks,"camera-samples.verification.json"),JsonSerializer.Serialize(CameraSampleComparison.Compare(System.IO.Path.Combine(tracks,"camera.samples.json"),p,camera,rig,gameAudio),SongProject.Json));
   }
   var hashes=Directory.GetFiles(tracks).ToDictionary(x=>"tracks/"+System.IO.Path.GetFileName(x),x=>Convert.ToHexStringLower(SHA256.HashData(File.ReadAllBytes(x))));
   var manifest=new Dictionary<string,object>{{"schemaVersion",1},{"id",p.Id},{"title",p.Title},{"duration",p.ExportOut-p.ExportIn},{"music","tracks/music.wav"},{"hashes",hashes},{"runtimeStatus","editor-export-only"}};if(dance!=null){manifest["dance"]="tracks/dance.hkx";manifest["danceOutputFps"]=danceFps;}if(camera!=null){manifest["camera"]="tracks/camera.hkx";manifest["cameraSamples"]="tracks/camera.samples.json";manifest["cameraSampleFps"]=cameraFps;}File.WriteAllText(System.IO.Path.Combine(folder,"song.json"),JsonSerializer.Serialize(manifest,SongProject.Json));
   rig.EnsureSourceUnchanged();dance?.EnsureSourceUnchanged();camera?.EnsureSourceUnchanged();success=true;return folder;
  } finally {
   if(!success&&Directory.Exists(folder)&&System.IO.Path.GetFullPath(folder).StartsWith(parent.TrimEnd(System.IO.Path.DirectorySeparatorChar)+System.IO.Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))Directory.Delete(folder,true);
  }
 }
}
