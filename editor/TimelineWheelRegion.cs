namespace DanceStudio;

// Pure coordinate policy, kept separate from WPF event routing so it can be tested.
public static class TimelineWheelRegion {
 public static bool Contains(double x,double width,double left,double right)=>
  double.IsFinite(x)&&double.IsFinite(width)&&double.IsFinite(left)&&double.IsFinite(right)&&
  left>=0&&right>=0&&width>left+right&&x>=left&&x<=width-right;
}
