using System.Windows;
using System.Windows.Input;
using System.Windows.Controls;
using System.Windows.Media;
using System.Globalization;
namespace DanceStudio;
public sealed class TimelineView:FrameworkElement {
 public SongProject Project=new();public WaveAudio? Audio;public double Playhead,Span=240,ViewStart;
 public double[] SourceDurations=[0,0,0];public double[] GridRates=[24,24,24];
 public Action<int>? RemoveTrack;public Action<double>? Seek;public Action? Edited;public Action<int>? Selected;
 readonly Color[] colors=[Color.FromRgb(90,199,219),Color.FromRgb(165,141,236),Color.FromRgb(214,173,91)];
 bool scrubbing;int drag=-1,edge;double initial,oldIn,oldOut,oldStart;
 const double Left=132,Top=34,Row=58;
 public TimelineView(){MinHeight=225;Focusable=true;MouseDown+=Down;MouseMove+=Move;LostMouseCapture+=(_,_)=>CancelDrag();Unloaded+=(_,_)=>CancelDrag();MouseUp+=(_,_)=>{drag=-1;scrubbing=false;ReleaseMouseCapture();};MouseWheel+=Wheel;}
 double WidthTime=>Math.Max(1,ActualWidth-Left-18);
 double X(double t)=>Left+(t-ViewStart)/Span*WidthTime;
 double Time(double x)=>Math.Max(0,ViewStart+(x-Left)/WidthTime*Span);
 void Text(DrawingContext d,string text,double x,double y,Brush? color=null,double size=12){d.DrawText(new FormattedText(text,CultureInfo.CurrentCulture,FlowDirection.LeftToRight,new Typeface("Malgun Gothic"),size,color??Brushes.LightSlateGray,VisualTreeHelper.GetDpi(this).PixelsPerDip),new Point(x,y));}
 protected override void OnRender(DrawingContext d){
  base.OnRender(d);d.DrawRectangle(new SolidColorBrush(Color.FromRgb(18,24,35)),null,new Rect(0,0,ActualWidth,ActualHeight));
  double step=Span>500?60:Span>180?30:Span>60?10:Span>20?5:1;
  for(double t=Math.Ceiling(ViewStart/step)*step;t<ViewStart+Span;t+=step){double x=X(t);d.DrawLine(new Pen(new SolidColorBrush(Color.FromRgb(41,51,66)),1),new(x,27),new(x,Top+Row*3));Text(d,Format(t),x+4,7);}
  var tracks=new[]{Project.Dance,Project.Camera,Project.Audio};string[] names=["춤 · HKX","카메라 · HKX","음악 · 오디오"];
  d.PushClip(new RectangleGeometry(new Rect(Left,Top,WidthTime,Row*3)));
  for(int i=0;i<3;i++){
   double y=Top+i*Row+7;var t=tracks[i];if(t.Path.Length==0)continue;double x=X(t.Start),end=X(t.TimelineEnd);
   var brush=new SolidColorBrush(colors[i]){Opacity=.22};d.DrawRoundedRectangle(brush,new Pen(new SolidColorBrush(colors[i]),1),new Rect(x,y,Math.Max(1,end-x),42),4,4);
   if(i==2&&Audio!=null){for(double px=Math.Max(x,Left);px<Math.Min(end,ActualWidth-18);px+=2){double source=t.In+(ViewStart+(px-Left)/WidthTime*Span)-t.Start;int n=(int)(source/Audio.Duration*Audio.Peaks.Length);if(n>=0&&n<Audio.Peaks.Length){double h=Audio.Peaks[n]*17;d.DrawLine(new Pen(new SolidColorBrush(colors[i]),1),new(px,y+21-h),new(px,y+21+h));}}}
   else Text(d,System.IO.Path.GetFileName(t.Path)+(i<2?$" · {t.Speed:0.####}×":""),Math.Max(x+12,Left+8),y+12,new SolidColorBrush(colors[i]));
   d.DrawRectangle(new SolidColorBrush(colors[i]),null,new Rect(x,y,4,42));d.DrawRectangle(new SolidColorBrush(colors[i]),null,new Rect(end-4,y,4,42));
  }
  d.Pop();
  for(int i=0;i<3;i++)Text(d,names[i],14,Top+i*Row+17,new SolidColorBrush(colors[i]));
  double begin=X(Project.ExportIn),finish=X(Project.ExportOut);
  if(finish>Left&&begin<ActualWidth){d.DrawLine(new Pen(Brushes.MediumAquamarine,3),new(Math.Max(Left,begin),Top+Row*3+4),new(Math.Min(ActualWidth-18,finish),Top+Row*3+4));}
  if(X(Playhead)>=Left&&X(Playhead)<=ActualWidth-18)d.DrawLine(new Pen(Brushes.White,1.5),new(X(Playhead),25),new(X(Playhead),Top+Row*3+10));
  Text(d,"끝 손잡이: 자르기   ·   트랙 드래그: 이동 · 세로선/눈금: 탐색   ·   휠: 확대",Left,Top+Row*3+16,null,11);
 }
 internal ContextMenu? TrackMenu(Point p){
  if(p.X<Left||p.X>ActualWidth-18||p.Y<Top)return null;
  int i=(int)((p.Y-Top)/Row);if(i<0||i>2)return null;
  var track=new[]{Project.Dance,Project.Camera,Project.Audio}[i];double y=Top+i*Row+7;
  if(track.Path.Length==0||p.Y<y||p.Y>y+42||p.X<X(track.Start)||p.X>X(track.TimelineEnd))return null;
  var menu=new ContextMenu();var item=new MenuItem{Header="트랙 삭제"};item.Click+=(_,_)=>{CancelDrag();RemoveTrack?.Invoke(i);InvalidateVisual();};menu.Items.Add(item);return menu;
 }
 void Down(object sender,MouseButtonEventArgs e){if(e.ChangedButton==MouseButton.Right){CancelDrag();var menu=TrackMenu(e.GetPosition(this));if(menu!=null){menu.PlacementTarget=this;menu.IsOpen=true;}e.Handled=true;return;}if(e.ChangedButton!=MouseButton.Left)return;PointerDown(e.GetPosition(this));e.Handled=true;}
 internal void PointerDown(Point p){Focus();if(p.X<Left||p.X>ActualWidth-18)return;if(p.Y<Top||Math.Abs(p.X-X(Playhead))<6){scrubbing=true;CaptureMouse();Seek?.Invoke(Math.Clamp(Time(p.X),0,ProjectRules.MaxProjectTime));return;}int i=(int)((p.Y-Top)/Row);if(i>=0&&i<3){Selected?.Invoke(i);var track=new[]{Project.Dance,Project.Camera,Project.Audio}[i];double a=X(track.Start),b=X(track.TimelineEnd);if(track.Path.Length>0&&p.X>=a-8&&p.X<=b+8){edge=Math.Abs(p.X-a)<7?-1:Math.Abs(p.X-b)<7?1:0;drag=i;initial=p.X;oldIn=track.In;oldOut=track.Out;oldStart=track.Start;CaptureMouse();return;}}Seek?.Invoke(Time(p.X));}
 public void CancelDrag(){drag=-1;scrubbing=false;if(IsMouseCaptured)ReleaseMouseCapture();}
 void Move(object sender,MouseEventArgs e){PointerMove(e.GetPosition(this),e.LeftButton==MouseButtonState.Pressed);}
 internal void PointerMove(Point p,bool pressed){if(!pressed){CancelDrag();return;}if(scrubbing){Seek?.Invoke(Math.Clamp(Time(Math.Clamp(p.X,Left,ActualWidth-18)),0,ProjectRules.MaxProjectTime));return;}if(drag<0)return;double delta=(Math.Clamp(p.X,Left,ActualWidth-18)-initial)/WidthTime*Span;double rate=GridRates[drag];delta=Math.Round(delta*rate)/rate;var track=new[]{Project.Dance,Project.Camera,Project.Audio}[drag];if(edge<0)track.TrimStart(oldIn,oldOut,oldStart,delta,rate);else if(edge>0)track.TrimEnd(oldIn,oldOut,delta,SourceDurations[drag]);else track.Start=Math.Clamp(oldStart+delta,0,Math.Max(0,ProjectRules.MaxProjectTime-track.TimelineDuration));Edited?.Invoke();InvalidateVisual();}
 void Wheel(object sender,MouseWheelEventArgs e){double before=Time(e.GetPosition(this).X);double factor=Math.Pow(.8,e.Delta/120.0);double next=Math.Clamp(Span*factor,2,3600);ViewStart=Math.Max(0,before-(before-ViewStart)*next/Span);Span=next;InvalidateVisual();}
 public static string Format(double t)=>$"{(int)t/60:00}:{t%60:00.00}";
}
