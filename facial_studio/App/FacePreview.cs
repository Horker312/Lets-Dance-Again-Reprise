using System.IO;
using System.Windows.Documents;
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
 RaceMenuImport.Result? lastImport;string? resourceRoot;readonly TextBox importDetails=new(){IsReadOnly=true,TextWrapping=TextWrapping.Wrap,VerticalScrollBarVisibility=ScrollBarVisibility.Auto,MaxHeight=140};
 readonly TextBlock importSummary=new(){TextWrapping=TextWrapping.Wrap};readonly CheckBox hair=new(){IsChecked=true};
 FaceScene? textured;Preset? current;
 readonly Viewport3D viewport=new();readonly PerspectiveCamera camera=new(){FieldOfView=40};readonly TextBlock status=new(){Text=L.T("실제 얼굴 TRI를 불러오세요. 무텍스처 형상 미리보기입니다."),TextWrapping=TextWrapping.Wrap};
 readonly Model3DGroup models=new();readonly List<TriFace> parts=[];readonly List<GeometryModel3D> shapes=[];double yaw=Math.PI,pitch,distance=50;Point last;bool dragging;Point3D center;
 public FacePreview(){
 Assets=new FaceAssetPanel(SetScene,L.T){Header=L.T("수동 리소스 연결"),IsExpanded=false};SetDock(Assets,Dock.Bottom);Children.Add(Assets);
 var buttons=new WrapPanel();SetDock(buttons,Dock.Top);Children.Add(buttons);
 Button Make(string name,Action action){var b=new Button{Content=L.T(name),Padding=new Thickness(5),Margin=new Thickness(2)};b.Click+=(_,_)=>{try{action();}catch(Exception e){status.Text=L.T("로딩 실패: ")+e.Message;importDetails.Text=e.ToString()+"\n"+importDetails.Text;}};return b;}
 buttons.Children.Add(Make("RaceMenu 프리셋 불러오기",OpenRaceMenu));hair.Content=L.T("머리카락 표시");hair.Margin=new Thickness(8,3,8,3);hair.Click+=(_,_)=>{if(textured!=null){textured.ShowHair=hair.IsChecked==true;RebuildVisual();}};buttons.Children.Add(hair);buttons.Children.Add(Make("정면 보기",()=>{yaw=Math.PI;pitch=0;Camera();}));buttons.Children.Add(Make("얼굴 중심 재설정",FitScene));
 var fallback=new WrapPanel();fallback.Children.Add(Make("리소스 Data 폴더",ChooseRoot));fallback.Children.Add(Make("얼굴 TRI 열기",()=>Load(false)));fallback.Children.Add(Make("눈·입 부위 TRI 추가",()=>Load(true)));fallback.Children.Add(Make("확장 모프 TRI 추가",Overlay));var manual=new Expander{Header=L.T("TRI 단독 미리보기 · 경로 변경"),Content=fallback};SetDock(manual,Dock.Bottom);Children.Add(manual);
 var diagnostics=new StackPanel();diagnostics.Children.Add(Make("진단 결과 저장",SaveDiagnostics));diagnostics.Children.Add(importDetails);var detail=new Expander{Header=L.T("상세 진단"),Content=diagnostics};SetDock(detail,Dock.Bottom);Children.Add(detail);
 var overview=new Expander{Header=L.T("얼굴 불러오기 결과"),Content=importSummary,IsExpanded=false};SetDock(overview,Dock.Top);Children.Add(overview);
 SetDock(status,Dock.Bottom);Children.Add(status);Children.Add(viewport);viewport.MinHeight=180;viewport.Camera=camera;viewport.SizeChanged+=(_,_)=>Camera();
  models.Children.Add(new AmbientLight(Color.FromRgb(110,110,110)));models.Children.Add(new DirectionalLight(Colors.White,new Vector3D(-1,-1,2)));viewport.Children.Add(new ModelVisual3D{Content=models});
  viewport.MouseLeftButtonDown+=(_,e)=>{dragging=true;last=e.GetPosition(viewport);viewport.CaptureMouse();};viewport.MouseLeftButtonUp+=(_,_)=>{dragging=false;viewport.ReleaseMouseCapture();};viewport.LostMouseCapture+=(_,_)=>dragging=false;
  viewport.MouseMove+=(_,e)=>{if(!dragging)return;var p=e.GetPosition(viewport);yaw+=(p.X-last.X)*.01;pitch=Math.Clamp(pitch+(p.Y-last.Y)*.01,-1.4,1.4);last=p;Camera();};viewport.MouseWheel+=(_,e)=>{distance=Math.Clamp(distance*Math.Exp(-e.Delta*.001),.5,10000);Camera();e.Handled=true;};
 }
 public RaceMenuImport.Result LoadRaceMenu(string path,string root,bool confirmed=false,string? exported=null){var result=RaceMenuImport.Load(path,root,confirmed,exported);lastImport=result;resourceRoot=root;result.Scene.ShowHair=hair.IsChecked==true;Assets.AdoptScene(result.Scene);Assets.SetPreviewEnabled(true);ShowImport(result);return result;}
 void ShowImport(RaceMenuImport.Result result){importDetails.Text=result.Summary;var report=ImportOverview.Describe(result.Scene,result.Messages);importSummary.Inlines.Clear();foreach(var item in report){var run=new Run(L.T(item.Label)+": "+L.T(item.State));ToolTipService.SetToolTip(run,item.Detail);importSummary.Inlines.Add(run);importSummary.Inlines.Add(new LineBreak());}status.Text=L.T(report.Any(r=>r.Label=="눈"&&r.State.Contains("누락"))?"눈 리소스 누락 · 얼굴 불러오기가 완전하지 않습니다.":"얼굴 연결됨 · 얼굴형·재질의 제한은 결과에서 확인하세요.");}
 void SaveDiagnostics(){var d=new SaveFileDialog{Filter="Text|*.txt",FileName="Facial-Studio-face-diagnostics.txt"};if(d.ShowDialog(Window.GetWindow(this))==true)File.WriteAllText(d.FileName,(lastImport==null?importSummary.Text:string.Join("\n",ImportOverview.Describe(lastImport.Scene,lastImport.Messages).Select(i=>L.T(i.Label)+": "+L.T(i.State)+"\n"+i.Detail)))+"\n\n"+importDetails.Text);}
 void ChooseRoot(){var d=new OpenFolderDialog{Title=L.T("게임 Data 또는 추출한 리소스 폴더 선택")};if(d.ShowDialog(Window.GetWindow(this))==true)resourceRoot=d.FolderName;}
 string? ChooseExport(string slot,string root){var candidates=ExportedFace.Candidates(slot,root);if(candidates.Count==0)return null;
  var list=new ListBox{ItemsSource=candidates,MaxHeight=220,MinWidth=420,Foreground=Brushes.Black,Background=Brushes.White};var panel=new StackPanel{Margin=new Thickness(14)};panel.Children.Add(new TextBlock{Text=L.T("기본 슬라이더를 완전히 복원하려면 RaceMenu에서 내보낸 얼굴 NIF를 선택하세요. 같은 이름만으로 동일한 얼굴임을 보장하지 않습니다. 선택 후 메시·UV 대응을 검사합니다."),TextWrapping=TextWrapping.Wrap,MaxWidth=580,Margin=new Thickness(0,0,0,10)});panel.Children.Add(list);var row=new WrapPanel();var use=new Button{Content=L.T("이 얼굴 NIF 사용")};var skip=new Button{Content=L.T("NIF 없이 계속")};row.Children.Add(use);row.Children.Add(skip);panel.Children.Add(row);var dialog=new Window{Title=L.T("내보낸 얼굴 NIF 확인"),Owner=Window.GetWindow(this),Content=panel,SizeToContent=SizeToContent.WidthAndHeight,WindowStartupLocation=WindowStartupLocation.CenterOwner,ResizeMode=ResizeMode.NoResize};use.Click+=(_,_)=>{if(list.SelectedItem!=null)dialog.DialogResult=true;};skip.Click+=(_,_)=>dialog.DialogResult=false;return dialog.ShowDialog()==true?list.SelectedItem as string:null;
 }
 void OpenRaceMenu(){var d=new OpenFileDialog{Filter="RaceMenu preset|*.jslot"};if(d.ShowDialog(Window.GetWindow(this))!=true)return;resourceRoot??=GameResources.Detect();if(resourceRoot==null)ChooseRoot();if(resourceRoot==null){status.Text=L.T("리소스 폴더를 선택하지 않았습니다. 기존 얼굴을 유지합니다.");return;}
  var first=RaceMenuImport.Load(d.FileName,resourceRoot);bool confirmed=false;string? exported=ChooseExport(d.FileName,resourceRoot);
  if(first.Messages.Any(m=>m.Contains("correspondence not proven")||m.Contains("Extension topology/reference rejected")||m.Contains("explicit correspondence confirmation"))&&MessageBox.Show(Window.GetWindow(this),L.T("지정된 HDPT의 NIF와 TRI가 같은 인덱스 토폴로지지만 기준 형상이 다릅니다. 설치 리소스의 정점 대응이 유지됨을 확인한 경우에만 다시 불러오세요. 허용할까요?"),L.T("정점 대응 확인"),MessageBoxButton.YesNo)==MessageBoxResult.Yes)confirmed=true;
  LoadRaceMenu(d.FileName,resourceRoot,confirmed,exported);
 }
 void RebuildVisual(){while(models.Children.Count>2)models.Children.RemoveAt(2);if(textured!=null)models.Children.Add(textured.CreateVisual());}

 string? Choose(){var d=new OpenFileDialog{Filter=L.T("얼굴 메시·모프 (FRTRI003)|*.tri")};return d.ShowDialog()==true?d.FileName:null;}
 void Load(bool add){var path=Choose();if(path!=null)LoadFile(path,add);}
 public void LoadFile(string path,bool add=false){var face=TriFace.Read(path);if(face.Indices.Length==0)throw new InvalidOperationException(L.T("기본 얼굴에는 삼각형이 필요합니다."));if(add&&parts.Sum(p=>(long)p.Vertices.Length*p.Morphs.Count)+(long)face.Vertices.Length*face.Morphs.Count>4000000)throw new InvalidOperationException(L.T("미리보기 리소스 합계가 너무 큽니다."));if(add&&parts.Count>=16)throw new InvalidOperationException(L.T("최대 16개 부위를 지원합니다."));Assets.SetPreviewEnabled(false);if(!add){parts.Clear();shapes.Clear();while(models.Children.Count>2)models.Children.RemoveAt(2);}parts.Add(face);
  var material=new DiffuseMaterial(new SolidColorBrush(Color.FromRgb(206,176,153)));var shape=new GeometryModel3D{Material=material,BackMaterial=material};shapes.Add(shape);models.Children.Add(shape);var xs=parts[0].Vertices.Select(p=>p.X);var ys=parts[0].Vertices.Select(p=>p.Y);var zs=parts[0].Vertices.Select(p=>p.Z);center=new((xs.Min()+xs.Max())/2,(zs.Min()+zs.Max())/2,-(ys.Min()+ys.Max())/2);distance=Math.Max(1,Math.Max(xs.Max()-xs.Min(),zs.Max()-zs.Min())*2.5);Camera();Update(null,null);LoadedResources?.Invoke();
 }
 void Overlay(){if(parts.Count==0)throw new InvalidOperationException(L.T("기본 얼굴을 먼저 여세요."));var path=Choose();if(path==null)return;var extra=TriFace.Read(path);if(parts.Sum(p=>(long)p.Vertices.Length*p.Morphs.Count)+(long)extra.Vertices.Length*extra.Morphs.Count>4000000)throw new InvalidOperationException(L.T("미리보기 리소스 합계가 너무 큽니다."));bool allow=false;if(parts[0].SameTopology(extra)&&!parts[0].SameReference(extra)){if(MessageBox.Show(L.T("면과 정점 순서 구조는 같지만 기본 형상이 다릅니다. 리소스 제작자가 호환을 명시한 경우에만 추가하세요. 확장 모프를 적용할까요?"), L.T("호환성 미확인"),MessageBoxButton.YesNo)!=MessageBoxResult.Yes)return;allow=true;}parts[0].AddMorphs(extra,allow);LoadedResources?.Invoke();}
 public event Action? LoadedResources;
 void Camera(){var offset=new Vector3D(Math.Sin(yaw)*Math.Cos(pitch),Math.Sin(pitch),Math.Cos(yaw)*Math.Cos(pitch))*(distance*Math.Max(1,viewport.ActualWidth/Math.Max(1,viewport.ActualHeight)));camera.Position=center+offset;camera.LookDirection=-offset;camera.UpDirection=new(0,1,0);camera.NearPlaneDistance=Math.Max(.01,distance/1000);camera.FarPlaneDistance=distance*100;}
 void SetScene(FaceScene? scene){if(scene?.JslotPath==null){lastImport=null;importSummary.Text="";importDetails.Text="";}else if(!ReferenceEquals(lastImport?.Scene,scene))lastImport=new(scene,scene.Warnings.ToList(),0,0);hair.IsChecked=scene?.ShowHair??true;textured=scene;while(models.Children.Count>2)models.Children.RemoveAt(2);if(scene!=null)models.Children.Add(scene.CreateVisual());else foreach(var shape in shapes)models.Children.Add(shape);FitScene();Update(current,null);if(lastImport!=null)ShowImport(lastImport);LoadedResources?.Invoke();}
 void FitScene(){var bounds=textured?.Parts.FirstOrDefault(p=>p.HeadPartType==1)?.Mesh.Bounds??models.Bounds;if(bounds.IsEmpty)return;center=new(bounds.X+bounds.SizeX/2,bounds.Y+bounds.SizeY/2,bounds.Z+bounds.SizeZ/2);distance=Math.Max(1,Math.Max(bounds.SizeX,bounds.SizeY)*2.3);Camera();}
 public bool Supports(Morph morph){if(morph.Backend=="MFG"&&morph.Id.StartsWith("modifier:")&&int.TryParse(morph.Id.Split(':')[1],out int modifier)&&modifier>=14)return false;if(!morph.Enabled)return true;string? n=morph.Backend=="MFG"?TriFace.MfgName(morph.Id):morph.Name;return n!=null&&(textured!=null?textured.Parts.Any(p=>p.Tri?.Morphs.ContainsKey(n)==true):parts.Any(p=>p.Morphs.ContainsKey(n)));}
 public IEnumerable<string> AvailableNames=>(textured!=null?textured.Parts.Where(p=>p.Tri!=null).SelectMany(p=>p.Tri!.Morphs.Keys):parts.SelectMany(p=>p.Morphs.Keys)).Distinct(StringComparer.OrdinalIgnoreCase).OrderBy(x=>x);
 public bool HasFace=>textured!=null||parts.Count>0;
 public void Update(Preset? preset,Morph? solo){current=preset;if(textured!=null){textured.Apply(preset);if(textured.JslotPath!=null&&lastImport!=null)return;status.Text=(textured.JslotPath!=null?L.T("부분 불러오기 · 게임 얼굴과 동일함을 보장하지 않습니다."):L.T("텍스처 NIF 미리보기 · 게임 최종 합성과 다를 수 있습니다."))+"\n"+string.Join(" / ",textured.Parts.Where(p=>p.Tri!=null).Select(p=>p.Name));return;}var morphs=preset?.Channels.Values.SelectMany(c=>c.Morphs).Where(m=>m.Enabled&&(solo==null||m==solo)).ToList()??[];var visible=solo==null&&preset!=null?MorphSelection.Resolve(preset,true):morphs;var weights=visible.Select(m=>(name:m.Backend=="MFG"?TriFace.MfgName(m.Id):m.Name,weight:m.Strength/100)).Where(x=>x.name!=null).GroupBy(x=>x.name!,StringComparer.OrdinalIgnoreCase).Select(g=>(g.Key,Math.Clamp(g.Max(x=>x.weight),0,1))).ToList();
  for(int i=0;i<parts.Count;i++){var points=parts[i].Evaluate(weights);var geometry=new MeshGeometry3D{Positions=new Point3DCollection(points.Select(p=>new Point3D(p.X,p.Z,-p.Y))),TriangleIndices=new Int32Collection(parts[i].Indices)};shapes[i].Geometry=geometry;}
  if(parts.Count>0)status.Text=L.F("실제 TRI: {0} 부위 / {1} 정점 · 드래그 회전 / 휠 확대\n미지원 지정 모프: {2}. MFEE 등록 여부는 게임에서 별도 확인. 피부·눈·대사 합성 미재현.",parts.Count,parts.Sum(p=>p.Vertices.Length),morphs.Count(m=>!Supports(m)));
 }
}
