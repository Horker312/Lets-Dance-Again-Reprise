using System.IO;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
namespace FacialStudio;

// Text resources are separate from authored preset/clip data. Never translate input fields.
public static class L {
 static readonly Dictionary<string,string> en=Read();
 static readonly Dictionary<string,string> reverse=en.GroupBy(p=>p.Value).ToDictionary(g=>g.Key,g=>g.First().Key);
 public static string SettingsPath{get;set;}=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"FacialStudio","language.json");
 static string language=ReadLanguage();public static string Language=>language;
 static Dictionary<string,string> Read(){using var s=typeof(L).Assembly.GetManifestResourceStream("FacialStudio.Locale.en.json")!;return JsonSerializer.Deserialize<Dictionary<string,string>>(s)!;}
 static string ReadLanguage(){try{using var d=JsonDocument.Parse(File.ReadAllText(SettingsPath));return d.RootElement.GetProperty("language").GetString()=="en"?"en":"ko";}catch{return "ko";}}
 public static void Set(string lang){if(lang is not ("ko" or "en"))throw new ArgumentException();Directory.CreateDirectory(Path.GetDirectoryName(SettingsPath)!);var temp=SettingsPath+Guid.NewGuid()+".tmp";try{File.WriteAllText(temp,JsonSerializer.Serialize(new{language=lang}));File.Move(temp,SettingsPath,true);language=lang;}finally{if(File.Exists(temp))File.Delete(temp);}}
 public static string T(string text)=>language=="en"?en.GetValueOrDefault(text,text):reverse.GetValueOrDefault(text,text);
 public static string F(string text,params object[] args)=>string.Format(System.Globalization.CultureInfo.InvariantCulture,T(text),args);
 public static void Apply(DependencyObject root){var seen=new HashSet<DependencyObject>();void Visit(DependencyObject o){if(!seen.Add(o))return;if(o is TextBox||o is ListBoxItem)return;
  if(o is TextBlock t&&T(t.Text)!=t.Text)t.SetCurrentValue(TextBlock.TextProperty,T(t.Text));
  if(o is HeaderedContentControl h&&h.Header is string hs)h.SetCurrentValue(HeaderedContentControl.HeaderProperty,T(hs));
  if(o is ContentControl c&&c.Content is string cs)c.SetCurrentValue(ContentControl.ContentProperty,T(cs));
  if(o is FrameworkElement f&&f.ToolTip is string tip)f.SetCurrentValue(FrameworkElement.ToolTipProperty,T(tip));
  foreach(var child in LogicalTreeHelper.GetChildren(o).OfType<DependencyObject>())Visit(child);
  if(o is Visual||o is System.Windows.Media.Media3D.Visual3D)for(int i=0;i<VisualTreeHelper.GetChildrenCount(o);i++)Visit(VisualTreeHelper.GetChild(o,i));
 }Visit(root);}
}
