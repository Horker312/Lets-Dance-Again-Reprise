using System.IO;
using System.Reflection.PortableExecutable;
using System.Buffers.Binary;
using System.Text;
namespace FaceAssets;
// SKSE64 GameData.h: FaceMorphList has 19 {uint type,pad,char* lower,char* upper}
// entries; FacePresetList has Nose/Brow/Eyes/Lip prefixes. Read the installed PE,
// never live process memory. No relocation addresses or race-specific names are stored.
public sealed class EngineFaceMappings {
 public sealed record Option(uint Type,string Lower,string Upper);
 public Option[] Options{get;private init;}=[];public string[] Presets{get;private init;}=[];
 public string Evidence{get;private init;}="";
 static readonly object gate=new();static readonly Dictionary<string,EngineFaceMappings?> cache=[];
 public static EngineFaceMappings? Load(GameResources resources){
  if(resources.EnginePath is not string file){resources.Notes.Add("Default face mapping unavailable: installed Skyrim executable not identified.");return null;}
  resources.Context.Cancellation.ThrowIfCancellationRequested();string digest=ImportFiles.Digest(file);lock(gate)if(cache.TryGetValue(digest,out var old)){if(old!=null)resources.Notes.Add(old.Evidence);return old;}
  EngineFaceMappings? result=null;try{using var f=File.OpenRead(file);if(f.Length>128L*1024*1024)throw new InvalidDataException("Executable size budget");using var pe=new PEReader(f,PEStreamOptions.PrefetchEntireImage);var h=pe.PEHeaders;if(h.PEHeader?.Magic!=PEMagic.PE32Plus)throw new NotSupportedException("Only PE32+ table layout is supported");byte[] b=pe.GetEntireImage().GetContent().ToArray();ulong imageBase=h.PEHeader.ImageBase;
   int Offset(ulong va){if(va<imageBase||va-imageBase>uint.MaxValue)return -1;long rva=(long)(va-imageBase);foreach(var section in h.SectionHeaders)if(rva>=section.VirtualAddress&&rva-section.VirtualAddress<section.SizeOfRawData){long at=section.PointerToRawData+rva-section.VirtualAddress;return at>=0&&at<b.Length?(int)at:-1;}return -1;}
   string? Text(ulong va){int at=Offset(va);if(at<0)return null;int end=at;while(end<b.Length&&end-at<=128&&b[end]!=0){if(b[end]<32||b[end]>126)return null;end++;}return end<b.Length&&end-at<=128?Encoding.ASCII.GetString(b,at,end-at):null;}
   var options=new List<Option[]>();var presets=new List<string[]>();
   foreach(var section in h.SectionHeaders.Where(s=>(s.SectionCharacteristics&SectionCharacteristics.MemExecute)==0)){
    int end=(int)Math.Min(b.Length,(long)section.PointerToRawData+section.SizeOfRawData);for(int at=section.PointerToRawData;at+24<=end;at+=8){if((at&0xffff)==0)resources.Context.Cancellation.ThrowIfCancellationRequested();ulong lower=BinaryPrimitives.ReadUInt64LittleEndian(b.AsSpan(at+8));if(Text(lower)=="NoseShort"&&Text(BinaryPrimitives.ReadUInt64LittleEndian(b.AsSpan(at+16)))=="NoseLong"&&at+19*24<=end){var rows=new List<Option>();for(int i=0;i<19;i++){int p=at+i*24;uint type=BinaryPrimitives.ReadUInt32LittleEndian(b.AsSpan(p));string? l=Text(BinaryPrimitives.ReadUInt64LittleEndian(b.AsSpan(p+8))),u=Text(BinaryPrimitives.ReadUInt64LittleEndian(b.AsSpan(p+16)));if(type is not (1 or 2)||string.IsNullOrEmpty(l)||u==null)break;rows.Add(new(type,l,u));}if(rows.Count==19)options.Add(rows.ToArray());}
     if(Text(BinaryPrimitives.ReadUInt64LittleEndian(b.AsSpan(at)))=="NoseType"&&at+4*16<=end){var names=Enumerable.Range(0,4).Select(i=>Text(BinaryPrimitives.ReadUInt64LittleEndian(b.AsSpan(at+i*16)))).ToArray();if(names.SequenceEqual(new[]{"NoseType","BrowType","EyesType","LipType"}))presets.Add(names.Select(x=>x!).ToArray());}
    }
   }
   if(options.Count!=1||presets.Count!=1)throw new NotSupportedException("Installed engine tables are missing or ambiguous");result=new(){Options=options[0],Presets=presets[0],Evidence="Installed PE FaceMorphList/FacePresetList; SHA256="+digest};resources.Notes.Add(result.Evidence);
  }catch(OperationCanceledException){throw;}catch(Exception e)when(ImportFiles.Expected(e)||e is BadImageFormatException){resources.Notes.Add("Default mapping rejected: "+e.Message);}
  lock(gate){if(cache.Count>=4)cache.Clear();cache[digest]=result;}return result;
 }
}
