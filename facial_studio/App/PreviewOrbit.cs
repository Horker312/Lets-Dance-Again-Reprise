using System.Windows;
using System.Windows.Media.Media3D;
namespace FacialStudio;
// Orbiting the camera around a fixed model is inverse to dragging its visible front.
public static class PreviewOrbit {
 public static (double Yaw,double Pitch) Drag(double yaw,double pitch,Vector delta)=>(yaw-delta.X*.01,Math.Clamp(pitch+delta.Y*.01,-1.4,1.4));
 public static Vector3D Offset(double yaw,double pitch,double radius)=>new Vector3D(Math.Sin(yaw)*Math.Cos(pitch),Math.Sin(pitch),Math.Cos(yaw)*Math.Cos(pitch))*radius;
}
