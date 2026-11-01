using System.Windows;
using System.IO;
using System.Windows.Media;
using System.Windows.Media.Imaging;
namespace FacialStudio;
public static class Program {
 [STAThread]public static void Main(string[] args){
  bool jslot=args.Length==4&&(args[0]=="--jslot-smoke"||args[0]=="--jslot-smoke-confirmed");bool smoke=args.Length==4&&args[0]=="--preview-smoke";
  if(smoke||jslot)L.SettingsPath=Path.Combine(Path.GetDirectoryName(Path.GetFullPath(args[3]))!,"smoke-language.json");
  var app=new Application();app.DispatcherUnhandledException+=(_,e)=>{MessageBox.Show(e.Exception.Message,L.T("Facial Studio 오류"));e.Handled=true;};var window=new MainWindow();
  if(jslot)window.Loaded+=async(_,_)=>{try{var result=window.LoadRaceMenuSmoke(args[1],args[2],args[0]=="--jslot-smoke-confirmed");File.WriteAllText(args[3]+".report.json",System.Text.Json.JsonSerializer.Serialize(new{result.Status,result.MorphsApplied,result.SculptVertices,result.Messages,parts=result.Scene.Parts.Select(p=>new{p.Name,vertices=p.Reference.Length,expressions=p.Tri?.Morphs.Count})},new System.Text.Json.JsonSerializerOptions{WriteIndented=true}));await Task.Delay(150);window.UpdateLayout();var image=new RenderTargetBitmap((int)window.ActualWidth,(int)window.ActualHeight,96,96,PixelFormats.Pbgra32);image.Render(window);var encoder=new PngBitmapEncoder();encoder.Frames.Add(BitmapFrame.Create(image));using var stream=File.Create(args[3]);encoder.Save(stream);}catch(Exception e){File.WriteAllText(args[3]+".error.txt",e.ToString());Environment.ExitCode=1;}finally{window.CleanForSmoke();window.Close();}};
  if(smoke)window.Loaded+=async(_,_)=>{try{window.LoadPreviewSmoke(args[1],args[2]);await Task.Delay(200);window.UpdateLayout();var bitmap=new RenderTargetBitmap((int)window.ActualWidth,(int)window.ActualHeight,96,96,PixelFormats.Pbgra32);bitmap.Render(window);var encoder=new PngBitmapEncoder();encoder.Frames.Add(BitmapFrame.Create(bitmap));using(var stream=File.Create(args[3]))encoder.Save(stream);}catch(Exception error){File.WriteAllText(args[3]+".error.txt",error.ToString());Environment.ExitCode=1;}finally{window.CleanForSmoke();window.Close();}};
  app.Run(window);
 }
}
