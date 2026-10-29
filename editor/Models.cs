using System.IO;
using System.Text.Json;
namespace DanceStudio;

public sealed class TrackEdit {
 public string Path {get;set;}="";
 public int? HkxBindingIndex {get;set;}=null;
 public string? IdentityRigProof {get;set;}
 public double In {get;set;}
 public double Out {get;set;}
 public double Start {get;set;}
 public bool HoldEnd {get;set;}=true;
 public double Speed {get;set;}=1;
 [System.Text.Json.Serialization.JsonIgnore]
 public double TimelineDuration=>(Out-In)/Speed;
 [System.Text.Json.Serialization.JsonIgnore]
 public double TimelineEnd=>Start+TimelineDuration;
 public double SourceTime(double timeline)=>In+(timeline-Start)*Speed;
 public void MoveStart(double original,double delta){Start=Math.Max(0,original+delta);}
 public void TrimStart(double originalIn,double originalOut,double originalStart,double timelineDelta,double fps){
  originalStart=Math.Max(0,originalStart);double gap=Math.Min(Math.Max(0,originalOut-originalIn),Speed/Math.Max(1,fps));double lower=Math.Max(-originalIn,-originalStart*Speed),upper=Math.Max(lower,originalOut-originalIn-gap);double sourceDelta=Math.Clamp(timelineDelta*Speed,lower,upper);In=originalIn+sourceDelta;Start=Math.Max(0,originalStart+sourceDelta/Speed);
 }
 public void TrimEnd(double originalIn,double originalOut,double timelineDelta,double sourceDuration){double upper=Math.Max(originalIn,sourceDuration),lower=Math.Min(upper,originalIn+.001);Out=Math.Clamp(originalOut+timelineDelta*Speed,lower,upper);}
 public void Validate(double sourceDuration) {
  if(!double.IsFinite(Speed)||Speed<.05||Speed>8)throw new InvalidDataException("배속은 0.05~8.0 사이의 숫자여야 합니다.");
  if(!double.IsFinite(In)||!double.IsFinite(Out)||!double.IsFinite(Start)||In<0||Out<=In||Out>sourceDuration+0.0001)
   throw new InvalidDataException("트랙의 시작·끝·시간차를 확인하세요. 끝은 시작보다 커야 합니다.");
 }
}
public sealed class SongProject {
 public int SchemaVersion {get;set;}=5;
 public string Id {get;set;}=Guid.NewGuid().ToString("N");
 public Dictionary<string,string> PackageIds {get;set;}=new(){["both"]=Guid.NewGuid().ToString("N"),["dance"]=Guid.NewGuid().ToString("N"),["camera"]=Guid.NewGuid().ToString("N")};
 public static string PackageKind(bool dance,bool camera)=>dance?(camera?"both":"dance"):"camera";
 public string PackageId(bool dance,bool camera){string kind=PackageKind(dance,camera);PackageIds??=new();if(!PackageIds.TryGetValue(kind,out var id)||string.IsNullOrWhiteSpace(id))PackageIds[kind]=id=Guid.NewGuid().ToString("N");return id;}
 public void RemoveTrack(int index){switch(index){case 0:Dance=new();break;case 1:Camera=new();break;case 2:Audio=new();break;default:throw new ArgumentOutOfRangeException(nameof(index));}}

