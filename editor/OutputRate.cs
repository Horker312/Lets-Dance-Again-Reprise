using System.IO;
using HKX2E;
namespace DanceStudio;
public sealed class OutputRate {
 public string Mode {get;set;}="Auto";
 public double Value {get;set;}=24;
 public void Validate(){if(Mode is not ("Auto" or "Manual")||!double.IsFinite(Value)||Value<1||Value>120)throw new InvalidDataException("출력 FPS는 Auto/Manual, 수동 값은 1~120이어야 합니다.");}
 public static double? Source(MotionClip? clip){
  if(clip==null||clip.Frames<2||clip.Duration<=0)return null;
  double rate=(clip.Frames-1)/clip.Duration;
  if(clip.Animation is hkaSplineCompressedAnimation a){
   object? raw=a.GetType().GetProperty("frameDuration")?.GetValue(a);
   if(raw==null)return null;double step=Convert.ToDouble(raw);
   if(!double.IsFinite(step)||step<=0||Math.Abs(step*(clip.Frames-1)-clip.Duration)>Math.Max(.0001,clip.Duration*.00001))return null;
   rate=1/step;
  }else if(clip.Animation is not hkaInterleavedUncompressedAnimation)return null;
  if(!double.IsFinite(rate)||rate<1||rate>120.001)return null;
  return Math.Abs(rate-Math.Round(rate))<.001?Math.Round(rate):rate;
 }
 public double Resolve(MotionClip? clip,TrackEdit edit){Validate();return Mode=="Manual"?Value:Math.Clamp((Source(clip)??24)*edit.Speed,1,120);}
 public static bool Unedited(MotionClip clip,TrackEdit edit,double begin,double end,bool poseEdited)=>edit.In==0&&Math.Abs(edit.Out-clip.Duration)<1e-6&&edit.Start==0&&edit.Speed==1&&begin==0&&Math.Abs(end-clip.Duration)<1e-6&&!poseEdited;
 public string Describe(MotionClip? clip,TrackEdit edit,bool copy){var source=Source(clip);return $"원본 {(source.HasValue?source.Value.ToString("0.###"):"확인 불가")} / 출력 {Resolve(clip,edit):0.###} FPS · {(copy?"원본 복사":"재샘플링")}"+(Mode=="Auto"&&(source??24)*edit.Speed>120?" (120 제한)":"");}
}
