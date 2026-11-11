using System.IO;
using System.Runtime.CompilerServices;
using System.Text.Json;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using System.Windows.Media.Imaging;
using FaceAssets;
namespace FacialStudio;

// Display-only approximation approved from the side-by-side test. Never rewrites DDS or preset colors.
public static class SkinPreviewColor {
 sealed class State {public Material? Original,Converted;}
 static readonly ConditionalWeakTable<FacePart,State> states=new();
 static string SettingsFile=>Path.Combine(Path.GetDirectoryName(L.SettingsPath)!,"preview.json");
 public static bool ReadPreference(){try{using var d=JsonDocument.Parse(File.ReadAllText(SettingsFile));return d.RootElement.GetProperty("skinDisplayCorrection").GetBoolean();}catch{return true;}}
 public static void SavePreference(bool enabled){Directory.CreateDirectory(Path.GetDirectoryName(SettingsFile)!);string temp=SettingsFile+Guid.NewGuid()+".tmp";try{File.WriteAllText(temp,JsonSerializer.Serialize(new{skinDisplayCorrection=enabled}));File.Move(temp,SettingsFile,true);}finally{if(File.Exists(temp))File.Delete(temp);}}
 public static void RestoreOriginalsInCopy(FaceScene source,FaceScene copy){for(int i=0;i<source.Parts.Count;i++)if(states.TryGetValue(source.Parts[i],out var state)&&state.Original!=null)copy.Parts[i].Material=state.Original;}
 public static void Apply(FaceScene scene,bool enabled){foreach(var part in scene.Parts.Where(p=>p.HeadPartType==1)){
  var state=states.GetValue(part,_=>new State());if(!ReferenceEquals(part.Material,state.Converted)&&!ReferenceEquals(part.Material,state.Original)){state.Original=part.Material;state.Converted=null;}
  if(!enabled){if(state.Original!=null)part.Material=state.Original;continue;}
  if(state.Converted==null&&state.Original is DiffuseMaterial{Brush:ImageBrush{ImageSource:BitmapSource source} brush}){
   var image=ConvertForDisplay(source);var corrected=brush.Clone();corrected.ImageSource=image;corrected.Freeze();var material=new DiffuseMaterial(corrected);material.Freeze();state.Converted=material;
  }
  if(state.Converted!=null)part.Material=state.Converted;
 }}
 public static BitmapSource ConvertForDisplay(BitmapSource source){var image=new FormatConvertedBitmap(source,PixelFormats.Bgra32,null,0);int stride=checked(image.PixelWidth*4);byte[] bytes=new byte[checked(stride*image.PixelHeight)];image.CopyPixels(bytes,stride,0);for(int i=0;i<bytes.Length;i++)if(i%4!=3){double c=bytes[i]/255.0;bytes[i]=(byte)Math.Clamp(Math.Round(255*(c<=.0031308?12.92*c:1.055*Math.Pow(c,1/2.4)-.055)),0,255);}var result=BitmapSource.Create(image.PixelWidth,image.PixelHeight,96,96,PixelFormats.Bgra32,null,bytes,stride);result.Freeze();return result;}
}
