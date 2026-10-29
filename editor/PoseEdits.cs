using System.IO;
using System.Numerics;
using System.Text.RegularExpressions;
namespace DanceStudio;

public static class PoseEdits {
 public static Vector3 Reflect(Vector3 v)=>new(-v.X,v.Y,v.Z);
 public static Quaternion Reflect(Quaternion q)=>Quaternion.Normalize(new(q.X,-q.Y,-q.Z,q.W));
 public static void Validate(SongProject p){if(!double.IsFinite(p.PlayerYaw)||Math.Abs(p.PlayerYaw)>3600)throw new InvalidDataException(UiText.T("t0310"));}
 public static Quaternion Yaw(double degrees)=>Quaternion.CreateFromAxisAngle(Vector3.UnitZ,(float)(degrees*Math.PI/180));
 public static string Counterpart(string name)=>Regex.Replace(name,@"\bL\b|\bR\b|\[L|\[R|Left|Right|left|right",m=>m.Value switch{"L"=>"R","R"=>"L","[L"=>"[R","[R"=>"[L","Left"=>"Right","Right"=>"Left","left"=>"right",_=>"left"});
 public static Pose CameraView(Pose source,double yaw,string axis,bool mirror){
  Vector3 f=axis switch{"+Y"=>Vector3.UnitY,"-Y"=>-Vector3.UnitY,"+X"=>Vector3.UnitX,"-X"=>-Vector3.UnitX,"+Z"=>Vector3.UnitZ,"-Z"=>-Vector3.UnitZ,_=>throw new InvalidDataException(UiText.T("t0311"))};
  var rotation=Quaternion.Normalize(source.R*Yaw(yaw));
  var forward=Vector3.Normalize(Vector3.Transform(f,rotation));var up=Vector3.Transform(Vector3.UnitZ,rotation);
  if(Math.Abs(Vector3.Dot(forward,up))>.99)up=Vector3.Transform(Vector3.UnitY,rotation);
  var position=source.T;if(mirror){position=Reflect(position);forward=Reflect(forward);up=Reflect(up);}
  var right=Vector3.Normalize(Vector3.Cross(forward,up));up=Vector3.Normalize(Vector3.Cross(right,forward));
  var matrix=new Matrix4x4(right.X,right.Y,right.Z,0,forward.X,forward.Y,forward.Z,0,up.X,up.Y,up.Z,0,0,0,0,1);
  return new(position,Quaternion.Normalize(Quaternion.CreateFromRotationMatrix(matrix)),Vector3.One);
 }
 // Re-express a camera in the stage basis selected by the player's yaw.
 public static Pose StageCamera(Pose source,double yaw,string axis,bool mirror,double stageYaw){
  var view=CameraView(source,yaw,axis,mirror);var inverse=Yaw(-stageYaw);
  return new(Vector3.Transform(view.T,inverse),Quaternion.Normalize(inverse*view.R),view.S);
 }
 public static Pose Compose(Pose parent,Pose local)=>new(parent.T+Vector3.Transform(local.T*parent.S,parent.R),Quaternion.Normalize(parent.R*local.R),parent.S*local.S);
 public static Pose Relative(Pose parent,Pose world){
  if(Math.Min(Math.Abs(parent.S.X),Math.Min(Math.Abs(parent.S.Y),Math.Abs(parent.S.Z)))<1e-6)throw new InvalidDataException(UiText.T("t0312"));
  var inverse=Quaternion.Inverse(parent.R);
  return new(Vector3.Transform(world.T-parent.T,inverse)/parent.S,Quaternion.Normalize(inverse*world.R),world.S/parent.S);
 }
}

