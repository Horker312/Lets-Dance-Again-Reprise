using System.IO;
using System.Text.Json;
namespace DanceStudio;
public static class FovTests {
 public static void Run(string projectPath,string output){
  Directory.CreateDirectory(output);int checks=0;
  void Check(bool value,string why){checks++;if(!value)throw new Exception(why);}
  void Reject(Action action,string why){bool failed=false;try{action();}catch{failed=true;}Check(failed,why);}
  var original=SongProject.Load(projectPath);var rig=Rig.Load(original.Skeleton);var dance=MotionClip.Load(original.Dance.Path);var camera=MotionClip.Load(original.Camera.Path);var audio=WaveAudio.Load(original.Audio.Path);
  Check(!FovPolicy.HasVerifiedSource(camera),"Booo without float tracks never claims Auto");
  var unknown=new MotionClip{Duration=2,Tracks=1,Samples=[[],[]]}; // Only the trust gate is exercised.
  Check(!FovPolicy.HasVerifiedSource(unknown),"No source-name or float-index heuristic");
  string? firstDance=null,firstMusic=null;
  foreach(double value in new[]{30.0,45.0,60.0,70.0}){
   var p=SongProject.Load(projectPath);p.Fov=value;p.ExportOut=2;p.Save(Path.Combine(output,"manual.danceproj"));var saved=SongProject.Load(Path.Combine(output,"manual.danceproj"));Check(saved.Fov==value&&saved.FovMode=="Manual"&&saved.SchemaVersion==5,"Manual FOV saved exactly");
   Check(FovPolicy.Value(p,camera,0)==value&&FovPolicy.Value(p,camera,1)==value,"Preview uses explicit manual FOV");
   var folder=GameExporter.Export(output,p,dance,camera,audio,rig);using var doc=JsonDocument.Parse(File.ReadAllText(Path.Combine(folder,"camera.samples.json")));Check(doc.RootElement.GetProperty("fov").GetDouble()==value&&doc.RootElement.GetProperty("schemaVersion").GetInt32()==1,"Manual export retains schema 1 and exact value");
   var dh=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(File.ReadAllBytes(Path.Combine(folder,"mt_idle.hkx"))));var mh=Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(File.ReadAllBytes(Path.Combine(folder,"music.wav"))));if(firstDance==null){firstDance=dh;firstMusic=mh;}else{Check(firstDance==dh&&firstMusic==mh,"FOV changes do not change dance HKX or music");}
   File.WriteAllText(Path.Combine(output,$"manual-{value}.txt"),Path.Combine(folder,"camera.samples.json"));
  }
  foreach(int version in new[]{1,2,3}){var raw=JsonSerializer.SerializeToNode(original,SongProject.Json)!;raw["SchemaVersion"]=version;raw["Fov"]=70;raw["FovMode"]="Auto";var path=Path.Combine(output,"old.danceproj");File.WriteAllText(path,raw.ToJsonString());Reject(()=>SongProject.Load(path),"pre-independent-FPS schema is rejected explicitly");}
  original.FovMode="Auto";Reject(()=>FovPolicy.Value(original,camera,0),"Unknown Auto fails explicitly, never falls back to 45");
  foreach(double bad in new[]{double.NaN,double.PositiveInfinity,0,4.99,140.01})Reject(()=>FovPolicy.Valid(bad),"Invalid FOV rejected");
  foreach(double aspect in new[]{4.0/3,16.0/9,21.0/9})foreach(double fov in new[]{30.0,45.0,60.0,70.0}){
   var view=new PreviewView(false){Width=800,Height=600};view.Measure(new System.Windows.Size(800,600));view.Arrange(new System.Windows.Rect(0,0,800,600));view.SetAspect(aspect);view.UpdateLayout();view.Camera(new Pose(System.Numerics.Vector3.Zero,System.Numerics.Quaternion.Identity,System.Numerics.Vector3.One),fov,0,"+Y");
   var flags=System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic;var lens=(System.Windows.Media.Media3D.PerspectiveCamera)typeof(PreviewView).GetField("viewCamera",flags)!.GetValue(view)!;var viewport=(System.Windows.Controls.Viewport3D)typeof(PreviewView).GetField("viewport",flags)!.GetValue(view)!;
   Check(lens.FieldOfView==fov,"Actual WPF horizontal FOV uses manual value");Check(Math.Abs(viewport.Width/viewport.Height-aspect)<1e-9,"Actual preview viewport preserves selected aspect");
  }
  var curve=new FovCurve((0,60),(1,30),(2,70));Check(curve.At(0)==60&&curve.At(1)==30&&curve.At(2)==70&&curve.At(.5)==45&&curve.At(1.5)==50,"Synthetic degree curve interpolation");
  var edit=new TrackEdit{In=.5,Out=1.5,Start=.25,Speed=2};Check(curve.Edited(edit,.25)==45&&curve.Edited(edit,.5)==30&&curve.Edited(edit,.75)==50,"Trim offset and speed share camera source-time mapping");
  Check(curve.Edited(edit,0)==45&&curve.Edited(edit,2)==50,"Edited curve clamps held endpoints");
  foreach(var keys in new[]{new[]{(0.0,60.0),(0.0,30.0)},new[]{(0.0,60.0),(1.0,double.NaN)}})Reject(()=>new FovCurve(keys),"Invalid synthetic keys rejected");
  var fixture=new{schemaVersion=2,fovConvention="skyrimWorldDegrees",fov=45,yawDegrees=0,forward="+Y",duration=2,samples=Enumerable.Range(0,49).Select(i=>new{time=i/24.0,position=new[]{0,0,0},rotation=new[]{0,0,0,1},fov=curve.At(i/24.0)})};
  File.WriteAllText(Path.Combine(output,"synthetic-camera-v2.json"),JsonSerializer.Serialize(fixture,SongProject.Json));
  File.WriteAllText(Path.Combine(output,"validation.json"),JsonSerializer.Serialize(new{passed=true,checks,autoSourceAvailable=false,curveIsSynthetic=true,gameTested=false},SongProject.Json));
 }
}
