namespace DanceStudio;

// Both tabs share the viewport and the project's existing transport clock.
public sealed class TimelineAxis {
 public const double Left=132, Right=18;
 public double Span=240,Start;
 public event Action? Changed;
 public double Width(double width)=>Math.Max(1,width-Left-Right);
 public double X(double time,double width)=>Left+(time-Start)/Span*Width(width);
 public double Time(double x,double width)=>Math.Max(0,Start+(x-Left)/Width(width)*Span);
 public void Zoom(double x,double width,int delta){double before=Time(x,width),next=Math.Clamp(Span*Math.Pow(.8,delta/120.0),2,3600);Start=Math.Max(0,before-(before-Start)*next/Span);Span=next;Changed?.Invoke();}
 public static double MajorStep(double span,double pixels){double target=span*105/Math.Max(1,pixels),power=Math.Pow(10,Math.Floor(Math.Log10(Math.Max(.001,target))));return new[]{1.0,2.0,5.0,10.0}.Select(n=>n*power).First(n=>n>=target);}
 public IEnumerable<(double time,double x,bool major)> Ticks(double width){double step=MajorStep(Span,Width(width)),minor=step/5;int index=(int)Math.Ceiling(Start/minor);for(;index*minor<Start+Span;index++){double t=index*minor;yield return(t,X(t,width),index%5==0);}}
}

