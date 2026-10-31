using System.Windows;
using FaceAssets;
using System.Numerics;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using V=System.Numerics.Vector3;
using Q=System.Numerics.Quaternion;
namespace DanceStudio;

public sealed class PreviewView:Border {
 readonly TextBlock cameraWarning=new(){Text=UiText.T("t0318"),Foreground=Brushes.LightGray,HorizontalAlignment=HorizontalAlignment.Center,VerticalAlignment=VerticalAlignment.Center,Visibility=Visibility.Collapsed};
 readonly Viewport3D viewport=new();readonly Grid frame=new(){Background=Brushes.Black};double aspect=16.0/9;readonly Model3DGroup group=new();readonly Model3DGroup body=new();readonly Model3DGroup floor=new();
 readonly PerspectiveCamera viewCamera=new(){FieldOfView=48,NearPlaneDistance=.1,FarPlaneDistance=20000};
 readonly List<(int A,int B,MeshGeometry3D Mesh,double Radius)> bones=[];
 FaceScene? face;readonly Model3DGroup faceLayer=new();bool faceZoom,faceMirror;
 MeshGeometry3D? head;Rig? rig;Pose[]? world;bool orbit;Point? last;double yaw=28,pitch=12,distance=370;
 public PreviewView(bool orbit) {
  this.orbit=orbit;Background=new SolidColorBrush(Color.FromRgb(17,23,34));CornerRadius=new CornerRadius(8);Child=frame;frame.Children.Add(viewport);frame.Children.Add(cameraWarning);ClipToBounds=true;SizeChanged+=(_,_)=>FitAspect();
  viewport.Camera=viewCamera;group.Children.Add(new AmbientLight(Color.FromRgb(160,170,185)));group.Children.Add(new DirectionalLight(Colors.White,new Vector3D(-1,-2,-1)));

  for(int i=-5;i<=5;i++){floor.Children.Add(Line(new(i*50,-250,0),new(i*50,250,0),.35,Color.FromRgb(45,57,73)));floor.Children.Add(Line(new(-250,i*50,0),new(250,i*50,0),.35,Color.FromRgb(45,57,73)));}
  floor.Children.Add(Line(new(0,0,.5f),new(120,0,.5f),1.1,Color.FromRgb(220,90,90)));floor.Children.Add(Line(new(0,0,.5f),new(0,120,.5f),1.1,Color.FromRgb(90,200,120)));
  group.Children.Add(floor);group.Children.Add(body);group.Children.Add(faceLayer);viewport.Children.Add(new ModelVisual3D{Content=group});
  MouseLeftButtonDown+=(_,e)=>{if(orbit){last=e.GetPosition(this);CaptureMouse();}};
  MouseLeftButtonUp+=(_,_)=>{last=null;ReleaseMouseCapture();};
  MouseMove+=(_,e)=>{if(last is Point p){var now=e.GetPosition(this);yaw+=(now.X-p.X)*.4;pitch=Math.Clamp(pitch-(now.Y-p.Y)*.3,-65,75);last=now;Orbit();}};
  MouseWheel+=(_,e)=>{if(orbit){distance=Math.Clamp(distance*Math.Pow(.9,e.Delta/120.0),70,2000);Orbit();}};Orbit();
 }
 public void SetCameraValid(bool valid){if(orbit)return;viewport.Visibility=valid?Visibility.Visible:Visibility.Hidden;cameraWarning.Visibility=valid?Visibility.Collapsed:Visibility.Visible;}
 public void SetStageYaw(double degrees){floor.Transform=new RotateTransform3D(new AxisAngleRotation3D(new Vector3D(0,1,0),degrees));}
 public void SetAspect(double ratio){aspect=ratio;FitAspect();}
 void FitAspect(){if(orbit){viewport.Width=double.NaN;viewport.Height=double.NaN;return;}double w=ActualWidth,h=ActualHeight;if(w<=0||h<=0)return;viewport.Width=Math.Min(w,h*aspect);viewport.Height=viewport.Width/aspect;}
 static Point3D P(V v)=>new(v.X,v.Z,-v.Y);
 static Vector3D D(V v)=>new(v.X,v.Z,-v.Y);
 static Material Mat(Color c)=>new DiffuseMaterial(new SolidColorBrush(c));
 static MeshGeometry3D Tube(V a,V b,double radius) {
  var mesh=new MeshGeometry3D();SetTube(mesh,a,b,radius);return mesh;
 }
 static GeometryModel3D Line(V a,V b,double radius,Color color) {var m=Mat(color);return new(Tube(a,b,radius),m){BackMaterial=m};}
 static void SetTube(MeshGeometry3D mesh,V a,V b,double radius) {
  var dir=b-a;if(dir.LengthSquared()<1e-6f)dir=V.UnitZ;dir=V.Normalize(dir);
  var u=V.Normalize(V.Cross(dir,Math.Abs(dir.Z)>.95?V.UnitX:V.UnitZ));var v=V.Cross(dir,u);var points=new Point3DCollection();
  for(int end=0;end<2;end++)for(int i=0;i<8;i++){double angle=i*Math.PI/4;points.Add(P((end==0?a:b)+(float)radius*((float)Math.Cos(angle)*u+(float)Math.Sin(angle)*v)));}
  mesh.Positions=points;
  if(mesh.TriangleIndices.Count==0){var ids=new Int32Collection();for(int i=0;i<8;i++){int j=(i+1)%8;foreach(var n in new[]{i,j,8+i,j,8+j,8+i})ids.Add(n);}mesh.TriangleIndices=ids;}
 }
 public void SetRig(Rig r) {
  rig=r;body.Children.Clear();bones.Clear();
  string[] names=["NPC Pelvis [Pelv]","NPC Spine [Spn0]","NPC Spine1 [Spn1]","NPC Spine2 [Spn2]","NPC Neck [Neck]","NPC Head [Head]",
   "NPC L Clavicle [LClv]","NPC R Clavicle [RClv]","NPC L UpperArm [LUar]","NPC R UpperArm [RUar]","NPC L Forearm [LLar]","NPC R Forearm [RLar]","NPC L Hand [LHnd]","NPC R Hand [RHnd]",
   "NPC L Thigh [LThg]","NPC R Thigh [RThg]","NPC L Calf [LClf]","NPC R Calf [RClf]","NPC L Foot [Lft ]","NPC R Foot [Rft ]","NPC L Toe0 [LToe]","NPC R Toe0 [RToe]"];
  foreach(var name in names){int b=Array.IndexOf(r.Names,name);if(b<0||r.Parents[b]<0)continue;int a=r.Parents[b];double radius=name.Contains("Thigh")?5:name.Contains("Calf")?3.5:name.Contains("Spine")?8:name.Contains("UpperArm")?3:name.Contains("Forearm")?2.3:2;var mesh=new MeshGeometry3D();var material=Mat(name.Contains(" L ")?Color.FromRgb(101,200,221):name.Contains(" R ")?Color.FromRgb(164,149,238):Color.FromRgb(175,188,211));body.Children.Add(new GeometryModel3D(mesh,material){BackMaterial=material});bones.Add((a,b,mesh,radius));}
  head=new MeshGeometry3D();var hm=Mat(Color.FromRgb(211,200,191));body.Children.Add(new GeometryModel3D(head,hm){BackMaterial=hm});Update(r.World(r.Reference));
 }
 public void Clear(){body.Children.Clear();bones.Clear();rig=null;world=null;head=null;faceLayer.Children.Clear();}
 public void Update(Pose[] w) {
  world=w;foreach(var b in bones)SetTube(b.Mesh,w[b.A].T,w[b.B].T,b.Radius);
  if(rig!=null&&head!=null){int h=Array.IndexOf(rig.Names,"NPC Head [Head]");if(h>=0&&face==null)SetTube(head,w[h].T,w[h].T+V.Transform(new V(0,0,13),w[h].R),6);}
  UpdateFacePose();if(orbit)Orbit();
 }
 public void SetFace(FaceScene? scene){face=scene?.HasHeadBind==true?scene:null;faceLayer.Children.Clear();if(face!=null)faceLayer.Children.Add(face.CreateVisual());if(world!=null)Update(world);}
 public void SetFaceOptions(bool mirror,bool zoom){faceMirror=mirror;faceZoom=zoom&&face!=null;if(orbit){if(faceZoom)group.Children.Remove(body);else if(!group.Children.Contains(body))group.Children.Insert(Math.Min(3,group.Children.Count),body);}UpdateFacePose();if(orbit)Orbit();}
 void UpdateFacePose(){if(face==null||rig==null||world==null)return;int h=Array.IndexOf(rig.Names,"NPC Head [Head]");if(h<0)return;if(faceLayer.Children.Count==0)faceLayer.Children.Add(face.CreateVisual());if(head!=null)head.Positions=new Point3DCollection();var pose=world[h];var m=Matrix4x4.CreateScale(pose.S*(faceMirror?new V(-1,1,1):V.One))*Matrix4x4.CreateFromQuaternion(pose.R)*Matrix4x4.CreateTranslation(pose.T);var c=new Matrix4x4(1,0,0,0,0,0,-1,0,0,1,0,0,0,0,0,1);Matrix4x4.Invert(c,out var inverse);m=inverse*m*c;faceLayer.Transform=new MatrixTransform3D(new Matrix3D(m.M11,m.M12,m.M13,m.M14,m.M21,m.M22,m.M23,m.M24,m.M31,m.M32,m.M33,m.M34,m.M41,m.M42,m.M43,m.M44));}
 void Orbit() {
  if(!orbit)return;var target=new V(0,0,85);if(rig!=null&&world!=null){int com=Array.IndexOf(rig.Names,"NPC Pelvis [Pelv]");if(com>=0)target=new(world[com].T.X,world[com].T.Y,85);}
  double viewingDistance=distance;if(faceZoom&&face!=null&&rig!=null&&world!=null){int h=Array.IndexOf(rig.Names,"NPC Head [Head]");if(h>=0){var bounds=faceLayer.Bounds;target=FaceScene.Vector(new Point3D(bounds.X+bounds.SizeX/2,bounds.Y+bounds.SizeY/2,bounds.Z+bounds.SizeZ/2));viewingDistance=Math.Clamp(distance/6,8,150);}}viewCamera.FieldOfView=faceZoom?36:48;
  double y=yaw*Math.PI/180,p=pitch*Math.PI/180;var pos=target+new V((float)(viewingDistance*Math.Sin(y)*Math.Cos(p)),(float)(viewingDistance*Math.Cos(y)*Math.Cos(p)),(float)(viewingDistance*Math.Sin(p)));
  viewCamera.Position=P(pos);viewCamera.LookDirection=D(target-pos);viewCamera.UpDirection=new(0,1,0);
 }
 public void Camera(Pose pose,double fov,double yawDegrees,string axis,bool mirror=false) {
  if(orbit)return;var effective=PoseEdits.CameraView(pose,yawDegrees,axis,mirror);
  viewCamera.Position=P(effective.T);viewCamera.LookDirection=D(V.Transform(V.UnitY,effective.R));viewCamera.UpDirection=D(V.Transform(V.UnitZ,effective.R));viewCamera.FieldOfView=Math.Clamp(fov,5,140);
 }
}
