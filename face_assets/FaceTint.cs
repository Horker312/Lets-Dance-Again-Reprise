using System.Text.Json;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Media.Media3D;
namespace FaceAssets;

// WPF albedo-only approximation. Uses the actual ordered jslot masks/colors;
// does not reproduce Skyrim skin lighting, normal maps, ENB or shader blend modes.
public static class FaceTint {
 public static void ApplyHair(FaceScene scene,uint color){foreach(var p in scene.Parts.Where(p=>p.UsesHairTint&&p.Diffuse!=null&&p.HeadPartType is 3 or 6)){var image=p.Diffuse!;var b=Pixels(image);for(int i=0;i<b.Length;i+=4){b[i]=(byte)(b[i]*(color&255)/255);b[i+1]=(byte)(b[i+1]*((color>>8)&255)/255);b[i+2]=(byte)(b[i+2]*((color>>16)&255)/255);}var tinted=BitmapSource.Create(image.PixelWidth,image.PixelHeight,96,96,PixelFormats.Bgra32,null,b,image.PixelWidth*4);tinted.Freeze();var brush=new ImageBrush(tinted){Opacity=p.Opacity};brush.Freeze();var material=new DiffuseMaterial(brush);material.Freeze();p.Material=material;}}
 public static int Apply(FaceScene scene,JsonElement layers,GameResources resources,List<string> notes){
  if(layers.GetArrayLength()>128)throw new System.IO.InvalidDataException("Tint layer count limit");int applied=0;
  foreach(var part in scene.Parts.Where(p=>p.HeadPartType==1&&p.Diffuse!=null)){
   var image=part.Diffuse!;int w=image.PixelWidth,h=image.PixelHeight;byte[] pixels=Pixels(image);
   foreach(var layer in layers.EnumerateArray().OrderBy(l=>l.GetProperty("index").GetInt32())){
    uint color=layer.GetProperty("color").GetUInt32();double alpha=(color>>24)/255.0;if(alpha==0)continue;string name=layer.GetProperty("texture").GetString()??"";string? path=resources.Texture(name);if(path==null){resources.Missing("textures/"+name);continue;}
    try{var mask=DdsTexture.Load(path,true);var bytes=Pixels(mask);int mw=mask.PixelWidth,mh=mask.PixelHeight;double red=((color>>16)&255)/255.0,green=((color>>8)&255)/255.0,blue=(color&255)/255.0;bool skin=layer.GetProperty("index").GetInt32()==0;
     for(int y=0;y<h;y++)for(int x=0;x<w;x++){int i=(y*w+x)*4,m=((y*mh/h)*mw+x*mw/w)*4;double amount=alpha*(bytes[m+2]/255.0)*(bytes[m+3]/255.0);if(amount==0)continue;double luminance=(pixels[i]+pixels[i+1]+pixels[i+2])/3.0;double Target(int c,double channel)=>skin?pixels[i+c]*channel:luminance*channel;pixels[i]=(byte)Math.Clamp(Math.Round(pixels[i]*(1-amount)+Target(0,blue)*amount),0,255);pixels[i+1]=(byte)Math.Clamp(Math.Round(pixels[i+1]*(1-amount)+Target(1,green)*amount),0,255);pixels[i+2]=(byte)Math.Clamp(Math.Round(pixels[i+2]*(1-amount)+Target(2,red)*amount),0,255);}
     applied++;
    }catch(Exception e){notes.Add("Tint mask rejected: "+name+": "+e.Message);}
   }
   var composed=BitmapSource.Create(w,h,96,96,PixelFormats.Bgra32,null,pixels,w*4);composed.Freeze();var brush=new ImageBrush(composed){Opacity=part.Opacity,ViewportUnits=BrushMappingMode.Absolute,Viewport=new Rect(0,0,1,1),TileMode=TileMode.Tile,Stretch=Stretch.Fill};brush.Freeze();var material=new DiffuseMaterial(brush);material.Freeze();part.Material=material;
  }
  if(applied>0)notes.Add("Tint masks applied in index order: "+applied+" (WPF albedo approximation; Skyrim tint shader/lighting not reproduced).");return applied;
 }
 static byte[] Pixels(BitmapSource image){if(image.Format!=PixelFormats.Bgra32)image=new FormatConvertedBitmap(image,PixelFormats.Bgra32,null,0);var b=new byte[checked(image.PixelWidth*image.PixelHeight*4)];image.CopyPixels(b,image.PixelWidth*4,0);return b;}
}
