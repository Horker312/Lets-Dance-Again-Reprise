using System.IO;
namespace DanceStudio;
// Cleanup is secondary to the operation result. Unique work paths are never reused.
public static class FileCleanup {
 public static bool DeleteFile(string path)=>Try(path,false);
 public static bool DeleteDirectory(string path)=>Try(path,true);
 static bool Try(string path,bool directory){try{if(directory){if(Directory.Exists(path))Directory.Delete(path,true);}else if(File.Exists(path))File.Delete(path);return true;}catch(IOException e){StartupDiagnostics.Error("Temporary cleanup: "+path,e);return false;}catch(UnauthorizedAccessException e){StartupDiagnostics.Error("Temporary cleanup: "+path,e);return false;}}
}
