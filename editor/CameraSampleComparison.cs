using System.IO;
using System.Numerics;
using System.Text.Json;
namespace DanceStudio;
public static class CameraSampleComparison {
 public static object Compare(string file,SongProject project,MotionClip source,Rig rig,bool enforceGameLimits=false){
  using var document=JsonDocument.Parse(File.ReadAllText(file));var rows=document.RootElement.GetProperty("samples").EnumerateArray().ToArray();var stamps=rows.Select(r=>r.GetProperty("time").GetDouble()).ToArray();
  Vector3 Vec(JsonElement a)=>new(a[0].GetSingle(),a[1].GetSingle(),a[2].GetSingle());
  var poses=rows.Select(r=>{var q=r.GetProperty("rotation");return new Pose(Vec(r.GetProperty("position")),Quaternion.Normalize(new(q[0].GetSingle(),q[1].GetSingle(),q[2].GetSingle(),q[3].GetSingle())),Vector3.One);}).ToArray();
  bool exceedsRuntimeRange=poses.Any(p=>Math.Max(Math.Abs(p.T.X),Math.Max(Math.Abs(p.T.Y),Math.Abs(p.T.Z)))>20000);if(enforceGameLimits&&exceedsRuntimeRange)throw new InvalidDataException("선택한 카메라 본의 위치가 게임 카메라 샘플 한도(축별 20,000단위)를 벗어납니다. 본과 좌표계를 확인하세요.");
  double duration=project.ExportOut-project.ExportIn;var times=new SortedSet<double>{0,duration};for(int i=0;i<stamps.Length-1;i++)for(int j=0;j<=project.Verification.Subdivisions;j++)times.Add(stamps[i]+(stamps[i+1]-stamps[i])*j/project.Verification.Subdivisions);for(int i=0;i<source.Frames;i++){double time=project.Camera.Start+(source.Duration*i/(source.Frames-1)-project.Camera.In)/project.Camera.Speed-project.ExportIn;if(time>=0&&time<=duration)times.Add(time);}
  var error=new BoneError{Index=project.CameraBone,Name=rig.Names[project.CameraBone]};
  foreach(double time in times){int index=Array.BinarySearch(stamps,time);Pose actual;if(index>=0)actual=poses[index];else{int next=~index;if(next==0)actual=poses[0];else if(next>=poses.Length)actual=poses[^1];else actual=Pose.Blend(poses[next-1],poses[next],(float)((time-stamps[next-1])/(stamps[next]-stamps[next-1])));}
   double sourceTime=Math.Clamp(project.Camera.SourceTime(project.ExportIn+time),project.Camera.In,project.Camera.Out);var expected=PoseEdits.StageCamera(source.World(rig,sourceTime)[project.CameraBone],project.CameraYaw,project.CameraForward,project.MirrorCamera,project.PlayerYaw);double distance=Vector3.Distance(expected.T,actual.T),angle=MotionComparison.Angle(expected.R,actual.R);if(distance>error.Position){error.Position=distance;error.PositionTime=time;}if(angle>error.RotationDegrees){error.RotationDegrees=angle;error.RotationTime=time;}
  }
  bool warning=error.Position>project.Verification.PositionWarning||error.RotationDegrees>project.Verification.RotationWarningDegrees;
  return new{compared=true,exceedsRuntimeRange,sourceSha256=source.SourceHash,sampleCount=times.Count,includesOutputMidpoints=true,cameraBone=error,warnings=warning?new[]{error}:Array.Empty<BoneError>(),floatWarning=false,floatTracksNotConsumedByCameraSamples=source.Animation.numberOfFloatTracks,annotationEventsNotExecutedByCameraSamples=source.Animation.annotationTracks.Sum(t=>t.annotations.Count),scaleNotDirectlyAppliedToView=true,referenceHkxPreservedSeparately=true,limit="Compares managed TRS camera-sample interpolation against the editor's decoded source at the same times. It is not independent continuous Havok or Skyrim rendering verification. Source Float and Annotation data are not executed by the sampled camera path."};
 }
}
