using System.Numerics;
using System.IO;
namespace DanceStudio;
public sealed class MotionCheckOptions {
 public double PositionWarning {get;set;}=.1;
 public double RotationWarningDegrees {get;set;}=.5;
 public double ScaleWarning {get;set;}=.001;
 public double FloatWarning {get;set;}=.001;
 public int Subdivisions {get;set;}=2;
 public void Validate(){if(new[]{PositionWarning,RotationWarningDegrees,ScaleWarning,FloatWarning}.Any(x=>!double.IsFinite(x)||x<0)||Subdivisions is <2 or >8)throw new InvalidDataException("진단 임계값은 유한한 음수 아닌 값, 프레임 분할은 2~8이어야 합니다.");}
}
public sealed class BoneError {
 public string Name {get;set;}="";public int Index {get;set;}
 public double Position {get;set;}public double RotationDegrees {get;set;}public double Scale {get;set;}
 public double PositionTime {get;set;}public double RotationTime {get;set;}public double ScaleTime {get;set;}
}
public static class MotionComparison {
 public static double Angle(Quaternion a,Quaternion b){double dot=(double)a.X*b.X+(double)a.Y*b.Y+(double)a.Z*b.Z+(double)a.W*b.W;double aa=(double)a.X*a.X+(double)a.Y*a.Y+(double)a.Z*a.Z+(double)a.W*a.W,bb=(double)b.X*b.X+(double)b.Y*b.Y+(double)b.Z*b.Z+(double)b.W*b.W;return 2*Math.Acos(Math.Clamp(Math.Abs(dot)/Math.Sqrt(aa*bb),0,1))*180/Math.PI;}
 public static object Compare(MotionClip source,MotionClip output,TrackEdit edit,double begin,double end,Func<double,Pose[]>? edited,Rig? rig,MotionCheckOptions options){
  options.Validate();if(rig!=null)source.ValidateRig(rig);if(!source.BoneMap.SequenceEqual(output.BoneMap))throw new InvalidDataException("출력 Transform 바인딩이 원본과 다릅니다.");
  if(!source.Container.bindings[source.BindingIndex].floatTrackToFloatSlotIndices.SequenceEqual(output.Container.bindings[output.BindingIndex].floatTrackToFloatSlotIndices))throw new InvalidDataException("출력 Float 바인딩이 원본과 다릅니다.");
  double duration=end-begin;var times=new SortedSet<double>{0,duration};
  int steps=(output.Frames-1)*options.Subdivisions;for(int i=0;i<=steps;i++)times.Add(duration*i/steps);
  for(int i=0;i<source.Frames;i++){double t=edit.Start+(source.Duration*i/(source.Frames-1)-edit.In)/edit.Speed-begin;if(t>=0&&t<=duration)times.Add(t);}
  var errors=source.BoneMap.Select(b=>new BoneError{Index=b,Name=rig!=null?rig.Names[b]:"bone index "+b}).ToArray();
  double floatError=0,floatTime=0;int floatTrack=-1;
  foreach(double t in times){double sourceTime=Math.Clamp(edit.SourceTime(begin+t),edit.In,edit.Out);var expected=edited==null?source.At(sourceTime):edited(sourceTime);var actual=output.At(t);
   if(rig!=null){Pose[] World(Pose[] tracks){var local=rig.Reference.ToArray();for(int i=0;i<source.Tracks;i++)local[source.BoneMap[i]]=tracks[i];return rig.World(local);}expected=World(expected);actual=World(actual);}
   for(int i=0;i<source.Tracks;i++){int b=rig==null?i:source.BoneMap[i];var a=expected[b];var z=actual[b];double p=Vector3.Distance(a.T,z.T),r=Angle(a.R,z.R),s=Vector3.Distance(a.S,z.S);var error=errors[i];if(p>error.Position){error.Position=p;error.PositionTime=t;}if(r>error.RotationDegrees){error.RotationDegrees=r;error.RotationTime=t;}if(s>error.Scale){error.Scale=s;error.ScaleTime=t;}}
   var f=source.FloatsAt(sourceTime);var g=output.FloatsAt(t);if(f.Length!=g.Length)throw new InvalidDataException("출력 Float 트랙 수가 달라졌습니다.");for(int i=0;i<f.Length;i++)if(Math.Abs(f[i]-g[i])>floatError){floatError=Math.Abs(f[i]-g[i]);floatTime=t;floatTrack=i;}
  }
  var inputAnnotations=source.Animation.annotationTracks;var outAnnotations=output.Animation.annotationTracks;
  if(inputAnnotations.Count!=outAnnotations.Count)throw new InvalidDataException("Annotation 트랙 수가 달라졌습니다.");
  int events=0;for(int i=0;i<inputAnnotations.Count;i++){var a=inputAnnotations[i];var b=outAnnotations[i];var expected=a.annotations.Where(x=>x.time>=edit.In&&x.time<=edit.Out).Select(x=>new{time=(x.time-edit.In)/edit.Speed+edit.Start-begin,x.text}).Where(x=>x.time>=0&&x.time<=duration).ToArray();if(a.trackName!=b.trackName||expected.Length!=b.annotations.Count)throw new InvalidDataException("Annotation 이름 또는 이벤트 수가 달라졌습니다.");for(int j=0;j<expected.Length;j++)if(expected[j].text!=b.annotations[j].text||Math.Abs(expected[j].time-b.annotations[j].time)>.0001)throw new InvalidDataException("Annotation 이벤트 텍스트·시간이 달라졌습니다.");events+=expected.Length;}
  return new{compared=true,sourceSha256=source.SourceHash,sourceDuration=source.Duration,sourceFrameCount=source.Frames,outputFrameCount=output.Frames,outputBegin=begin,outputEnd=end,sourceTrim=new{edit.In,edit.Out,edit.Start,edit.Speed,edit.HoldEnd},coordinateSpace=rig==null?"local transforms":"rig world transforms",sampleCount=times.Count,includesEndpoints=true,includesOutputMidpoints=true,includesSourceKeyTimes=true,thresholdsAreDiagnosticOnly=true,options,bones=errors,warnings=errors.Where(x=>x.Position>options.PositionWarning||x.RotationDegrees>options.RotationWarningDegrees||x.Scale>options.ScaleWarning).ToArray(),floatError,floatTime,floatTrack,floatWarning=floatError>options.FloatWarning,annotationEvents=events,annotationsPreserved=true,decoderLimit="Reference is Matcher integer-frame decode with editor interpolation, NOT an independently verified continuous Havok spline evaluator. Root extracted movement and in-game event execution are not evaluated."};
 }
}
