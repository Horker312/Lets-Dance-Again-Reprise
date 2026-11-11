using System.IO;
using System.Text.Json;
using FaceAssets;
namespace FacialStudio;
public static class ResourceRootPreference {
 static string FilePath=>Path.Combine(Path.GetDirectoryName(L.SettingsPath)!,"resource-root.json");
 public static string? Read(){try{if(new FileInfo(FilePath).Length>16384)return null;using var doc=JsonDocument.Parse(File.ReadAllText(FilePath));return doc.RootElement.GetProperty("root").GetString();}catch(Exception e)when(ImportFiles.Expected(e)){return null;}}
 // Persist only a path and observable execution evidence, never parsed scene connections.
 static string Scope(string root){var observed=new GameResources(root).UsvfsObserved;if(!observed)return "direct";string list=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Skyrim Special Edition","plugins.txt");return File.Exists(list)&&new FileInfo(list).Length<1024*1024?"usvfs|"+GameResources.BackingPath(list)+"|"+ImportFiles.Digest(list):"usvfs-unproven";}
 public static bool MatchesEnvironment(string root){try{if(new FileInfo(FilePath).Length>16384)return false;using var doc=JsonDocument.Parse(File.ReadAllText(FilePath));string current=Scope(root);return current!="usvfs-unproven"&&doc.RootElement.TryGetProperty("scope",out var scope)&&scope.GetString()==current;}catch(Exception e)when(ImportFiles.Expected(e)){return false;}}
 public static string Validate(string root){var full=Path.GetFullPath(root);if(!Directory.Exists(full))throw new DirectoryNotFoundException(full);if(!File.Exists(Path.Combine(full,"Skyrim.esm"))&&File.Exists(Path.Combine(full,"Data","Skyrim.esm")))full=Path.Combine(full,"Data");if(!Directory.Exists(Path.Combine(full,"meshes"))&&!Directory.Exists(Path.Combine(full,"textures"))&&!Directory.EnumerateFiles(full).Take(4096).Any(f=>Path.GetExtension(f).ToLowerInvariant() is ".bsa" or ".esm" or ".esp" or ".esl"))throw new InvalidDataException("No resource root structure found");return full;}
 public static void Save(string root){Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);var tmp=FilePath+Guid.NewGuid()+".tmp";try{File.WriteAllText(tmp,JsonSerializer.Serialize(new{root,scope=Scope(root)}));File.Move(tmp,FilePath,true);}finally{if(File.Exists(tmp))File.Delete(tmp);}}
}
