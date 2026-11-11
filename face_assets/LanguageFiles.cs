using System.Collections.ObjectModel;
using System.IO;
using System.Text.Json;
using System.Text.RegularExpressions;
namespace FaceAssets;

public sealed record LanguageChoice(string Id,string Name){public override string ToString()=>Name;}
public sealed record LanguagePack(string Id,string Name,IReadOnlyDictionary<string,string> Strings);

// A small shared file format. Discovery never changes the selected language.
public static class LanguageFiles {
 public const int MaximumFiles=64,MaximumBytes=1024*1024;
 public static IReadOnlyList<LanguagePack> Load(string directory){
  var found=new List<LanguagePack>();
  try{
   if(!Directory.Exists(directory))return [];
   var files=Directory.EnumerateFiles(directory,"*.json",SearchOption.TopDirectoryOnly).Take(MaximumFiles+1).ToArray();
   // Reject an overfull directory instead of choosing a filesystem-order subset.
   if(files.Length>MaximumFiles)return [];
   foreach(string file in files.OrderBy(x=>x,StringComparer.Ordinal))try{
    using var stream=new FileStream(file,FileMode.Open,FileAccess.Read,FileShare.Read);
    if(stream.Length is <2 or >MaximumBytes)continue;
    using var doc=JsonDocument.Parse(stream,new JsonDocumentOptions{MaxDepth=8});var root=doc.RootElement;
    if(root.ValueKind!=JsonValueKind.Object||Duplicates(root)||!root.TryGetProperty("schemaVersion",out var version)||!version.TryGetInt32(out int schema)||schema!=1)continue;
    if(!root.TryGetProperty("id",out var idValue)||idValue.ValueKind!=JsonValueKind.String||!root.TryGetProperty("name",out var labelValue)||labelValue.ValueKind!=JsonValueKind.String)continue;
    string id=idValue.GetString()!,label=labelValue.GetString()!;
    if(!Regex.IsMatch(id,@"\A[a-z][a-z0-9_-]{1,31}\z",RegexOptions.CultureInvariant)||id is "ko" or "en"||string.IsNullOrWhiteSpace(label)||label.Length>128||label.Any(char.IsControl))continue;
    if(!root.TryGetProperty("strings",out var strings)||strings.ValueKind!=JsonValueKind.Object||Duplicates(strings))continue;
    var values=new Dictionary<string,string>(StringComparer.Ordinal);
    foreach(var row in strings.EnumerateObject()){
     if(row.Name.Length is <1 or >512||row.Name.Contains('\0')||row.Value.ValueKind!=JsonValueKind.String)continue;
     string value=row.Value.GetString()!;if(string.IsNullOrWhiteSpace(value)||value.Length>16384||value.Contains('\0'))continue;values[row.Name]=value;
    }
    found.Add(new(id,label,new ReadOnlyDictionary<string,string>(values)));
   }catch(Exception e) when(e is IOException or UnauthorizedAccessException or JsonException or ArgumentException or InvalidOperationException){ }
  }catch(Exception e) when(e is IOException or UnauthorizedAccessException or ArgumentException or NotSupportedException){ }
  return found.GroupBy(p=>p.Id,StringComparer.Ordinal).Where(g=>g.Count()==1).Select(g=>g.Single()).OrderBy(p=>p.Id,StringComparer.Ordinal).ToArray();
 }
 static bool Duplicates(JsonElement obj){var names=new HashSet<string>(StringComparer.Ordinal);foreach(var p in obj.EnumerateObject())if(!names.Add(p.Name))return true;return false;}
}
