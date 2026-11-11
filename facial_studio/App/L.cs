using System.IO;
using System.Text.Json;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Runtime.CompilerServices;
using FaceAssets;
namespace FacialStudio;

// Text resources are separate from authored preset/clip data. Never translate input fields.
public static class L {
 static readonly Dictionary<string,string> en=Read();
 static IReadOnlyList<LanguagePack>? packs;static Dictionary<string,string> reverse=new(StringComparer.Ordinal);static IReadOnlyList<LanguageChoice> languages=[];
 public static string SettingsPath{get;set;}=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"FacialStudio","language.json");
 public static string LangDirectory{get;set;}=Path.Combine(AppContext.BaseDirectory,"lang");
 static string language="ko";public static string Language{get{Ensure();return language;}}
 public static IReadOnlyList<LanguageChoice> Languages{get{Ensure();return languages;}}
 static Dictionary<string,string> Read(){using var s=typeof(L).Assembly.GetManifestResourceStream("FacialStudio.Locale.en.json")!;return JsonSerializer.Deserialize<Dictionary<string,string>>(s)!;}
 static void Ensure(){if(packs!=null)return;packs=LanguageFiles.Load(LangDirectory).Select(p=>p with{Strings=p.Strings.Where(v=>en.ContainsKey(v.Key)&&Compatible(v.Key,v.Value)).ToDictionary(v=>v.Key,v=>v.Value,StringComparer.Ordinal)}).ToArray();languages=new[]{new LanguageChoice("ko","한국어"),new LanguageChoice("en","English")}.Concat(packs.Select(p=>new LanguageChoice(p.Id,p.Name))).ToArray();var aliases=en.Select(p=>(Text:p.Value,Source:p.Key)).Concat(packs.SelectMany(p=>p.Strings.Select(v=>(Text:v.Value,Source:v.Key))));reverse=aliases.GroupBy(p=>p.Text,StringComparer.Ordinal).Where(g=>g.Select(p=>p.Source).Distinct(StringComparer.Ordinal).Count()==1).ToDictionary(g=>g.Key,g=>g.First().Source,StringComparer.Ordinal);language=ReadLanguage();}
 static bool Compatible(string source,string translated){if(!Regex.IsMatch(source,@"\{\d"))return true;try{var a=Regex.Matches(source,@"(?<!\{)\{(\d+)(?:[,}:])").Select(m=>m.Groups[1].Value).Order().ToArray();var b=Regex.Matches(translated,@"(?<!\{)\{(\d+)(?:[,}:])").Select(m=>m.Groups[1].Value).Order().ToArray();_ = CompositeFormat.Parse(translated);return a.SequenceEqual(b);}catch(FormatException){return false;}}
 static string ReadLanguage(){try{if(new FileInfo(SettingsPath).Length>4096)return "ko";using var d=JsonDocument.Parse(File.ReadAllText(SettingsPath));string id=d.RootElement.GetProperty("language").GetString()??"ko";return languages.Any(l=>l.Id==id)?id:"ko";}catch{return "ko";}}
 // Explicit reload is useful for isolated checks; normal app startup discovers once.
 public static void ReloadLanguages(){packs=null;Ensure();}
 public static void Set(string lang){Ensure();if(!languages.Any(l=>l.Id==lang))throw new ArgumentException("Unsupported language");Directory.CreateDirectory(Path.GetDirectoryName(SettingsPath)!);var temp=SettingsPath+Guid.NewGuid()+".tmp";try{File.WriteAllText(temp,JsonSerializer.Serialize(new{language=lang}));File.Move(temp,SettingsPath,true);language=lang;}finally{if(File.Exists(temp))File.Delete(temp);}}
 sealed record SourceText(string Source);static readonly ConditionalWeakTable<string,SourceText> translatedOrigins=new();
 static string Canonical(string text)=>translatedOrigins.TryGetValue(text,out var origin)?origin.Source:en.ContainsKey(text)?text:reverse.GetValueOrDefault(text,text);
 public static string T(string text){Ensure();string source=Canonical(text);string result=language=="ko"?source:language!="en"&&packs!.FirstOrDefault(p=>p.Id==language)?.Strings.TryGetValue(source,out var translated)==true?translated:en.GetValueOrDefault(source,source);if(!ReferenceEquals(source,result))translatedOrigins.GetValue(result,_=>new(source));return result;}
 public static string F(string text,params object[] args)=>string.Format(System.Globalization.CultureInfo.InvariantCulture,T(text),args);
 sealed record Applied(string Source,string Last);
 static readonly ConditionalWeakTable<DependencyObject,Dictionary<DependencyProperty,Applied>> originals=new();
 static void Translate(DependencyObject owner,DependencyProperty property,string current){if(System.Windows.Data.BindingOperations.IsDataBound(owner,property))return;var fields=originals.GetOrCreateValue(owner);string source=fields.TryGetValue(property,out var old)&&old.Last==current?old.Source:Canonical(current);string next=T(source);fields[property]=new(source,next);if(next!=current)owner.SetCurrentValue(property,next);}
 public static void Apply(DependencyObject root){var seen=new HashSet<DependencyObject>();void Visit(DependencyObject o){if(!seen.Add(o))return;if(o is TextBox||o is ListBoxItem)return;
  if(o is TextBlock t)Translate(t,TextBlock.TextProperty,t.Text);
  if(o is HeaderedContentControl h&&h.Header is string hs)Translate(h,HeaderedContentControl.HeaderProperty,hs);
  if(o is ContentControl c&&c.Content is string cs)Translate(c,ContentControl.ContentProperty,cs);
  if(o is FrameworkElement f&&f.ToolTip is string tip)Translate(f,FrameworkElement.ToolTipProperty,tip);
  foreach(var child in LogicalTreeHelper.GetChildren(o).OfType<DependencyObject>())Visit(child);
  if(o is Visual||o is System.Windows.Media.Media3D.Visual3D)for(int i=0;i<VisualTreeHelper.GetChildrenCount(o);i++)Visit(VisualTreeHelper.GetChild(o,i));
 }Visit(root);}
}