 public string Title {get;set;}="새 곡";
 public string Skeleton {get;set;}=Rig.BuiltinId;
 public TrackEdit Dance {get;set;}=new();
 public TrackEdit Camera {get;set;}=new();
 public TrackEdit Audio {get;set;}=new();
 public double ExportIn {get;set;}
 public double ExportOut {get;set;}=10;
 public OutputRate DanceOutputFps {get;set;}=new();
 public OutputRate CameraSampleFps {get;set;}=new();
 [System.Text.Json.Serialization.JsonIgnore]
 public int Fps {get=>24;set{DanceOutputFps=new(){Mode="Manual",Value=value};CameraSampleFps=new(){Mode="Manual",Value=value};}}
 public double Fov {get;set;}=45;
 public double CameraHeightOffset {get;set;}
 public string FovMode {get;set;}="Manual";
 public double PreviewAspectRatio {get;set;}=16.0/9;
 public double PreviewVolume {get;set;}=.7;
 public bool ApplyVolumeInGame {get;set;}
 public bool MirrorPlayer {get;set;}
 public bool MirrorCamera {get;set;}
 public double PlayerYaw {get;set;}
 public double CameraYaw {get;set;}
 public string CameraForward {get;set;}="+Y";
 public int CameraBone {get;set;}=-1;
 public bool CameraBoneManual {get;set;}
 public bool CameraForwardManual {get;set;}
 public MotionCheckOptions Verification {get;set;}=new();
 public static JsonSerializerOptions Json {get;}=new(){WriteIndented=true};
 public void Save(string path) {
  ProjectRules.Validate(this);
  string destination=System.IO.Path.GetFullPath(path);
  if(new[]{Dance.Path,Camera.Path,Audio.Path,Skeleton}.Where(x=>!string.IsNullOrEmpty(x)&&!Rig.IsBuiltin(x)).Any(x=>string.Equals(System.IO.Path.GetFullPath(x),destination,StringComparison.OrdinalIgnoreCase)))throw new IOException("원본 파일 경로에는 프로젝트를 저장할 수 없습니다.");
  var copy=JsonSerializer.Deserialize<SongProject>(JsonSerializer.Serialize(this,Json))!;copy.SchemaVersion=5;
  string folder=System.IO.Path.GetDirectoryName(System.IO.Path.GetFullPath(path))!;
  foreach(var t in new[]{copy.Dance,copy.Camera,copy.Audio})
   if(t.Path.Length>0)t.Path=System.IO.Path.GetRelativePath(folder,t.Path);
  copy.Skeleton=Rig.IsBuiltin(copy.Skeleton)?Rig.BuiltinId:copy.Skeleton;if(!Rig.IsBuiltin(copy.Skeleton))copy.Skeleton=System.IO.Path.GetRelativePath(folder,copy.Skeleton);
  AtomicWrite(path,JsonSerializer.Serialize(copy,Json));
 }
 public static SongProject Load(string path) {
  if(new FileInfo(path).Length>1024*1024)throw new InvalidDataException("프로젝트 파일이 너무 큽니다.");
  var p=JsonSerializer.Deserialize<SongProject>(File.ReadAllText(path),Json)??throw new InvalidDataException("프로젝트를 읽을 수 없습니다.");
  if(p.SchemaVersion!=5)throw new InvalidDataException("이 편집기는 독립 FPS 프로젝트 버전 5를 사용합니다. 이전 프로젝트는 새 프로젝트로 다시 만들어주세요.");
  using var raw=JsonDocument.Parse(File.ReadAllText(path));if(!raw.RootElement.TryGetProperty("CameraBoneManual",out _))p.CameraBoneManual=p.CameraBone>=0;if(!raw.RootElement.TryGetProperty("CameraForwardManual",out _))p.CameraForwardManual=raw.RootElement.TryGetProperty("CameraForward",out _);
  if(p.SchemaVersion<4)p.FovMode="Manual";
  if(p.FovMode=="Auto")throw new InvalidDataException("이 버전에서는 확인된 원본 HKX FOV 추출기가 없습니다. 프로젝트의 FOV 모드를 Manual로 지정해야 합니다.");
  ProjectRules.Validate(p);
  string folder=System.IO.Path.GetDirectoryName(System.IO.Path.GetFullPath(path))!;
  foreach(var t in new[]{p.Dance,p.Camera,p.Audio})
   if(t.Path.Length>0)t.Path=System.IO.Path.GetFullPath(t.Path,folder);
  if(!Rig.IsBuiltin(p.Skeleton)){var requested=System.IO.Path.GetFullPath(p.Skeleton,folder);p.Skeleton=requested;}else p.Skeleton=Rig.BuiltinId;
  return p;
 }
 public static void AtomicWrite(string path,string content) {
  var temp=path+".tmp-"+Guid.NewGuid().ToString("N");
  try{File.WriteAllText(temp,content);File.Move(temp,path,true);}finally{if(File.Exists(temp))File.Delete(temp);}
 }
}
