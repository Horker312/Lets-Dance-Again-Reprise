using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Diagnostics;
using System.ComponentModel;
using Microsoft.Win32.SafeHandles;
namespace DanceStudio;
public static class ModLocation {
 const string Marker="DanceStudioData.esl";
 static string Cache=>Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"DanceStudio","ModLocation.txt");
 public static string LastReport{get;private set;}="Not searched";
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern uint GetFinalPathNameByHandleW(SafeFileHandle file,StringBuilder path,uint length,uint flags);
 static bool ExistsChecked(string path){try{return (File.GetAttributes(path)&FileAttributes.Directory)==0;}catch(FileNotFoundException){return false;}catch(DirectoryNotFoundException){return false;}}
 public static bool IsModRoot(string path)=>ExistsChecked(Path.Combine(path,Marker))&&ExistsChecked(Path.Combine(path,"SKSE","Plugins","DanceStudio.dll"));
 static string Physical(string path){using var file=File.Open(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete);var b=new StringBuilder(32768);uint n=GetFinalPathNameByHandleW(file.SafeFileHandle,b,(uint)b.Capacity,0);if(n==0)throw new IOException("Physical path resolution failed: "+path,new Win32Exception(Marshal.GetLastWin32Error()));if(n>=b.Capacity)throw new IOException("Physical path exceeds buffer: "+path);string s=b.ToString();if(s.StartsWith(@"\\?\UNC\"))s=@"\\"+s[8..];else if(s.StartsWith(@"\\?\"))s=s[4..];return Path.GetFullPath(s);}
 public static string? ResolveRoot(string candidate){candidate=Path.GetFullPath(candidate);if(!IsModRoot(candidate))return null;var esl=Physical(Path.Combine(candidate,Marker));var dll=Physical(Path.Combine(candidate,"SKSE","Plugins","DanceStudio.dll"));var root=Path.GetDirectoryName(esl)!;if(!dll.Equals(Path.Combine(root,"SKSE","Plugins","DanceStudio.dll"),StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("Mixed physical ESL/DLL installation: "+esl+" / "+dll);if(new DirectoryInfo(root).Name.Equals("overwrite",StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("MO2 overwrite is not an installation root.");return root;}
 public static string Version(string root){try{return FileVersionInfo.GetVersionInfo(Path.Combine(root,"SKSE","Plugins","DanceStudio.dll")).FileVersion??"Unknown";}catch{return "Unknown";}}
 public static string? Discover(string baseDirectory,string? cachePath=null)=>DiscoverCore(baseDirectory,cachePath,Environment.CurrentDirectory);
 public static string? DiscoverCore(string baseDirectory,string? cachePath,string workingDirectory){
 var lines=new List<string>();var matches=new HashSet<string>(StringComparer.OrdinalIgnoreCase);var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);bool incomplete=false;var watch=Stopwatch.StartNew();
 void Check(string p){try{if(!seen.Add(Path.GetFullPath(p)))return;if(ResolveRoot(p)is string found){matches.Add(found);lines.Add("Candidate: "+found+" | DLL "+Version(found));}}catch(Exception e)when(e is IOException or UnauthorizedAccessException or ArgumentException or NotSupportedException){incomplete=true;lines.Add(e.GetType().Name+": "+p+" | "+e.Message);}}
 var containers=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
 foreach(var start in new[]{baseDirectory,workingDirectory})try{for(var d=new DirectoryInfo(Path.GetFullPath(start));d!=null;d=d.Parent){Check(d.FullName);if(d.Name.Equals("mods",StringComparison.OrdinalIgnoreCase))containers.Add(d.FullName);if(d.Name.Equals("Tools",StringComparison.OrdinalIgnoreCase)&&d.Parent?.Parent is {} c)containers.Add(c.FullName);if(File.Exists(Path.Combine(d.FullName,"ModOrganizer.exe"))||File.Exists(Path.Combine(d.FullName,"ModOrganizer.ini")))containers.Add(Path.Combine(d.FullName,"mods"));}}catch(Exception e){incomplete=true;lines.Add("Base path: "+e.GetType().Name);}
 Check(Path.Combine(workingDirectory,"Data"));
 string? cached=null;try{var cache=cachePath??Cache;if(File.Exists(cache)&&new FileInfo(cache).Length<32768){var p=File.ReadAllText(cache).Trim();if(p.Length>0){cached=ResolveRoot(p);lines.Add(cached==null?"Invalid cache; continuing discovery":"Validated explicit cached choice: "+cached);}}}catch(Exception e){lines.Add("Cache ignored: "+e.GetType().Name+" "+e.Message);}
 foreach(var folder in containers)try{if(!Directory.Exists(folder))continue;foreach(var d in Directory.EnumerateDirectories(folder)){if(watch.Elapsed>TimeSpan.FromSeconds(8)){incomplete=true;lines.Add("Search incomplete: time budget reached in "+folder);break;}try{if((File.GetAttributes(d)&FileAttributes.ReparsePoint)!=0){incomplete=true;lines.Add("Skipped reparse directory: "+d);continue;}Check(d);}catch(Exception e){incomplete=true;lines.Add("Directory access: "+e.GetType().Name+" "+d);}}}catch(Exception e){incomplete=true;lines.Add("Enumeration incomplete: "+folder+" "+e.GetType().Name);}
 string? result=cached??(!incomplete&&matches.Count==1?matches.Single():null);lines.Add("Result: "+(result??(matches.Count>1?"Ambiguous candidates":incomplete?"Incomplete search":"Not found")));LastReport=string.Join("\n",lines);StartupDiagnostics.Record("Discovery",LastReport);return result;
 }
 public static string Select(string directory){var root=ResolveRoot(directory)??throw new InvalidDataException(UiText.T("t0253"));Directory.CreateDirectory(Path.GetDirectoryName(Cache)!);SongProject.AtomicWrite(Cache,root);return root;}
 public static string Library(string root)=>Path.Combine(root,GameExporter.LibraryRelative);
}
