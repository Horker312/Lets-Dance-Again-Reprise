using System.IO;
using System.Numerics;
namespace DanceStudio;
public sealed record CameraCandidate(int Bone,string Name,double PositionSpan,double RotationSpanDegrees,bool StaticIsValid);
public sealed record CameraProposal(int? Bone,string? AxisSuggestion,CameraCandidate[] Candidates,string Explanation);
public static class CameraAnalysis {
 public static void Validate(MotionClip clip,Rig rig,int bone){clip.ValidateRig(rig);if(bone<0||bone>=rig.Names.Length||!clip.BoneMap.Contains(bone))throw new InvalidDataException("카메라 기준 본이 실제 HKX Transform 트랙에 연결되지 않습니다. 본을 직접 선택하세요.");}
 public static CameraProposal Analyze(MotionClip clip,Rig rig){
  clip.ValidateRig(rig);var candidates=new List<CameraCandidate>();
  foreach(int bone in clip.BoneMap.Where(b=>(rig.Names[b].Contains("Cam",StringComparison.OrdinalIgnoreCase)||rig.Names[b].Contains("View",StringComparison.OrdinalIgnoreCase)||rig.Names[b].Contains("Lens",StringComparison.OrdinalIgnoreCase)))){var first=clip.World(rig,0)[bone];double position=0,angle=0;for(int i=1;i<=8;i++){var pose=clip.World(rig,clip.Duration*i/8)[bone];position=Math.Max(position,Vector3.Distance(first.T,pose.T));angle=Math.Max(angle,2*Math.Acos(Math.Clamp(Math.Abs((double)Quaternion.Dot(first.R,pose.R)),0,1))*180/Math.PI);}candidates.Add(new(bone,rig.Names[bone],position,angle,true));}
  var named=candidates.Where(c=>c.Name.Equals("Camera3rd [Cam3]",StringComparison.OrdinalIgnoreCase)||c.Name.Equals("Camera",StringComparison.OrdinalIgnoreCase)).ToArray();int? selected=named.Length==1?named[0].Bone:candidates.Count==1?candidates[0].Bone:null;
  string? suggestion=null;
  if(selected.HasValue){var axes=new Dictionary<string,Vector3>{{"+X",Vector3.UnitX},{"-X",-Vector3.UnitX},{"+Y",Vector3.UnitY},{"-Y",-Vector3.UnitY},{"+Z",Vector3.UnitZ},{"-Z",-Vector3.UnitZ}};var scores=axes.ToDictionary(x=>x.Key,_=>0.0);for(int i=0;i<=8;i++){var pose=clip.World(rig,clip.Duration*i/8)[selected.Value];var target=new Vector3(0,0,85)-pose.T;if(target.LengthSquared()<1)continue;target=Vector3.Normalize(target);foreach(var axis in axes)scores[axis.Key]+=Vector3.Dot(Vector3.Transform(axis.Value,pose.R),target);}var ranked=scores.OrderByDescending(x=>x.Value).ToArray();if(ranked[0].Value-ranked[1].Value>2)suggestion=ranked[0].Key;}
  return new(selected,suggestion,candidates.ToArray(),"본 후보는 실제 바인딩과 9개 시점의 위치·회전을 검사합니다. 정적 카메라도 허용합니다. 이름과 캐릭터 중심을 바라본다는 가정은 좌표 규약의 증명이 아닙니다. 전방축 추천은 자동 적용하지 않으며 수동값을 덮어쓰지 않습니다. 확정된 원본 좌표 규약은 아직 없습니다.");
 }
}
