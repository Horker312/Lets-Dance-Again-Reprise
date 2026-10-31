using System.Text.Json;
namespace FacialPresets;

public sealed class MfeeEntry {
 public string Id{get;set;}="";public string Name{get;set;}="";public string Category{get;set;}="";
 public bool ApiRegistered{get;set;}public bool ActorReadable{get;set;}
}
public sealed class MfeeCatalog {
 public int SchemaVersion{get;set;}=1;public int ApiVersion{get;set;}=7;
 public List<MfeeEntry> Entries{get;set;}=[];
 // Explicit, externally verified mappings only; stock MFEE has no verified A/I/U/E/O set.
 public bool VowelMappingsVerified{get;set;}
 public Dictionary<string,string> Vowels{get;set;}=new();
 public MfeeEntry? Vowel(string vowel)=>VowelMappingsVerified&&Vowels.TryGetValue(vowel,out var id)?Entries.FirstOrDefault(e=>e.Id==id&&e.ApiRegistered&&e.ActorReadable):null;
 public static MfeeCatalog Load(string path){if(new FileInfo(path).Length>2*1024*1024)throw new InvalidDataException("MFEE catalog exceeds 2 MiB.");var c=JsonSerializer.Deserialize<MfeeCatalog>(File.ReadAllText(path),new JsonSerializerOptions{PropertyNameCaseInsensitive=true})??throw new InvalidDataException("Empty MFEE catalog.");if(c.SchemaVersion!=1||c.ApiVersion!=7||c.Entries==null||c.Vowels==null||c.Entries.Count>4096)throw new InvalidDataException("Unsupported MFEE catalog.");var ids=new HashSet<string>();var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);foreach(var e in c.Entries){if(e==null||string.IsNullOrWhiteSpace(e.Id)||e.Id.Length>256||string.IsNullOrWhiteSpace(e.Name)||e.Name.Length>512||e.Name.Contains('\0')||e.Id.Contains('\0')||e.Category==null||e.Category.Length>128||!ids.Add(e.Id)||!names.Add(e.Name))throw new InvalidDataException("Invalid/duplicate MFEE catalog entry.");}foreach(var (v,id) in c.Vowels)if(v is not ("a" or "i" or "u" or "e" or "o")||!ids.Contains(id))throw new InvalidDataException("Invalid vowel mapping.");return c;}
 public static MfeeCatalog Documented(){var c=new MfeeCatalog();foreach(var (category,names) in new[]{("Mood","MoodLaugh MoodFlabbergasted MoodTemptation MoodSulky"),("Eyes","EyesIn EyesOut EyesSurprise EyesShock"),("Mouth","MouthLaugh MouthTemptation MouthSmacklips MouthFlabbergasted MouthCry MouthCat MouthClench MouthClenchLip MouthPucker MouthUpperUp MouthBow"),("Face","FaceInflateCheeks")})foreach(var n in names.Split(' '))c.Entries.Add(new(){Id=n.ToLowerInvariant(),Name=n,Category=category});return c;}
}
public static class MfeePolicy {
 public static IEnumerable<Morph> Legacy(Preset p)=>p.Channels.Values.SelectMany(c=>c.Morphs).Where(m=>m.Backend!="MFEE");
 public static IEnumerable<Morph> Legacy(IEnumerable<FacialSegment> tracks)=>tracks.SelectMany(t=>t.Clip==null?Legacy(t.Preset):t.Clip.Keys.SelectMany(k=>Legacy(k.State)));
 static List<string> Resources(IEnumerable<string> input)=>input.Distinct().ToList();
 public static Preset PlaybackCopy(Preset p){var copy=FacialTimeline.Copy(p);foreach(var c in copy.Channels.Values)c.Morphs.RemoveAll(m=>!m.Enabled||(m.Backend=="MFG"&&m.Id.StartsWith("modifier:")&&int.Parse(m.Id.Split(':')[1])>=14));copy.RequiredResources=Resources(copy.RequiredResources);return copy;}
 public static FacialClip PlaybackCopy(FacialClip c){var copy=JsonSerializer.Deserialize<FacialClip>(JsonSerializer.Serialize(c))!;foreach(var k in copy.Keys)k.State=PlaybackCopy(k.State);copy.RequiredResources=Resources(copy.RequiredResources.Concat(copy.Keys.SelectMany(k=>k.State.RequiredResources)));return copy;}
}
