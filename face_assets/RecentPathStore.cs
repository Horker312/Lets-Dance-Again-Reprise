using System.IO;
using System.Text.Json;

namespace FaceAssets;

// Local editor preferences only: never written into a dance project or distributed export.
// Persist *directories*, not filenames, separately for each asset group.
public sealed class RecentPathStore {
 readonly string settingsFile;
 readonly object gate=new();
 Dictionary<string,string>? lastFolders;
 public RecentPathStore(string settingsFile){this.settingsFile=Path.GetFullPath(settingsFile);}
 void EnsureLoaded(){
  if(lastFolders!=null)return;
  lastFolders=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
  try {
   if(!File.Exists(settingsFile)||new FileInfo(settingsFile).Length>1024*1024)return;
   var saved=JsonSerializer.Deserialize<Dictionary<string,string>>(File.ReadAllText(settingsFile));
   if(saved==null)return;
   foreach(var (key,folder) in saved)if(!string.IsNullOrWhiteSpace(key)&&!string.IsNullOrWhiteSpace(folder)&&Directory.Exists(folder))lastFolders[key]=Path.GetFullPath(folder);
  }catch(Exception ex) when(ex is IOException or UnauthorizedAccessException or JsonException or ArgumentException or NotSupportedException) { /* Corrupt or inaccessible preferences are nonfatal. */ }
 }
 public string Resolve(string group,string? fallback=null){
  if(string.IsNullOrWhiteSpace(group))throw new ArgumentException("Missing asset group",nameof(group));
  lock(gate){
   EnsureLoaded();
   if(lastFolders!.TryGetValue(group,out var saved)&&Directory.Exists(saved))return saved;
  }
  if(!string.IsNullOrWhiteSpace(fallback)&&Directory.Exists(fallback))return Path.GetFullPath(fallback);
  foreach(var preferred in new[]{Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),AppContext.BaseDirectory})if(Directory.Exists(preferred))return preferred;
  return AppContext.BaseDirectory;
 }
 public void RecordFile(string group,string file){
  if(string.IsNullOrWhiteSpace(file))return;
  try{var parent=Path.GetDirectoryName(Path.GetFullPath(file));if(parent!=null)RecordDirectory(group,parent);}catch(Exception ex) when(ex is ArgumentException or IOException or NotSupportedException or UnauthorizedAccessException){}
 }
 public void RecordDirectory(string group,string folder){
  if(string.IsNullOrWhiteSpace(group)||string.IsNullOrWhiteSpace(folder))return;
  try {
   var full=Path.GetFullPath(folder);
   if(!Directory.Exists(full))return;
   lock(gate){
    EnsureLoaded();lastFolders![group]=full;
    try{
     var directory=Path.GetDirectoryName(settingsFile)!;Directory.CreateDirectory(directory);
     var tmp=settingsFile+"."+Guid.NewGuid().ToString("N")+".tmp";
     try {File.WriteAllText(tmp,JsonSerializer.Serialize(lastFolders,new JsonSerializerOptions{WriteIndented=true}));File.Move(tmp,settingsFile,true);}
     finally {if(File.Exists(tmp))File.Delete(tmp);}
    }catch(Exception ex) when(ex is IOException or UnauthorizedAccessException or ArgumentException or NotSupportedException) { /* Read-only profile: current session still remembers. */ }
   }
  }catch(Exception ex) when(ex is IOException or UnauthorizedAccessException or ArgumentException or NotSupportedException){}
 }
}
