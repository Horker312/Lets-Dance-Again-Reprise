using System.IO;
using System.Buffers.Binary;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using BCnEncoder.Decoder;
using BCnEncoder.Shared;
namespace FaceAssets;

public static class DdsTexture {
 static readonly Dictionary<string,WeakReference<BitmapSource>> cache=new();
 public static BitmapSource Load(string path,bool alpha=true,byte threshold=0){
  var info=new FileInfo(path);if(info.Length<128||info.Length>128*1024*1024)throw new InvalidDataException("DDS size must be 128 B–128 MiB.");
  string key=Path.GetFullPath(path)+"|"+info.Length+"|"+info.LastWriteTimeUtc.Ticks+"|"+alpha+"|"+threshold;lock(cache)if(cache.TryGetValue(key,out var weak)&&weak.TryGetTarget(out var cached))return cached;
  using var file=File.OpenRead(path);if(file.Length<128||file.Length>128*1024*1024)throw new InvalidDataException("DDS size exceeds limits.");byte[] bytes=new byte[checked((int)file.Length)];file.ReadExactly(bytes);uint U(int at)=>BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(at,4));
  if(U(0)!=0x20534444||U(4)!=124||U(76)!=32)throw new InvalidDataException("Invalid DDS header.");
  int h=checked((int)U(12)),w=checked((int)U(16));if(w<1||h<1||w>4096||h>4096||U(24)>1||(U(112)&0xFE00)!=0)throw new InvalidDataException("Only 2D DDS up to 4096×4096 is supported; arrays/cubes/volume are excluded.");
  uint code=U(84);int start=128;CompressionFormat format;
  if(code==0x30315844){if(bytes.Length<148||U(132)!=3||U(140)!=1||(U(136)&4)!=0)throw new InvalidDataException("Unsupported DDS DX10 resource.");start=148;format=U(128) switch {71 or 72=>CompressionFormat.Bc1WithAlpha,74 or 75=>CompressionFormat.Bc2,77 or 78=>CompressionFormat.Bc3,80=>CompressionFormat.Bc4,83=>CompressionFormat.Bc5,98 or 99=>CompressionFormat.Bc7,28 or 29=>CompressionFormat.Rgba,87 or 91=>CompressionFormat.Bgra,_=>throw new InvalidDataException("Unsupported DXGI DDS format (BC6/HDR/signed formats excluded).")};}
  else format=code switch {0x31545844=>CompressionFormat.Bc1WithAlpha,0x33545844=>CompressionFormat.Bc2,0x35545844=>CompressionFormat.Bc3,0x31495441 or 0x55344342=>CompressionFormat.Bc4,0x32495441 or 0x55354342=>CompressionFormat.Bc5,0 when U(88)==32&&U(92)==0x00ff0000&&U(96)==0x0000ff00&&U(100)==0x000000ff=>CompressionFormat.Bgra,0 when U(88)==32&&U(92)==0xff&&U(96)==0xff00&&U(100)==0xff0000=>CompressionFormat.Rgba,_=>throw new InvalidDataException("Unsupported DDS format.")};
  int length=format is CompressionFormat.Rgba or CompressionFormat.Bgra?checked(w*h*4):checked(((w+3)/4)*((h+3)/4)*(format is CompressionFormat.Bc1WithAlpha or CompressionFormat.Bc4?8:16));
  if(bytes.Length-start<length)throw new InvalidDataException("Truncated DDS pixels.");
  var decoder=new BcDecoder();decoder.Options.IsParallel=false;var pixels=decoder.DecodeRaw(bytes.AsSpan(start,length).ToArray(),w,h,format);var bgra=new byte[checked(w*h*4)];
  for(int i=0;i<pixels.Length;i++){bgra[i*4]=pixels[i].b;bgra[i*4+1]=pixels[i].g;bgra[i*4+2]=pixels[i].r;bgra[i*4+3]=alpha&&!(code==0&&U(104)==0)?(pixels[i].a<threshold?(byte)0:pixels[i].a):(byte)255;}
  var image=BitmapSource.Create(w,h,96,96,PixelFormats.Bgra32,null,bgra,w*4);image.Freeze();lock(cache){if(cache.Count>=32)cache.Clear();cache[key]=new(image);}return image;
 }
}
