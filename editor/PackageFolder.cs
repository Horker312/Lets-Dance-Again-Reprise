using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;
namespace DanceStudio;
public static class PackageFolder {
 static string Cache=>Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"DanceStudio","PackageFolder.txt");
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern SafeFileHandle CreateFileW(string name,uint access,uint share,IntPtr security,uint creation,uint flags,IntPtr template);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern uint GetFinalPathNameByHandleW(SafeFileHandle file,StringBuilder path,uint length,uint flags);
 public static string? Existing(string path){try{if(!Directory.Exists(path))return null;using var handle=CreateFileW(Path.GetFullPath(path),0,7,IntPtr.Zero,3,0x02000000,IntPtr.Zero);if(handle.IsInvalid)return null;var text=new StringBuilder(32768);uint n=GetFinalPathNameByHandleW(handle,text,(uint)text.Capacity,0);if(n==0||n>=text.Capacity)return null;string result=text.ToString();if(result.StartsWith(@"\\?\UNC\"))result=@"\\"+result[8..];else if(result.StartsWith(@"\\?\"))result=result[4..];return Path.GetFullPath(result);}catch(IOException){return null;}catch(UnauthorizedAccessException){return null;}}
 public static string? Last(){try{return File.Exists(Cache)&&new FileInfo(Cache).Length<32768?Existing(File.ReadAllText(Cache).Trim()):null;}catch{return null;}}
 public static void Remember(string path){var physical=Existing(path)??throw new DirectoryNotFoundException(path);Directory.CreateDirectory(Path.GetDirectoryName(Cache)!);SongProject.AtomicWrite(Cache,physical);}
}
