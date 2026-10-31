using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;
namespace DanceStudio;
public static class ModLocation {
 const string Marker="DanceStudioData.esl";
 static string Cache=>Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"DanceStudio","ModLocation.txt");
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern uint GetFinalPathNameByHandleW(SafeFileHandle file,StringBuilder path,uint length,uint flags);
 public static bool IsModRoot(string path)=>File.Exists(Path.Combine(path,Marker))&&File.Exists(Path.Combine(path,"SKSE","Plugins","DanceStudio.dll"));
 public static string? ResolveRoot(string candidate){
  if(!IsModRoot(candidate))return null;
  // MO2 can present a virtual Data path. Resolve the ESL's open handle to its
  // actual backing file, so exporting goes to this mod instead of overwrite.
  using var file=File.OpenRead(Path.Combine(candidate,Marker));var buffer=new StringBuilder(32768);uint length=GetFinalPathNameByHandleW(file.SafeFileHandle,buffer,(uint)buffer.Capacity,0);
  if(length>0&&length<buffer.Capacity){string value=buffer.ToString();if(value.StartsWith(@"\\?\UNC\"))value=@"\\"+value[8..];else if(value.StartsWith(@"\\?\"))value=value[4..];string? root=Path.GetDirectoryName(value);if(root!=null&&IsModRoot(root))return Path.GetFullPath(root);}
  return null; // Never silently export through an unresolved virtual Data path.
 }
 public static string? Discover(string baseDirectory,string? cachePath=null){
  for(var dir=new DirectoryInfo(Path.GetFullPath(baseDirectory));dir!=null;dir=dir.Parent){try{if(ResolveRoot(dir.FullName)is string root)return root;}catch(IOException){}catch(UnauthorizedAccessException){}}
  foreach(var candidate in new[]{Path.Combine(Environment.CurrentDirectory,"Data"),Environment.CurrentDirectory})try{if(ResolveRoot(candidate)is string virtualRoot)return virtualRoot;}catch(IOException){}catch(UnauthorizedAccessException){}
  try{string cache=cachePath??Cache;if(File.Exists(cache)&&new FileInfo(cache).Length<32768)return ResolveRoot(File.ReadAllText(cache).Trim());}catch(IOException){}catch(UnauthorizedAccessException){}catch(ArgumentException){}
  for(var dir=new DirectoryInfo(Path.GetFullPath(baseDirectory));dir!=null;dir=dir.Parent)if(dir.Name.Equals("Tools",StringComparison.OrdinalIgnoreCase)&&dir.Parent?.Parent is DirectoryInfo container)try{var matches=container.EnumerateDirectories().Take(513).Select(d=>ResolveRoot(d.FullName)).Where(x=>x!=null).Distinct(StringComparer.OrdinalIgnoreCase).ToArray();if(matches.Length==1)return matches[0];}catch(IOException){}catch(UnauthorizedAccessException){}
  return null;
 }
 public static string Select(string directory){var root=ResolveRoot(directory)??throw new InvalidDataException(UiText.T("t0253"));Directory.CreateDirectory(Path.GetDirectoryName(Cache)!);SongProject.AtomicWrite(Cache,root);return root;}
 public static string Library(string root)=>Path.Combine(root,GameExporter.LibraryRelative);
}
