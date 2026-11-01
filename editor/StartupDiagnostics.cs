using System.IO;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Text;
namespace DanceStudio;
public static class StartupDiagnostics {
 static readonly object gate=new();static readonly StringBuilder history=new();
 public static string? LogPath{get;private set;}
 public static void Start(string[] args){foreach(var folder in new[]{Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"DanceStudio","Logs"),Path.Combine(Path.GetTempPath(),"DanceStudio-Logs")})try{Directory.CreateDirectory(folder);LogPath=Path.Combine(folder,"startup-"+DateTime.Now.ToString("yyyyMMdd-HHmmss")+"-"+Environment.ProcessId+".log");File.WriteAllText(LogPath,"");break;}catch{ }
  Record("Startup",$"Dance Studio {Brand.Version}\nEXE: {Environment.ProcessPath}\nBase: {AppContext.BaseDirectory}\nCWD: {Environment.CurrentDirectory}\nWindows: {RuntimeInformation.OSDescription}\n.NET: {RuntimeInformation.FrameworkDescription}\nArgument count: {args.Length} (values omitted)\nUSVFS module observed: {Usvfs()} (absence does not prove standalone launch)");
 }
 static string Usvfs(){try{return Process.GetCurrentProcess().Modules.Cast<ProcessModule>().Any(m=>m.ModuleName.StartsWith("usvfs",StringComparison.OrdinalIgnoreCase)).ToString();}catch{return "Unknown";}}
 public static void Record(string kind,string message){try{lock(gate){if(history.Length>64000)history.Remove(0,32000);var line=DateTime.Now.ToString("O")+" "+kind+": "+message+"\n";history.Append(line);if(LogPath!=null)try{File.AppendAllText(LogPath,line);}catch{LogPath=null;}}}catch{}}
 public static void Error(string stage,Exception ex)=>Record(stage,$"{ex.GetType().Name} HRESULT=0x{ex.HResult:X8}: {ex.Message}\n{ex.StackTrace}");
 public static string Report(){lock(gate)return history.ToString()+"\nLog: "+(LogPath??"unavailable")+"\nDiscovery:\n"+ModLocation.LastReport+"\n"+UiText.T("diagSecurity");}
}
