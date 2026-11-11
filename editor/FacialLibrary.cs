using System.IO;using System.Security.Cryptography;
using FacialPresets;
namespace DanceStudio;
public sealed class FacialLibrary {
 public string Root{get;}public List<string> Warnings{get;}=[];
 public sealed record Item(string Path,Preset? Preset,FacialClip? Clip){public string Name=>Clip?.Name??Preset!.Name;public override string ToString()=>Name+"\n"+(Clip==null?"Preset":$"Clip · {Clip.Duration:0.###} s")+" · "+string.Join(" / ",Clip==null?Preset!.Channels.Where(c=>c.Value.Morphs.Count>0).Select(c=>c.Key):Clip.Keys.SelectMany(k=>k.State.Channels.Where(c=>c.Value.Morphs.Count>0).Select(c=>c.Key)).Distinct());}
 public FacialLibrary(string? root=null){Root=root??System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"DanceStudio","FacialLibrary");}
 public List<Item> Read(){Warnings.Clear();var list=new List<Item>();if(!Directory.Exists(Root))return list;foreach(var f in Directory.EnumerateFiles(Root,"*.json"))try{list.Add(Load(f));}catch(Exception e){Warnings.Add(System.IO.Path.GetFileName(f)+": "+e.Message);}return list.OrderBy(i=>i.Name,StringComparer.CurrentCultureIgnoreCase).ToList();}
 static Item Load(string f)=>f.EndsWith(".facialclip.json",StringComparison.OrdinalIgnoreCase)?new(f,null,FacialClipFile.Load(f)):new(f,PresetFile.Load(f),null);
 public Item Import(string path,bool clip){var input=clip?new Item(path,null,FacialClipFile.Load(path)):new Item(path,PresetFile.Load(path),null);Directory.CreateDirectory(Root);string temp=System.IO.Path.Combine(Root,Guid.NewGuid()+".tmp");try{if(clip)FacialClipFile.Save(temp,input.Clip!);else PresetFile.Save(temp,input.Preset!);string hash=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(temp)));string target=System.IO.Path.Combine(Root,hash+(clip?".facialclip.json":".facialpreset.json"));if(!File.Exists(target))File.Move(temp,target);return Load(target);}finally{if(File.Exists(temp))File.Delete(temp);}}
 public void Remove(Item i){if(!System.IO.Path.GetFullPath(i.Path).StartsWith(System.IO.Path.GetFullPath(Root).TrimEnd(System.IO.Path.DirectorySeparatorChar)+System.IO.Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))throw new IOException("Not a library item");File.Delete(i.Path);}
}
