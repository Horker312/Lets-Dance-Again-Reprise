using System.Text.Json;
namespace FacialPresets;
public sealed class Morph {
 public string Backend {get;set;}="MFEE";
 public bool Enabled {get;set;}=true;
 public string Id {get;set;}="";
 public string Name {get;set;}="";
 public double Strength {get;set;}=25;
}
public sealed class Channel {
 public int Priority {get;set;}
 public List<Morph> Morphs {get;set;}=[];
}
public sealed class Preset {
 public int SchemaVersion {get;set;}=1;
 public Guid Id {get;set;}=Guid.NewGuid();
 public string Name {get;set;}="새 표정";
 public string Profile {get;set;}="vanilla";
 public Dictionary<string,Channel> Channels {get;set;}=new(){["emotion"]=new(),["eyes"]=new(),["mouth"]=new()};
 public List<string> RequiredResources {get;set;}=["MFEE API v7 + compatible target-race TRI (verify in game)"];
}
public static class PresetFile {
 static readonly JsonSerializerOptions Options=new(){WriteIndented=true,PropertyNamingPolicy=JsonNamingPolicy.CamelCase,PropertyNameCaseInsensitive=true};
 public static void Validate(Preset p){
  if(p.SchemaVersion!=1||p.Id==Guid.Empty||string.IsNullOrWhiteSpace(p.Name)||p.Name.Length>200||p.Profile is not ("vanilla" or "erin")||p.Channels==null||p.RequiredResources==null)throw new InvalidDataException("지원하지 않거나 잘못된 프리셋입니다.");
  foreach(var (key,c) in p.Channels){if(key is not ("emotion" or "eyes" or "mouth")||c?.Morphs==null||c.Morphs.Count>512)throw new InvalidDataException("잘못된 채널입니다.");
   var ids=new HashSet<string>();var stableIds=new HashSet<string>();foreach(var m in c.Morphs){if(m==null||m.Backend is not ("MFG" or "MFEE")||string.IsNullOrWhiteSpace(m.Id)||System.Text.Encoding.UTF8.GetByteCount(m.Id)>256||string.IsNullOrWhiteSpace(m.Name)||System.Text.Encoding.UTF8.GetByteCount(m.Name)>512||m.Name.Contains('\0')||m.Id.Contains('\0')||!double.IsFinite(m.Strength)||m.Strength<0||m.Strength>100||!ids.Add(m.Backend+":"+(m.Backend=="MFEE"?m.Name.ToLowerInvariant():m.Id))||!stableIds.Add(m.Backend+":"+m.Id))throw new InvalidDataException("잘못되거나 중복된 모프입니다.");
    if(m.Backend=="MFG"){var parts=m.Id.Split(':');if(parts.Length!=2||!int.TryParse(parts[1],out int n)||parts[1]!=n.ToString(System.Globalization.CultureInfo.InvariantCulture)||n<0||!(parts[0] switch{"expression"=>n<17,"modifier"=>n<17,"phoneme"=>n<16,_=>false}))throw new InvalidDataException("잘못된 MFG ID입니다.");}
   }
  }
 }
 public static Preset Load(string path){if(new FileInfo(path).Length>2*1024*1024)throw new InvalidDataException("파일이 너무 큽니다.");var text=File.ReadAllText(path);using var json=JsonDocument.Parse(text);foreach(var key in new[]{"schemaVersion","id","name","profile","channels"})if(json.RootElement.ValueKind!=JsonValueKind.Object||!json.RootElement.TryGetProperty(key,out _))throw new InvalidDataException("필수 필드가 없는 프리셋입니다.");JsonShape.Preset(json.RootElement);var p=JsonSerializer.Deserialize<Preset>(text,Options)??throw new InvalidDataException("빈 파일입니다.");Validate(p);return p;}
 public static void Save(string path,Preset p){Validate(p);var temp=path+"."+Guid.NewGuid()+".tmp";try{var text=JsonSerializer.Serialize(p,Options);if(System.Text.Encoding.UTF8.GetByteCount(text)>2*1024*1024)throw new InvalidDataException("프리셋 파일이 너무 큽니다.");File.WriteAllText(temp,text);File.Move(temp,path,true);}finally{if(File.Exists(temp))File.Delete(temp);}}
 public static Preset Clone(Preset p){var copy=JsonSerializer.Deserialize<Preset>(JsonSerializer.Serialize(p,Options),Options)!;copy.Id=Guid.NewGuid();copy.Name+=" 복사";return copy;}
}
