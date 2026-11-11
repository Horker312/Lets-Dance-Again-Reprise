namespace FacialStudio;
// Seconds in the authored clip. Zoom/pan never write to keys or clip duration.
public sealed class ClipViewport {
 public double Duration{get;private set;}=2;public double Start{get;private set;}public double Span{get;private set;}=2;
 public double End=>Start+Span;public double Maximum=>Math.Max(0,Duration-Span);
 public event Action? Changed;
 public void SetDuration(double duration,bool fit=false){if(!double.IsFinite(duration)||duration<=0||duration>600)throw new ArgumentOutOfRangeException(nameof(duration));bool full=Math.Abs(Span-Duration)<1e-8;Duration=duration;Span=fit||full?duration:Math.Clamp(Span,Math.Min(.05,duration),duration);Start=fit?0:Math.Clamp(Start,0,Maximum);Changed?.Invoke();}
 public void Fit(){Span=Duration;Start=0;Changed?.Invoke();}
 public void Pan(double start){if(!double.IsFinite(start))return;double next=Math.Clamp(start,0,Maximum);if(next==Start)return;Start=next;Changed?.Invoke();}
 public void Zoom(double span,double ratio){if(!double.IsFinite(span)||!double.IsFinite(ratio))throw new ArgumentOutOfRangeException(nameof(span));ratio=Math.Clamp(ratio,0,1);double anchor=Start+Span*ratio;Span=Math.Clamp(span,Math.Min(.05,Duration),Duration);Start=Math.Clamp(anchor-Span*ratio,0,Maximum);Changed?.Invoke();}
 public double At(double ratio)=>Math.Clamp(Start+Math.Clamp(ratio,0,1)*Span,0,Duration);
 public double Ratio(double time)=>(time-Start)/Span;
}
