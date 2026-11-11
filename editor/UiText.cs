using System.Globalization;
using System.IO;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Text;
using FaceAssets;
namespace DanceStudio;
public static class UiText {
 static readonly Dictionary<string,string> ko=Load("ko"),en=Load("en");
 static readonly IReadOnlyList<LanguagePack> packs=LanguageFiles.Load(Path.Combine(AppContext.BaseDirectory,"lang"));
 static readonly IReadOnlyList<LanguageChoice> choices=new[]{new LanguageChoice("ko","한국어"),new LanguageChoice("en","English")}.Concat(packs.Select(p=>new LanguageChoice(p.Id,p.Name))).ToArray();
 static readonly Dictionary<string,Dictionary<string,string>> external=packs.ToDictionary(p=>p.Id,p=>p.Strings.Where(kv=>ko.ContainsKey(kv.Key)&&CompatibleFormat(en.GetValueOrDefault(kv.Key,ko[kv.Key]),kv.Value)).ToDictionary(kv=>kv.Key,kv=>kv.Value),StringComparer.Ordinal);
 public static string SettingsPath {get;set;}=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"DanceStudio","language.json");
 static string language=ReadLanguage();
 static Dictionary<string,string> Load(string lang){using var s=typeof(UiText).Assembly.GetManifestResourceStream("DanceStudio.Locale."+lang+".json")??throw new InvalidDataException("Language resource missing: "+lang);return JsonSerializer.Deserialize<Dictionary<string,string>>(s)??throw new InvalidDataException("Invalid language resource");}
 static bool CompatibleFormat(string source,string value){try{string[] Tokens(string text)=>Regex.Matches(text,@"(?<!\{)\{\d+(?=[:,}])").Select(m=>m.Value).Order().ToArray();if(!Tokens(source).SequenceEqual(Tokens(value)))return false;if(Tokens(source).Length>0)CompositeFormat.Parse(value);return true;}catch{return false;}}
 static string ReadLanguage(){try{if(new FileInfo(SettingsPath).Length>4096)return "ko";using var d=JsonDocument.Parse(File.ReadAllText(SettingsPath));string? value=d.RootElement.GetProperty("language").GetString();return choices.Any(c=>c.Id==value)?value!:"ko";}catch{return "ko";}}
 public static string Language=>language;
 public static IReadOnlyList<LanguageChoice> Languages=>choices;
 public static IReadOnlyDictionary<string,string> Korean=>ko;
 public static IReadOnlyDictionary<string,string> English=>en;
 public static string T(string key){if(language=="ko")return ko.GetValueOrDefault(key,key);if(external.TryGetValue(language,out var map)&&map.TryGetValue(key,out var text))return text;return en.GetValueOrDefault(key,ko.GetValueOrDefault(key,key));}
 public static string Error(string message){foreach(var item in ko)if(item.Value==message)return T(item.Key);foreach(var item in en)if(item.Value==message)return T(item.Key);return message;}
 public static string F(string key,params object?[] args)=>string.Format(CultureInfo.InvariantCulture,T(key),args);
 internal static void RestoreSelection(string value){if(choices.Any(c=>c.Id==value))language=value;}
 public static void SetLanguage(string value){if(!choices.Any(c=>c.Id==value))throw new ArgumentException("Unsupported language");Directory.CreateDirectory(Path.GetDirectoryName(SettingsPath)!);var temp=SettingsPath+"."+Guid.NewGuid().ToString("N")+".tmp";try{File.WriteAllText(temp,JsonSerializer.Serialize(new{language=value}));File.Move(temp,SettingsPath,true);language=value;}finally{if(File.Exists(temp))File.Delete(temp);}}
}
