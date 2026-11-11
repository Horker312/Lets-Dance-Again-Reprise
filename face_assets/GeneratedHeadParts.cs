using System.IO;
using System.Text;
using System.Text.RegularExpressions;
namespace FaceAssets;

// Erin controller creates HDPT records at runtime: these records do not exist in ErinRace.esm.
// Resolve ONLY a literal creation-log ID + declared INI mesh + unique existing texture.
// Enumeration indexes are never reverse engineered. A historical preset may use another generation.
public static class GeneratedHeadParts {
 public static List<HeadPartRecords.Record> Resolve(string id,GameResources r,List<string> notes,string? exportedUnique=null){
  var result=new List<HeadPartRecords.Record>();if(!id.Split('|')[0].Equals("ErinRace.esm",StringComparison.OrdinalIgnoreCase))return result;
  string? log=exportedUnique==null?r.Find("SKSE/Plugins/ErinRace.log"):null;if(exportedUnique==null&&(log==null||new FileInfo(log).Length>16*1024*1024))return result;
  string text=exportedUnique!=null?"":File.ReadAllText(log!);int session=text.LastIndexOf("MuErinRace ",StringComparison.Ordinal);if(session>=0)text=text[session..];uint local=Convert.ToUInt32(id.Split('|')[1],16);
  var found=Regex.Matches(text,@"Created new \w+ Form : ([0-9a-fA-F]+) ([^\r\n]+)",RegexOptions.CultureInvariant,TimeSpan.FromSeconds(1)).Where(m=>uint.TryParse(m.Groups[1].Value,System.Globalization.NumberStyles.HexNumber,null,out var n)&&(n&0xffffff)==local).Select(m=>m.Groups[2].Value.Trim()).Distinct().ToList();
  if(exportedUnique!=null)found=[exportedUnique];if(found.Count!=1){if(found.Count>1)notes.Add("Conflicting runtime HeadPart log IDs: "+id);return result;}
  string unique=found[0];var parsed=Regex.Match(unique,@"^Erin\w+([MF])_([^/\\]+\.ini)_(.+)\*([0-9]+)$",RegexOptions.CultureInvariant,TimeSpan.FromSeconds(1));if(!parsed.Success)return result;
  string iniName=parsed.Groups[2].Value;if(Path.GetFileName(iniName)!=iniName)return result;string? ini=r.Find("SKSE/Plugins/ErinRace/Headpart/"+iniName);if(ini==null||new FileInfo(ini).Length>1024*1024)return result;
  var fields=new Dictionary<string,List<string>>(StringComparer.OrdinalIgnoreCase);string section="";foreach(string line in File.ReadLines(ini)){string t=line.Split('#')[0].Trim();if(t.StartsWith('[')){section=t.Trim('[',']');continue;}int at=t.IndexOf('=');if(at<0)continue;string key=section+"/"+t[..at].Trim();if(!fields.TryGetValue(key,out var values))fields[key]=values=[];values.Add(t[(at+1)..].Trim());}
  string Get(string key)=>fields.GetValueOrDefault(key)?.FirstOrDefault()??"";
  uint type=Get("/Type").ToLowerInvariant() switch {"misc"=>0,"face"=>1,"eyes"=>2,"hair"=>3,"facialhair"=>4,"scar"=>5,"eyebrows"=>6,_=>uint.MaxValue};if(type==uint.MaxValue)return result;
  string sex=parsed.Groups[1].Value,root=sex=="F"?"actors/Erin/":"actors/Erin/Male/";
  string? Texture(string folder,string token){if(folder.Length==0)return null;string rel="textures/actors/"+folder;bool sub=Get("Textures/SubFolder").Equals("true",StringComparison.OrdinalIgnoreCase);var candidates=r.EnumerateResources(rel,".dds",sub).ToList();var matches=candidates.Where(p=>(Path.GetFileName(Path.GetDirectoryName(p.Replace('/',Path.DirectorySeparatorChar))!)+"_"+Path.GetFileNameWithoutExtension(p.Replace('/',Path.DirectorySeparatorChar))).Equals(token,StringComparison.OrdinalIgnoreCase)).ToList();return matches.Count==1?matches[0]:null;}
  HeadPartRecords.Record Build(string section,int index,string recordId,uint partType){string Value(string suffix)=>fields.GetValueOrDefault(section+"/"+sex+"_"+suffix)?.ElementAtOrDefault(index)??"";var rec=new HeadPartRecords.Record{Generated=true,Id=recordId,Source="ErinRace.esm",Kind="HDPT"};void S(string tag,string value)=>rec.Fields.Add((tag,Encoding.UTF8.GetBytes(value+"\0")));S("EDID",unique);S("MODL",root+Value("Meshes"));rec.Fields.Add(("PNAM",BitConverter.GetBytes(partType)));rec.Fields.Add(("DATA",[(byte)(sex=="F"?4:2)]));foreach(var (n,key) in new[]{(0,"Races"),(1,"Expression"),(2,"Chargen")})if(Value(key).Length>0){rec.Fields.Add(("NAM0",BitConverter.GetBytes(n)));S("NAM1",root+Value(key));}return rec;}
  if(Get("Meshes/"+sex+"_Meshes").Length==0)return result;string identity=exportedUnique==null?id:"exported:"+unique;var main=Build("Meshes",0,identity,type);
  string explicitTexture=Get("Textures/"+(sex=="F"?"Female":"Male")+"_Diffuse");string? texture=explicitTexture.Length>0?"textures/actors/"+explicitTexture:Texture(Get("Textures/Folder"),parsed.Groups[3].Value);
  if(texture==null)notes.Add("Runtime HeadPart texture cannot be uniquely resolved: "+unique);main.Fields.Add(("DSGN",[1]));if(texture!=null)main.Fields.Add(("DSDF",Encoding.UTF8.GetBytes(texture+"\0")));result.Add(main);
  int extra=fields.GetValueOrDefault("ExtraMeshes/"+sex+"_Meshes")?.Count??0;if(extra>16)throw new InvalidDataException("Generated HeadPart extras budget");for(int i=0;i<extra;i++){var rec=Build("ExtraMeshes",i,identity+"#"+i,0);string extraDiffuse=fields.GetValueOrDefault("ExtraTextures/"+(sex=="F"?"Female":"Male")+"_Diffuse")?.ElementAtOrDefault(i)??"";if(extraDiffuse.Length>0)rec.Fields.Add(("DSDF",Encoding.UTF8.GetBytes("textures/actors/"+extraDiffuse+"\0")));result.Add(rec);}
  notes.Add((exportedUnique==null?"Runtime HeadPart resolved from visible Erin controller log + INI (saved-generation identity is NOT proven): ":"Exported HeadPart unique name + INI: ")+identity+" -> "+unique+" / "+texture);return result;
 }
}
