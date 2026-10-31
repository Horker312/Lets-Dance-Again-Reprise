using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using Microsoft.Win32;
using FacialPresets;
using FaceAssets;
namespace FacialStudio;
public sealed class FacePreview:DockPanel {
 public FaceAssetPanel Assets{get;}
 FaceScene? textured;Preset? current;
 readonly Viewport3D viewport=new();readonly PerspectiveCamera camera=new(){FieldOfView=40};readonly TextBlock status=new(){Text=L.T("실제 얼굴 TRI를 불러오세요. 무텍스처 형상 미리보기입니다."),TextWrapping=TextWrapping.Wrap};
 readonly Model3DGroup models=new();readonly List<TriFace> parts=[];readonly List<GeometryModel3D> shapes=[];double yaw=Math.PI,pitch,distance=50;Point last;bool dragging;Point3D center;
 public FacePreview(){Assets=new FaceAssetPanel(SetScene,L.T);SetDock(Assets,Dock.Top);Children.Add(Assets);var buttons=new WrapPanel();SetDock(buttons,Dock.Top);Children.Add(buttons);void Button(string name,Action action){var b=new Button{Content=name,Padding=new Thickness(5),Margin=new Thickness(2)};b.Click+=(_,_)=>{try{action();}catch(Exception e){status.Text=L.T("로딩 실패: ")+e.Message;}};buttons.Children.Add(b);}
  Button(L.T("얼굴 TRI 열기"),()=>Load(false));Button(L.T("눈·입 부위 TRI 추가"),()=>Load(true));Button(L.T("확장 모프 TRI 추가"),Overlay);Button(L.T("정면 보기"),()=>{yaw=Math.PI;pitch=0;Camera();});Button(L.T("얼굴 중심 재설정"),FitScene);SetDock(status,Dock.Bottom);Children.Add(status);Children.Add(viewport);viewport.Camera=camera;
  models.Children.Add(new AmbientLight(Color.FromRgb(110,110,110)));models.Children.Add(new DirectionalLight(Colors.White,new Vector3D(-1,-1,2)));viewport.Children.Add(new ModelVisual3D{Content=models});
  viewport.MouseLeftButtonDown+=(_,e)=>{dragging=true;last=e.GetPosition(viewport);viewport.CaptureMouse();};viewport.MouseLeftButtonUp+=(_,_)=>{dragging=false;viewport.ReleaseMouseCapture();};viewport.LostMouseCapture+=(_,_)=>dragging=false;
  viewport.MouseMove+=(_,e)=>{if(!dragging)return;var p=e.GetPosition(viewport);yaw+=(p.X-last.X)*.01;pitch=Math.Clamp(pitch+(p.Y-last.Y)*.01,-1.4,1.4);last=p;Camera();};viewport.MouseWheel+=(_,e)=>{distance=Math.Clamp(distance*Math.Exp(-e.Delta*.001),.5,10000);Camera();e.Handled=true;};
 }
 string? Choose(){var d=new OpenFileDialog{Filter=L.T("얼굴 메시·모프 (FRTRI003)|*.tri")};return d.ShowDialog()==true?d.FileName:null;}
 void Load(bool add){var path=Choose();if(path!=null)LoadFile(path,add);}
 public void LoadFile(string path,bool add=false){var face=TriFace.Read(path);if(face.Indices.Length==0)throw new InvalidOperationException(L.T("기본 얼굴에는 삼각형이 필요합니다."));if(add&&parts.Sum(p=>(long)p.Vertices.Length*p.Morphs.Count)+(long)face.Vertices.Length*face.Morphs.Count>4000000)throw new InvalidOperationException(L.T("미리보기 리소스 합계가 너무 큽니다."));if(add&&parts.Count>=16)throw new InvalidOperationException(L.T("최대 16개 부위를 지원합니다."));Assets.SetPreviewEnabled(false);if(!add){parts.Clear();shapes.Clear();while(models.Children.Count>2)models.Children.RemoveAt(2);}parts.Add(face);
  var material=new DiffuseMaterial(new SolidColorBrush(Color.FromRgb(206,176,153)));var shape=new GeometryModel3D{Material=material,BackMaterial=material};shapes.Add(shape);models.Children.Add(shape);var xs=parts[0].Vertices.Select(p=>p.X);var ys=parts[0].Vertices.Select(p=>p.Y);var zs=parts[0].Vertices.Select(p=>p.Z);center=new((xs.Min()+xs.Max())/2,(zs.Min()+zs.Max())/2,-(ys.Min()+ys.Max())/2);distance=Math.Max(1,Math.Max(xs.Max()-xs.Min(),zs.Max()-zs.Min())*2.5);Camera();Update(null,null);LoadedResources?.Invoke();
 }
 void Overlay(){if(parts.Count==0)throw new InvalidOperationException(L.T("기본 얼굴을 먼저 여세요."));var path=Choose();if(path==null)return;var extra=TriFace.Read(path);if(parts.Sum(p=>(long)p.Vertices.Length*p.Morphs.Count)+(long)extra.Vertices.Length*extra.Morphs.Count>4000000)throw new InvalidOperationException(L.T("미리보기 리소스 합계가 너무 큽니다."));bool allow=false;if(parts[0].SameTopology(extra)&&!parts[0].SameReference(extra)){if(MessageBox.Show(L.T("면과 정점 순서 구조는 같지만 기본 형상이 다릅니다. 리소스 제작자가 호환을 명시한 경우에만 추가하세요. 확장 모프를 적용할까요?"), L.T("호환성 미확인"),MessageBoxButton.YesNo)!=MessageBoxResult.Yes)return;allow=true;}parts[0].AddMorphs(extra,allow);LoadedResources?.Invoke();}
 public event Action? LoadedResources;
 void Camera(){var offset=new Vector3D(Math.Sin(yaw)*Math.Cos(pitch),Math.Sin(pitch),Math.Cos(yaw)*Math.Cos(pitch))*distance;camera.Position=center+offset;camera.LookDirection=-offset;camera.UpDirection=new(0,1,0);camera.NearPlaneDistance=Math.Max(.01,distance/1000);camera.FarPlaneDistance=distance*100;}
 void SetScene(FaceScene? scene){textured=scene;while(models.Children.Count>2)models.Children.RemoveAt(2);if(scene!=null)models.Children.Add(scene.CreateVisual());else foreach(var shape in shapes)models.Children.Add(shape);FitScene();Update(current,null);LoadedResources?.Invoke();}
 void FitScene(){var bounds=models.Bounds;if(bounds.IsEmpty)return;center=new(bounds.X+bounds.SizeX/2,bounds.Y+bounds.SizeY/2,bounds.Z+bounds.SizeZ/2);distance=Math.Max(1,Math.Max(bounds.SizeX,bounds.SizeY)*2.3);Camera();}
 public bool Supports(Morph morph){if(morph.Backend=="MFG"&&morph.Id.StartsWith("modifier:")&&int.TryParse(morph.Id.Split(':')[1],out int modifier)&&modifier>=14)return false;if(!morph.Enabled)return true;string? n=morph.Backend=="MFG"?TriFace.MfgName(morph.Id):morph.Name;return n!=null&&(textured!=null?textured.Parts.Any(p=>p.Tri?.Morphs.ContainsKey(n)==true):parts.Any(p=>p.Morphs.ContainsKey(n)));}
 public IEnumerable<string> AvailableNames=>(textured!=null?textured.Parts.Where(p=>p.Tri!=null).SelectMany(p=>p.Tri!.Morphs.Keys):parts.SelectMany(p=>p.Morphs.Keys)).Distinct(StringComparer.OrdinalIgnoreCase).OrderBy(x=>x);
 public bool HasFace=>textured!=null||parts.Count>0;
 public void Update(Preset? preset,Morph? solo){current=preset;if(textured!=null){textured.Apply(preset);status.Text=L.T("텍스처 NIF 미리보기 · 게임 최종 합성과 다를 수 있습니다.")+"\n"+string.Join(" / ",textured.Parts.Where(p=>p.Tri!=null).Select(p=>p.Name));return;}var morphs=preset?.Channels.Values.SelectMany(c=>c.Morphs).Where(m=>m.Enabled&&(solo==null||m==solo)).ToList()??[];var visible=solo==null&&preset!=null?MorphSelection.Resolve(preset,true):morphs;var weights=visible.Select(m=>(name:m.Backend=="MFG"?TriFace.MfgName(m.Id):m.Name,weight:m.Strength/100)).Where(x=>x.name!=null).GroupBy(x=>x.name!,StringComparer.OrdinalIgnoreCase).Select(g=>(g.Key,Math.Clamp(g.Max(x=>x.weight),0,1))).ToList();
  for(int i=0;i<parts.Count;i++){var points=parts[i].Evaluate(weights);var geometry=new MeshGeometry3D{Positions=new Point3DCollection(points.Select(p=>new Point3D(p.X,p.Z,-p.Y))),TriangleIndices=new Int32Collection(parts[i].Indices)};shapes[i].Geometry=geometry;}
  if(parts.Count>0)status.Text=L.F("실제 TRI: {0} 부위 / {1} 정점 · 드래그 회전 / 휠 확대\n미지원 지정 모프: {2}. MFEE 등록 여부는 게임에서 별도 확인. 피부·눈·대사 합성 미재현.",parts.Count,parts.Sum(p=>p.Vertices.Length),morphs.Count(m=>!Supports(m)));
 }
}
