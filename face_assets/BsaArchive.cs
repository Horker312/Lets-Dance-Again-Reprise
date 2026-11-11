using System.IO;
using System.IO.Compression;
using System.Buffers.Binary;
using System.Numerics;
using System.Text;
namespace FaceAssets;

// Read only, on-demand TES5 BSA 104 (zlib) / SSE 105 (LZ4 frame).
// Indexes names, never extracts an entire archive. No third-party decoder is bundled.
public sealed class BsaArchive {
 public sealed record Entry(string Name,uint Size,uint Offset);
 public string Path{get;} public uint Version{get;} public uint Flags{get;}public uint ContentTypes{get;}
 public IReadOnlyDictionary<string,Entry> Entries=>entries;
 readonly Dictionary<string,Entry> entries=new(StringComparer.OrdinalIgnoreCase);
 readonly CancellationToken cancellation;
 const int MaxEntry=128*1024*1024;
 public static string Normalize(string name){name=name.Replace('\\','/').TrimEnd('/');if(System.IO.Path.IsPathRooted(name)||name.Split('/').Any(p=>p is ".." or "."||p.Contains(':')||p.Contains('\0')))throw new InvalidDataException("Unsafe BSA name");return name;}
 public static uint ReadContentTypes(string path){using var f=File.OpenRead(path);using var r=new BinaryReader(f);if(f.Length<36||r.ReadUInt32()!=0x00415342)return 0;uint v=r.ReadUInt32();if(v is not (104 or 105))return 0;f.Position=32;return r.ReadUInt32();}
 public BsaArchive(string path,CancellationToken token=default){cancellation=token;token.ThrowIfCancellationRequested();
  Path=System.IO.Path.GetFullPath(path);using var f=File.OpenRead(Path);using var r=new BinaryReader(f);
  if(f.Length<36||f.Length>8L*1024*1024*1024||r.ReadUInt32()!=0x00415342)throw new InvalidDataException("Invalid BSA header/size");
  Version=r.ReadUInt32();if(Version is not (104 or 105))throw new InvalidDataException("Only BSA 104/105 supported");uint offset=r.ReadUInt32();Flags=r.ReadUInt32();uint folders=r.ReadUInt32(),files=r.ReadUInt32(),folderBytes=r.ReadUInt32(),nameBytes=r.ReadUInt32();ContentTypes=r.ReadUInt32();
  if((Flags&3)!=3||folders>100000||files>500000||folderBytes>16*1024*1024||nameBytes>32*1024*1024||offset<36||offset+(long)folders*(Version==105?24:16)>f.Length)throw new InvalidDataException("BSA names/count/offset budget");
  f.Position=offset;var counts=new List<int>();for(int i=0;i<folders;i++){r.ReadUInt64();uint n=r.ReadUInt32();if(n>files)throw new InvalidDataException("BSA folder count");counts.Add((int)n);if(Version==105){r.ReadUInt32();r.ReadUInt64();}else r.ReadUInt32();}
  if(counts.Sum(n=>(long)n)!=files)throw new InvalidDataException("BSA file count mismatch");var raw=new List<(string folder,uint size,uint at)>();long folderTotal=0;
  foreach(int n in counts){cancellation.ThrowIfCancellationRequested();int len=r.ReadByte();if(len==0)throw new InvalidDataException("Empty BSA folder name");var b=r.ReadBytes(len);if(b.Length!=len||b[^1]!=0)throw new InvalidDataException("BSA folder terminator");folderTotal+=len;string folder=Normalize(new UTF8Encoding(false,true).GetString(b[..^1]));for(int i=0;i<n;i++){r.ReadUInt64();uint size=r.ReadUInt32(),at=r.ReadUInt32();uint length=size&0x3fffffff;if((size&0x80000000)!=0||length>MaxEntry||at+(long)length>f.Length)throw new InvalidDataException("BSA entry bounds");raw.Add((folder,size,at));}}
  if(folderTotal!=folderBytes||nameBytes>f.Length-f.Position)throw new InvalidDataException("BSA name block bounds");byte[] names=r.ReadBytes((int)nameBytes);int start=0;foreach(var item in raw){cancellation.ThrowIfCancellationRequested();int stop=Array.IndexOf(names,(byte)0,start);if(stop<start||stop-start>1024)throw new InvalidDataException("BSA filename bounds");string name=Normalize(item.folder+"/"+new UTF8Encoding(false,true).GetString(names,start,stop-start));if(!entries.TryAdd(name,new(name,item.size,item.at)))throw new InvalidDataException("Duplicate BSA resource: "+name);start=stop+1;}if(start!=names.Length)throw new InvalidDataException("BSA trailing filename data");
 }
 public byte[] Read(Entry entry){cancellation.ThrowIfCancellationRequested();
  if(!entries.TryGetValue(entry.Name,out var own)||own!=entry)throw new InvalidDataException("Foreign BSA entry");using var f=File.OpenRead(Path);f.Position=entry.Offset;int length=(int)(entry.Size&0x3fffffff);byte[] blob=new byte[length];f.ReadExactly(blob);int p=0;
  if((Flags&0x100)!=0){if(blob.Length<1||blob[0]+1>blob.Length)throw new InvalidDataException("BSA embedded name bounds");p=blob[0]+1;}
  bool compressed=((Flags&4)!=0)^((entry.Size&0x40000000)!=0);if(!compressed)return blob[p..];
  if(blob.Length-p<4)throw new InvalidDataException("BSA compressed size missing");int n=BinaryPrimitives.ReadInt32LittleEndian(blob.AsSpan(p));p+=4;if(n<1||n>MaxEntry)throw new InvalidDataException("BSA expanded entry budget");
  if(Version==105)return DecodeLz4Frame(blob.AsSpan(p),n);
  using var z=new ZLibStream(new MemoryStream(blob,p,blob.Length-p),CompressionMode.Decompress);byte[] result=new byte[n];z.ReadExactly(result);if(z.ReadByte()!=-1)throw new InvalidDataException("BSA zlib expanded size mismatch");return result;
 }
 public static byte[] DecodeLz4Frame(ReadOnlySpan<byte> input,int expected){
  if(expected<1||expected>MaxEntry||input.Length<11||BinaryPrimitives.ReadUInt32LittleEndian(input)!=0x184d2204)throw new InvalidDataException("Invalid LZ4 frame");
  byte flags=input[4],bd=input[5];if((flags>>6)!=1||(flags&3)!=0||(bd&0x8f)!=0)throw new InvalidDataException("Unsupported LZ4 frame flags/dictionary");int blockMax=((bd>>4)&7) switch {4=>65536,5=>262144,6=>1048576,7=>4194304,_=>throw new InvalidDataException("LZ4 block maximum")};int p=6;
  if((flags&8)!=0){if(input.Length-p<9||BinaryPrimitives.ReadUInt64LittleEndian(input[p..])!=(ulong)expected)throw new InvalidDataException("LZ4 content size mismatch");p+=8;}
  if(p>=input.Length||input[p]!=(byte)(XxHash(input.Slice(4,p-4))>>8))throw new InvalidDataException("LZ4 header checksum");p++;var result=new byte[expected];int output=0;
  while(true){if(input.Length-p<4)throw new InvalidDataException("LZ4 block header");uint raw=BinaryPrimitives.ReadUInt32LittleEndian(input[p..]);p+=4;if(raw==0)break;int n=(int)(raw&0x7fffffff);if(n<1||n>blockMax||n>input.Length-p)throw new InvalidDataException("LZ4 block bounds");var block=input.Slice(p,n);p+=n;int before=output;
   if((raw&0x80000000)!=0){if(n>result.Length-output)throw new InvalidDataException("LZ4 output overflow");block.CopyTo(result.AsSpan(output));output+=n;}
   else{int at=0;int minimum=(flags&32)!=0?output:0;while(at<block.Length){byte token=block[at++];int literal=token>>4;if(literal==15){byte more;do{if(at>=block.Length)throw new InvalidDataException("LZ4 literal length");more=block[at++];literal=checked(literal+more);}while(more==255);}if(literal>block.Length-at||literal>result.Length-output)throw new InvalidDataException("LZ4 literal bounds");block.Slice(at,literal).CopyTo(result.AsSpan(output));at+=literal;output+=literal;if(at==block.Length)break;if(block.Length-at<2)throw new InvalidDataException("LZ4 match offset");int offset=BinaryPrimitives.ReadUInt16LittleEndian(block[at..]);at+=2;if(offset==0||offset>output-minimum)throw new InvalidDataException("LZ4 invalid match");int match=(token&15)+4;if((token&15)==15){byte more;do{if(at>=block.Length)throw new InvalidDataException("LZ4 match length");more=block[at++];match=checked(match+more);}while(more==255);}if(match>result.Length-output)throw new InvalidDataException("LZ4 match overflow");for(int i=0;i<match;i++)result[output+i]=result[output-offset+i];output+=match;}}
   if(output-before>blockMax)throw new InvalidDataException("LZ4 expanded block budget");if((flags&16)!=0){if(input.Length-p<4||BinaryPrimitives.ReadUInt32LittleEndian(input[p..])!=XxHash(block))throw new InvalidDataException("LZ4 block checksum");p+=4;}
  }
  if(output!=expected)throw new InvalidDataException("LZ4 expanded size mismatch");if((flags&4)!=0){if(input.Length-p<4||BinaryPrimitives.ReadUInt32LittleEndian(input[p..])!=XxHash(result))throw new InvalidDataException("LZ4 content checksum");p+=4;}if(p!=input.Length)throw new InvalidDataException("LZ4 trailing data");return result;
 }
 static uint XxHash(ReadOnlySpan<byte> data){unchecked{const uint a=2654435761,b=2246822519,c=3266489917,d=668265263,e=374761393;int p=0;uint h;
  uint Round(uint x,uint n)=>BitOperations.RotateLeft(x+n*b,13)*a;
  if(data.Length>=16){uint v1=a+b,v2=b,v3=0,v4=0-a;while(p<=data.Length-16){v1=Round(v1,BinaryPrimitives.ReadUInt32LittleEndian(data[p..]));v2=Round(v2,BinaryPrimitives.ReadUInt32LittleEndian(data[(p+4)..]));v3=Round(v3,BinaryPrimitives.ReadUInt32LittleEndian(data[(p+8)..]));v4=Round(v4,BinaryPrimitives.ReadUInt32LittleEndian(data[(p+12)..]));p+=16;}h=BitOperations.RotateLeft(v1,1)+BitOperations.RotateLeft(v2,7)+BitOperations.RotateLeft(v3,12)+BitOperations.RotateLeft(v4,18);}else h=e;
  h+=(uint)data.Length;while(p<=data.Length-4){h=BitOperations.RotateLeft(h+BinaryPrimitives.ReadUInt32LittleEndian(data[p..])*c,17)*d;p+=4;}while(p<data.Length)h=BitOperations.RotateLeft(h+data[p++]*e,11)*a;h^=h>>15;h*=b;h^=h>>13;h*=c;return h^(h>>16);
 }}
}
