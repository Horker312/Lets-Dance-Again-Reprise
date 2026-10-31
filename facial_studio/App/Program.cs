using System.Windows;
using System.IO;
using System.Windows.Media;
using System.Windows.Media.Imaging;
namespace FacialStudio;
public static class Program {
 [STAThread]public static void Main(string[] args){
  bool smoke=args.Length==4&&args[0]=="--preview-smoke";
  if(smoke)L.SettingsPath=Path.Combine(Path.GetDirectoryName(Path.GetFullPath(args[3]))!,"smoke-language.json");
  var app=new Application();app.DispatcherUnhandledException+=(_,e)=>{MessageBox.Show(e.Exception.Message,L.T("Facial Studio 오류"));e.Handled=true;};var window=new MainWindow();
  if(smoke)window.Loaded+=async(_,_)=>{try{window.LoadPreviewSmoke(args[1],args[2]);await Task.Delay(200);window.UpdateLayout();var bitmap=new RenderTargetBitmap((int)window.ActualWidth,(int)window.ActualHeight,96,96,PixelFormats.Pbgra32);bitmap.Render(window);var encoder=new PngBitmapEncoder();encoder.Frames.Add(BitmapFrame.Create(bitmap));using(var stream=File.Create(args[3]))encoder.Save(stream);}catch(Exception error){File.WriteAllText(args[3]+".error.txt",error.ToString());Environment.ExitCode=1;}finally{window.CleanForSmoke();window.Close();}};
  app.Run(window);
 }
}
