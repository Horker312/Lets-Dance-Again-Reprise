namespace FacialPresets;
public sealed class StrengthPoint {
 public Guid Id{get;set;}=Guid.NewGuid();public double Time{get;set;}public double Value{get;set;}=1;public string Interpolation{get;set;}="Linear";
}
public static class StrengthCurves {
 public static double Duration(FacialSegment c)=>c.EnvelopeDuration??c.End-c.Start;
 public static double At(FacialSegment c,double musicTime)=>Envelope(c,c.EnvelopeOffset+musicTime-c.Start);
 public static double Envelope(FacialSegment c,double t){if(c.StrengthCurve is {Count:>0} points){StrengthPoint? a=null,b=null;foreach(var point in points){if(point.Time<=t){if(a==null||point.Time>=a.Time)a=point;}else if(b==null||point.Time<b.Time)b=point;}if(a==null)return b!.Value;return b==null||a.Interpolation=="Hold"?a.Value:a.Value+(b.Value-a.Value)*(t-a.Time)/(b.Time-a.Time);}double value=1;if(c.FadeIn>0)value=Math.Min(value,t/c.FadeIn);if(c.FadeOut>0)value=Math.Min(value,(Duration(c)-t)/c.FadeOut);return Math.Clamp(value,0,1);}
 public static void ConvertFade(FacialSegment c){if(c.StrengthCurve!=null)return;double end=Duration(c);var times=new[]{0,c.FadeIn,end-c.FadeOut,end}.Distinct().Order().ToArray();c.StrengthCurve=times.Select(t=>new StrengthPoint{Time=t,Value=Envelope(c,t)}).ToList();}
 public static void Validate(FacialSegment c){if(c.StrengthCurve==null)return;if(c.StrengthCurve.Count==0||c.StrengthCurve.Count>2048)throw new InvalidDataException("강도 곡선 점 개수가 잘못되었습니다.");var ids=new HashSet<Guid>();double last=-1;foreach(var p in c.StrengthCurve.OrderBy(p=>p.Time)){if(p==null||p.Id==Guid.Empty||!ids.Add(p.Id)||!double.IsFinite(p.Time)||p.Time<0||p.Time>Duration(c)+1e-6||p.Time<=last||!double.IsFinite(p.Value)||p.Value<0||p.Value>1||p.Interpolation is not ("Linear" or "Hold"))throw new InvalidDataException("강도 곡선 시간·값·보간이 잘못되었습니다.");last=p.Time;}}
}
