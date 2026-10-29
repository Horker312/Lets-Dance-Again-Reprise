using System.IO;
using System.Reflection;
using HKX2E;
using System.Windows;
using System.Windows.Threading;
using System.Windows.Media;
using System.Windows.Media.Imaging;
namespace DanceStudio;
public static class Program {
 [STAThread] public static void Main(string[] args) {
  try {
   if(args.Length>0&&args[0]=="--localization-test"){LocalizationTests.Run(args[1]);return;}

   if(args.Length>0&&args[0]=="--inspect") {
    var a=MotionClip.Read(args[1]).Container.animations!.Single();var values=new Dictionary<string,object?>{{"type",a.GetType().Name}};
    foreach(var p in a.GetType().GetProperties()) {
     if(p.Name=="data")continue;
     object? value=p.GetValue(a);if(value is System.Collections.IEnumerable list&&value is not string)value=list.Cast<object>().Take(20).Select(x=>x.ToString()).ToArray();values[p.Name]=value;
    }
    if(a is hkaSplineCompressedAnimation c){values["dataStart"]=c.data.Take(64).ToArray();values["floatSections"]=c.floatBlockOffsets.Take(5).Select(off=>new{offset=off,bytes=c.data.Skip((int)off).Take(32).ToArray()}).ToArray();}
    File.WriteAllText(args[2],System.Text.Json.JsonSerializer.Serialize(values,SongProject.Json));return;
   }
   if (args.Length>0 && args[0]=="--probe") {
    var lines=new List<string>();
    foreach(var path in args.Skip(1)) {
     using var stream=File.OpenRead(path); var d=new PackFileDeserializer();
     var root=(hkRootLevelContainer)d.Deserialize(new BinaryReaderEx(stream));
     var c=root.namedVariants.Select(v=>v!.variant).OfType<hkaAnimationContainer>().Single();
     lines.Add(path);
     foreach(var a in c.animations!) {
      lines.Add($"{a.GetType().Name} duration={a.duration} tracks={a.numberOfTransformTracks} floats={a.numberOfFloatTracks} rootmotion={a.extractedMotion?.GetType().Name}");
      foreach(var f in a.GetType().GetFields().Where(f=>f.Name.Contains("num")||f.Name.Contains("float")||f.Name.Contains("mask")))lines.Add(f.Name+"="+f.GetValue(a));
      var t=Assembly.LoadFrom(Path.Combine(AppContext.BaseDirectory,"Matcher.dll")).GetType("MotionEditor.Core.SplinePreview")!;
      var method=t.GetMethod("Decode",BindingFlags.Public|BindingFlags.NonPublic|BindingFlags.Static)!;
      var frames=(Array)method.Invoke(null,new object[]{a})!;
      lines.Add("decoded frames="+frames.Length);
     }
     foreach(var binding in c.bindings)lines.Add("binding="+binding.originalSkeletonName+" bones="+string.Join(",",binding.transformTrackToBoneIndices.Take(12)));
    }
    File.WriteAllLines(Path.Combine(AppContext.BaseDirectory,"probe.txt"),lines);
    return;
   }
   if(args.Length>0&&args[0]=="--export-game") {
    var p=SongProject.Load(args[1]);var rig=Rig.Load(p.Skeleton);string folder=GameExporter.Export(args[2],p,p.Dance.Path.Length>0?MotionClip.Load(p.Dance.Path,p.Dance.HkxBindingIndex,p.Dance.IdentityRigProof):null,p.Camera.Path.Length>0?MotionClip.Load(p.Camera.Path,p.Camera.HkxBindingIndex,p.Camera.IdentityRigProof):null,AudioImport.Load(p.Audio.Path),rig);File.WriteAllText(args[3],folder);return;
   }
   if(args.Length>0&&args[0]=="--hkx-diagnose"){File.WriteAllText(args[2],HkxDiagnostics.Inspect(args[1],Rig.Load(Rig.BuiltinId)).Json());return;}
   if(args.Length>0&&args[0]=="--xwm-test"){XwmImportTests.Run(args[1],args[2]);return;}
   if(args.Length>0&&args[0]=="--content-test"){ContentTests.Run(args[1],args[2]);return;}
   if(args.Length>0&&args[0]=="--ui-feature-test"){UiFeatureTests.Run(args[1],args[2]);return;}
   if(args.Length>0&&args[0]=="--sync-mapping-test"){SyncMappingTests.Run(args[1],args[2]);return;}
   if(args.Length>0&&args[0]=="--independent-rate-test"){IndependentRateTests.Run(args[1],args[2]);return;}
   if(args.Length>0&&args[0]=="--preservation-test"){PreservationTests.Run(args[1],args[2]);return;}
   if(args.Length>0&&args[0]=="--fov-test"){FovTests.Run(args[1],args[2]);return;}
   if(args.Length>0&&args[0]=="--volume-test"){VolumeTests.Run(args[1]);return;}
   if(args.Length>0&&args[0]=="--rate-test"){RateTests.Run(args[1],args[2]);return;}
   if(args.Length>0&&args[0]=="--embedded-rig-test"){EmbeddedRigTests.Run(args[1],args[2]);return;}
   if(args.Length>0&&args[0]=="--transform-test"){TransformTests.Run(args[1],args[2]);return;}
   if(args.Length>0&&args[0]=="--self-test"){SelfTests.Run(args[1]);return;}
   if(args.Length>0&&args[0]=="--batch-test"){SelfTests.Batch(args[1],args[2]);return;}
   if(args.Length>0&&args[0]=="--catalog"){Catalog.Build(args[1],args[2]);return;}
   if(args.Length>0&&args[0]=="--smoke")System.Windows.Media.RenderOptions.ProcessRenderMode=System.Windows.Interop.RenderMode.SoftwareOnly;
   var app=new Application();
   app.DispatcherUnhandledException+=(_,e)=>{File.WriteAllText(Path.Combine(AppContext.BaseDirectory,"error.txt"),e.Exception.ToString());MessageBox.Show(e.Exception.Message,Brand.Name);e.Handled=true;};
   var window=new MainWindow();
   if(args.Length>0&&args[0]=="--smoke"){
    window.ShowInTaskbar=false;window.Loaded+=async(_,_)=>{
     try{
      await window.LoadProject(SongProject.Load(args[1]));
      var transport=await window.TransportSmoke();File.WriteAllText(args[2]+".transport.json",System.Text.Json.JsonSerializer.Serialize(transport,SongProject.Json));window.Seek(25);window.CleanForSmoke();window.UpdateLayout();
      await Task.Delay(350);window.UpdateLayout();
      if(args.Length>3&&args[3]=="controls"){void Scroll(System.Windows.DependencyObject obj){if(obj is System.Windows.Controls.ScrollViewer viewer&&viewer.ScrollableHeight>0)viewer.ScrollToVerticalOffset(650);for(int i=0;i<System.Windows.Media.VisualTreeHelper.GetChildrenCount(obj);i++)Scroll(System.Windows.Media.VisualTreeHelper.GetChild(obj,i));}Scroll(window);window.UpdateLayout();await Task.Delay(120);}
      object? Field(object obj,string name)=>obj.GetType().GetField(name,BindingFlags.Instance|BindingFlags.NonPublic)?.GetValue(obj);
      object Scene(string name){var preview=Field(window,name)!;var body=(System.Windows.Media.Media3D.Model3DGroup)Field(preview,"body")!;return new{models=body.Children.Count,bounds=body.Bounds.ToString()};}
      File.WriteAllText(args[2]+".scene.json",System.Text.Json.JsonSerializer.Serialize(new{rig=window.Rig?.Names.Length,tracks=window.Dance?.Tracks,stage=Scene("stage"),camera=Scene("camera")},SongProject.Json));
      var bitmap=new RenderTargetBitmap((int)window.ActualWidth,(int)window.ActualHeight,96,96,PixelFormats.Pbgra32);bitmap.Render(window);
      var png=new PngBitmapEncoder();png.Frames.Add(BitmapFrame.Create(bitmap));using(var f=File.Create(args[2]))png.Save(f);
      window.CleanForSmoke();window.Close();
     }catch(Exception ex){File.WriteAllText(args[2]+".error.txt",ex.ToString());Environment.ExitCode=1;window.CleanForSmoke();window.Close();}
    };
   }else if(args.Length>0&&File.Exists(args[0]))window.Loaded+=async(_,_)=>{try{await window.LoadProject(SongProject.Load(args[0]));}catch(Exception ex){MessageBox.Show(ex.Message,UiText.T("t0319"));}};
   app.Run(window);
  } catch(Exception ex) {File.WriteAllText(Path.Combine(AppContext.BaseDirectory,"error.txt"),ex.ToString());Environment.ExitCode=1;}
 }
}