// Preserve the original track mapping and untouched, unanimated skeleton bones.
// Reconstruct against the actual parent pose so sparse tracks remain valid.
public sealed class MotionEdits {
 readonly Rig rig;readonly MotionClip clip;readonly int[] mirrored,trackForBone;readonly Pose[] reference;
 readonly Quaternion[] mirrorCorrection;
 public MotionEdits(Rig r,MotionClip c){
  c.ValidateRig(r);rig=r;clip=c;reference=r.World(r.Reference);mirrored=new int[r.Names.Length];trackForBone=Enumerable.Repeat(-1,r.Names.Length).ToArray();mirrorCorrection=new Quaternion[r.Names.Length];
  for(int track=0;track<c.BoneMap.Length;track++){int bone=c.BoneMap[track];if(bone<0||bone>=r.Names.Length||trackForBone[bone]>=0)throw new InvalidDataException(UiText.T("t0313"));trackForBone[bone]=track;}
  for(int b=0;b<mirrored.Length;b++){int other=Array.IndexOf(r.Names,PoseEdits.Counterpart(r.Names[b]));mirrored[b]=other>=0?other:b;}
  for(int b=0;b<mirrored.Length;b++){if(mirrored[mirrored[b]]!=b)throw new InvalidDataException(UiText.T("t0314"));mirrorCorrection[b]=Quaternion.Normalize(Quaternion.Inverse(PoseEdits.Reflect(reference[mirrored[b]].R))*reference[b].R);}
 }
 public int PairedBones=>mirrored.Where((value,index)=>value!=index).Count();
 public Pose[] Dance(double time,bool mirror,double yaw){
  if(!double.IsFinite(yaw)||Math.Abs(yaw)>3600)throw new InvalidDataException(UiText.T("t0315"));
  if(!mirror&&yaw==0)return clip.At(time);
  if(mirror){foreach(string name in new[]{"NPC L UpperArm [LUar]","NPC L Forearm [LLar]","NPC L Hand [LHnd]","NPC L Thigh [LThg]","NPC L Calf [LClf]","NPC L Foot [Lft ]"}){int b=Array.IndexOf(rig.Names,name);if(b<0||mirrored[b]==b)throw new InvalidDataException(UiText.T("t0316")+name);}}
  var source=clip.World(rig,time);var desired=new Pose[source.Length];var rotate=PoseEdits.Yaw(yaw);
  for(int b=0;b<desired.Length;b++){
   var p=source[mirror?mirrored[b]:b];
   if(mirror)p=new(PoseEdits.Reflect(p.T),Quaternion.Normalize(PoseEdits.Reflect(p.R)*mirrorCorrection[b]),p.S);
   desired[b]=new(Vector3.Transform(p.T,rotate),Quaternion.Normalize(rotate*p.R),p.S);
  }
  return Tracks(desired);
 }
 public Pose[] Camera(double time,int bone,double yaw,string axis,bool mirror,double stageYaw=0){
  if(!mirror&&stageYaw==0)return clip.At(time);
  if(bone<0||bone>=trackForBone.Length||trackForBone[bone]<0)throw new InvalidDataException(UiText.T("t0317"));
  var desired=clip.World(rig,time);desired[bone]=PoseEdits.CameraView(desired[bone],yaw,axis,mirror);
  if(stageYaw!=0){var rotate=PoseEdits.Yaw(-stageYaw);for(int i=0;i<desired.Length;i++){var pose=desired[i];desired[i]=new(Vector3.Transform(pose.T,rotate),Quaternion.Normalize(rotate*pose.R),pose.S);}}
  return Tracks(desired);
 }
 Pose[] Tracks(Pose[] desired){
  var world=new Pose[rig.Names.Length];var result=new Pose[clip.Tracks];
  for(int b=0;b<world.Length;b++){
   int parent=rig.Parents[b],track=trackForBone[b];
   var local=track<0?rig.Reference[b]:parent<0?desired[b]:PoseEdits.Relative(world[parent],desired[b]);
   world[b]=parent<0?local:PoseEdits.Compose(world[parent],local);
   if(track>=0)result[track]=local;
  }
  return result;
 }
 public Pose[] World(Pose[] tracks){var local=rig.Reference.ToArray();for(int i=0;i<clip.Tracks;i++)local[clip.BoneMap[i]]=tracks[i];return rig.World(local);}
}
