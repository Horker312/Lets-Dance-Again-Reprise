using System.IO;using System.Text.Json;
namespace DanceStudio;
public static class LibraryLayout {
 public static string SettingsPath{get;set;}=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"DanceStudio","LibraryLayout.json");
 public static double Load(){try{if(new FileInfo(SettingsPath).Length>32768)return 220;using var doc=JsonDocument.Parse(File.ReadAllText(SettingsPath));double n=doc.RootElement.GetProperty("width").GetDouble();return double.IsFinite(n)?Math.Clamp(n,170,480):220;}catch{return 220;}}
 public static void Save(double n){Directory.CreateDirectory(Path.GetDirectoryName(SettingsPath)!);SongProject.AtomicWrite(SettingsPath,JsonSerializer.Serialize(new{width=Math.Clamp(n,170,480)}));}
}
