using System.Globalization;
using System.IO;
using System.Text.Json;
namespace DanceStudio;
public static class UiText {
 static readonly Dictionary<string,string> ko=Load("ko"),en=Load("en");
 public static string SettingsPath {get;set;}=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"DanceStudio","language.json");
 static string language=ReadLanguage();
 static Dictionary<string,string> Load(string lang){using var s=typeof(UiText).Assembly.GetManifestResourceStream("DanceStudio.Locale."+lang+".json")??throw new InvalidDataException("Language resource missing: "+lang);return JsonSerializer.Deserialize<Dictionary<string,string>>(s)??throw new InvalidDataException("Invalid language resource");}
 static string ReadLanguage(){try{using var d=JsonDocument.Parse(File.ReadAllText(SettingsPath));return d.RootElement.GetProperty("language").GetString()=="en"?"en":"ko";}catch{return "ko";}}
 public static string Language=>language;
 public static IReadOnlyDictionary<string,string> Korean=>ko;
 public static IReadOnlyDictionary<string,string> English=>en;
 public static string T(string key){var map=language=="en"?en:ko;return map.TryGetValue(key,out var value)?value:ko.TryGetValue(key,out value)?value:key;}
 public static string Error(string message){if(language!="en")return message;foreach(var item in ko)if(item.Value==message&&en.TryGetValue(item.Key,out var translated))return translated;return message;}
 public static string F(string key,params object?[] args)=>string.Format(CultureInfo.InvariantCulture,T(key),args);
 public static void SetLanguage(string value){if(value!="ko"&&value!="en")throw new ArgumentException("Unsupported language");Directory.CreateDirectory(Path.GetDirectoryName(SettingsPath)!);var temp=SettingsPath+"."+Guid.NewGuid().ToString("N")+".tmp";try{File.WriteAllText(temp,JsonSerializer.Serialize(new{language=value}));File.Move(temp,SettingsPath,true);language=value;}finally{if(File.Exists(temp))File.Delete(temp);}}
}
