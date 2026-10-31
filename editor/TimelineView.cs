using System.Windows;
using System.Windows.Input;
using System.Windows.Controls;
using System.Windows.Media;
using System.Globalization;
namespace DanceStudio;
public sealed class TimelineView:FrameworkElement {
 public SongProject Project=new();public WaveAudio? Audio;public double Playhead;public TimelineAxis Axis {get;}=new();public double Span{get=>Axis.Span;set=>Axis.Span=value;}public double ViewStart{get=>Axis.Start;set=>Axis.Start=value;}
 public double[] SourceDurations=[0,0,0];public double[] GridRates=[24,24,24];
 public Action<int>? RemoveTrack;public Action<double>? Seek;public Action? Edited;public Action<int>? Selected;
 readonly Color[] colors=[Color.FromRgb(90,199,219),Color.FromRgb(165,141,236),Color.FromRgb(214,173,91)];
 bool scrubbing;int selectedTrack=-1;int drag=-1,edge;double initial,oldIn,oldOut,oldStart;
 const double Left=132,Top=34,Row=58;
 public TimelineView(){Axis.Changed+=InvalidateVisual;ClipToBounds=true;MinHeight=225;Focusable=true;MouseDown+=Down;MouseMove+=Move;LostMouseCapture+=(_,_)=>CancelDrag();Unloaded+=(_,_)=>CancelDrag();MouseUp+=(_,_)=>{drag=-1;scrubbing=false;ReleaseMouseCapture();};MouseWheel+=Wheel;}
 double WidthTime=>Math.Max(1,ActualWidth-Left-18);
 double X(double t)=>Left+(t-ViewStart)/Span*WidthTime;
 double Time(double x)=>Math.Max(0,ViewStart+(x-Left)/WidthTime*Span);
 void Text(DrawingContext d,string text,double x,double y,Brush? color=null,double size=12,double maxWidth=double.PositiveInfinity){var label=new FormattedText(text,CultureInfo.CurrentCulture,FlowDirection.LeftToRight,new Typeface("Malgun Gothic"),size,color??StudioTheme.Muted,VisualTreeHelper.GetDpi(this).PixelsPerDip);if(double.IsFinite(maxWidth)){label.MaxTextWidth=Math.Max(1,maxWidth);label.Trimming=TextTrimming.CharacterEllipsis;}d.DrawText(label,new Point(x,y));}
 protected override void OnRender(DrawingContext d){
  base.OnRender(d);d.DrawRoundedRectangle(StudioTheme.Panel,new Pen(StudioTheme.Line,1),new Rect(0,0,ActualWidth,ActualHeight),6,6);for(int row=0;row<3;row++){var tint=new SolidColorBrush(colors[row]){Opacity=selectedTrack==row?.10:.035};d.DrawRectangle(tint,null,new Rect(0,Top+row*Row,ActualWidth,Row-1));}
  foreach(var (t,x,major) in Axis.Ticks(ActualWidth)){d.DrawLine(new Pen(StudioTheme.Line,1),new(x,major?27:30),new(x,Top+Row*3));if(major&&x+100<ActualWidth)Text(d,Format(t),x+4,7);}
  var tracks=new[]{Project.Dance,Project.Camera,Project.Audio};string[] names=[UiText.T("t0333"),UiText.T("t0334"),UiText.T("t0335")];
  d.PushClip(new RectangleGeometry(new Rect(Left,Top,WidthTime,Row*3)));
  for(int i=0;i<3;i++){
   double y=Top+i*Row+7;var t=tracks[i];if(t.Path.Length==0)continue;double x=X(t.Start),end=X(t.TimelineEnd);
   var brush=new SolidColorBrush(colors[i]){Opacity=selectedTrack==i?.26:.12};d.DrawRoundedRectangle(brush,new Pen(selectedTrack==i?StudioTheme.Accent:new SolidColorBrush(colors[i]),selectedTrack==i?2:1),new Rect(x,y,Math.Max(1,end-x),42),4,4);
   if(i==2&&Audio!=null){for(double px=Math.Max(x,Left);px<Math.Min(end,ActualWidth-18);px+=2){double source=t.In+(ViewStart+(px-Left)/WidthTime*Span)-t.Start;int n=(int)(source/Audio.Duration*Audio.Peaks.Length);if(n>=0&&n<Audio.Peaks.Length){double h=Audio.Peaks[n]*17;d.DrawLine(new Pen(new SolidColorBrush(colors[i]),1),new(px,y+21-h),new(px,y+21+h));}}}
   else Text(d,System.IO.Path.GetFileName(t.Path)+(i<2?$" · {t.Speed:0.####}×":""),Math.Max(x+12,Left+8),y+12,StudioTheme.Text,12,Math.Max(1,Math.Min(end,ActualWidth-18)-Math.Max(x+12,Left+8)-8));
   d.DrawRectangle(new SolidColorBrush(colors[i]),null,new Rect(x,y,5,42));d.DrawRectangle(new SolidColorBrush(colors[i]),null,new Rect(end-5,y,5,42));
  }
  d.Pop();
  for(int i=0;i<3;i++)Text(d,names[i],14,Top+i*Row+17,new SolidColorBrush(colors[i]));
  double begin=X(Project.ExportIn),finish=X(Project.ExportOut);
  if(finish>Left&&begin<ActualWidth){d.DrawLine(new Pen(Brushes.MediumAquamarine,3),new(Math.Max(Left,begin),Top+Row*3+4),new(Math.Min(ActualWidth-18,finish),Top+Row*3+4));}
  if(X(Playhead)>=Left&&X(Playhead)<=ActualWidth-18){d.DrawLine(new Pen(StudioTheme.Accent,2),new(X(Playhead),25),new(X(Playhead),Top+Row*3+10));var marker=new StreamGeometry();using(var c=marker.Open()){c.BeginFigure(new(X(Playhead)-5,20),true,true);c.LineTo(new(X(Playhead)+5,20),true,false);c.LineTo(new(X(Playhead),27),true,false);}d.DrawGeometry(StudioTheme.Accent,null,marker);}
  Text(d,UiText.T("t0336"),Left,Top+Row*3+16,null,11);
 }
 internal ContextMenu? TrackMenu(Point p){
  if(p.X<Left||p.X>ActualWidth-18||p.Y<Top)return null;
  int i=(int)((p.Y-Top)/Row);if(i<0||i>2)return null;
  var track=new[]{Project.Dance,Project.Camera,Project.Audio}[i];double y=Top+i*Row+7;
  if(track.Path.Length==0||p.Y<y||p.Y>y+42||p.X<X(track.Start)||p.X>X(track.TimelineEnd))return null;
  var menu=new ContextMenu();var item=new MenuItem{Header=UiText.T("t0337"),Foreground=StudioTheme.Danger};item.Click+=(_,_)=>{CancelDrag();RemoveTrack?.Invoke(i);InvalidateVisual();};menu.Items.Add(item);return menu;
 }
 void Down(object sender,MouseButtonEventArgs e){if(e.ChangedButton==MouseButton.Right){CancelDrag();var menu=TrackMenu(e.GetPosition(this));if(menu!=null){menu.PlacementTarget=this;menu.IsOpen=true;}e.Handled=true;return;}if(e.ChangedButton!=MouseButton.Left)return;PointerDown(e.GetPosition(this));e.Handled=true;}
 internal void PointerDown(Point p){Focus();if(p.X<Left||p.X>ActualWidth-18)return;if(p.Y<Top){scrubbing=true;CaptureMouse();Seek?.Invoke(Math.Clamp(Time(p.X),0,ProjectRules.MaxProjectTime));return;}int i=(int)((p.Y-Top)/Row);if(i>=0&&i<3){selectedTrack=i;InvalidateVisual();Selected?.Invoke(i);var track=new[]{Project.Dance,Project.Camera,Project.Audio}[i];double a=X(track.Start),b=X(track.TimelineEnd);if(track.Path.Length>0&&p.X>=a-8&&p.X<=b+8){edge=Math.Abs(p.X-a)<7?-1:Math.Abs(p.X-b)<7?1:0;drag=i;initial=p.X;oldIn=track.In;oldOut=track.Out;oldStart=track.Start;CaptureMouse();return;}}if(Math.Abs(p.X-X(Playhead))<6){scrubbing=true;CaptureMouse();}Seek?.Invoke(Time(p.X));}
 public void CancelDrag(){drag=-1;scrubbing=false;if(IsMouseCaptured)ReleaseMouseCapture();}
 void Move(object sender,MouseEventArgs e){var p=e.GetPosition(this);if(e.LeftButton!=MouseButtonState.Pressed){int i=(int)((p.Y-Top)/Row);ToolTip=null;Cursor=Cursors.Arrow;if(p.Y>=Top&&i>=0&&i<3){var t=new[]{Project.Dance,Project.Camera,Project.Audio}[i];if(t.Path.Length>0&&p.X>=X(t.Start)&&p.X<=X(t.TimelineEnd)){ToolTip=t.Path+$"\n{t.Start:0.###} → {t.TimelineEnd:0.###} s";Cursor=Math.Abs(p.X-X(t.Start))<7||Math.Abs(p.X-X(t.TimelineEnd))<7?Cursors.SizeWE:Cursors.SizeAll;}}}PointerMove(p,e.LeftButton==MouseButtonState.Pressed);}
 internal void PointerMove(Point p,bool pressed){if(!pressed){CancelDrag();return;}if(scrubbing){Seek?.Invoke(Math.Clamp(Time(Math.Clamp(p.X,Left,ActualWidth-18)),0,ProjectRules.MaxProjectTime));return;}if(drag<0)return;double delta=(Math.Clamp(p.X,Left,ActualWidth-18)-initial)/WidthTime*Span;double rate=GridRates[drag];delta=Math.Round(delta*rate)/rate;var track=new[]{Project.Dance,Project.Camera,Project.Audio}[drag];if(edge<0)track.TrimStart(oldIn,oldOut,oldStart,delta,rate);else if(edge>0)track.TrimEnd(oldIn,oldOut,delta,SourceDurations[drag]);else track.Start=Math.Clamp(oldStart+delta,0,Math.Max(0,ProjectRules.MaxProjectTime-track.TimelineDuration));Edited?.Invoke();InvalidateVisual();}
 void Wheel(object sender,MouseWheelEventArgs e){e.Handled=true;if(Mouse.Captured!=null)return;Axis.Zoom(e.GetPosition(this).X,ActualWidth,e.Delta);e.Handled=true;}
 public static string Format(double t)=>$"{(int)t/60:00}:{t%60:00.00}";
}
