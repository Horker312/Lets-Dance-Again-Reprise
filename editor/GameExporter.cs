using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
namespace DanceStudio;
public static class GameExporter {
 public const int DancePriority=int.MaxValue;
 public const string LibraryRelative=@"meshes\actors\character\animations\OpenAnimationReplacer\Dance Studio";
 public static uint Token(string id){var b=SHA256.HashData(Encoding.UTF8.GetBytes(id));return 1+((uint)b[0]|(uint)b[1]<<8|(uint)b[2]<<16)%16777215;}
 public static string FolderName(string title,uint token){
  var cleaned=new string(title.Trim().Select(c=>Path.GetInvalidFileNameChars().Contains(c)||char.IsControl(c)?'_':c).ToArray()).TrimEnd(' ','.');
  var elements=System.Globalization.StringInfo.GetTextElementEnumerator(cleaned);var label=new StringBuilder();int count=0;
  while(elements.MoveNext()&&count++<32){string element=elements.GetTextElement();if(label.Length+element.Length>48)break;label.Append(element);}
  return "DS_"+token+" - "+(label.Length>0?label.ToString():UiText.T("t0031"));
 }
 public static string Export(string parent,SongProject p,MotionClip? dance,MotionClip? camera,WaveAudio audio,Rig rig,bool newPackage=false,Func<ExportReview,PreviousOutputAction>? review=null){
  PackageUpdates.RequireGameClosed();var original=p;p=JsonSerializer.Deserialize<SongProject>(JsonSerializer.Serialize(p,SongProject.Json),SongProject.Json)!;
  if(dance==null&&camera==null)throw new InvalidDataException(UiText.T("t0032"));ProjectRules.Validate(p,true);PoseEdits.Validate(p);if(dance!=null)foreach(var frame in dance.Samples)foreach(var pose in frame)if(!float.IsFinite(pose.T.X)||!float.IsFinite(pose.T.Y)||!float.IsFinite(pose.T.Z)||!float.IsFinite(pose.R.X)||!float.IsFinite(pose.R.Y)||!float.IsFinite(pose.R.Z)||!float.IsFinite(pose.R.W)||!float.IsFinite(pose.S.X)||!float.IsFinite(pose.S.Y)||!float.IsFinite(pose.S.Z)||pose.T.LengthSquared()>1e10f||pose.S.LengthSquared()>30000)throw new InvalidDataException(UiText.T("t0033"));
  if(camera!=null&&p.CameraBone<0){var proposal=CameraAnalysis.Analyze(camera,rig);p.CameraBone=proposal.Bone??throw new InvalidDataException(UiText.T("t0034"));}
  string packageId=newPackage?Guid.NewGuid().ToString("N"):p.PackageId(dance!=null,camera!=null);
  parent=Path.GetFullPath(parent);Directory.CreateDirectory(parent);PackageUpdates.SafeTree(parent+Path.DirectorySeparatorChar+".check",parent);string stage=Exporter.Export(parent,p,dance,camera,audio,rig,true);string clean=Path.Combine(stage,"runtime");Directory.CreateDirectory(clean);
  try{var inventory=PackageUpdates.Inventory(parent);var usedTokens=inventory.Tokens;string revision;uint folderToken;int attempts=0;do{revision=Guid.NewGuid().ToString("N");folderToken=Token(packageId+"|"+revision);if(++attempts>32)throw new IOException("Cannot allocate a unique output token");}while(usedTokens.Contains(folderToken));uint token=dance!=null?folderToken:0;string submod=dance!=null?"DS_"+token:"";
   void Move(string from,string to){File.Move(Path.Combine(stage,"tracks",from),Path.Combine(clean,to));}
   if(File.Exists(Path.Combine(stage,"tracks/facial.timeline.json")))Move("facial.timeline.json","facial.timeline.json");
   Move("music.wav","music.wav");if(camera!=null)Move("camera.samples.json","camera.samples.json");
   if(dance!=null){Move("dance.hkx","mt_idle.hkx");Directory.CreateDirectory(Path.Combine(clean,"male"));File.Copy(Path.Combine(clean,"mt_idle.hkx"),Path.Combine(clean,"male/mt_idle.hkx"));Directory.CreateDirectory(Path.Combine(clean,"female"));File.Copy(Path.Combine(clean,"mt_idle.hkx"),Path.Combine(clean,"female/mt_idle.hkx"));object[] conditions=[new Dictionary<string,object>{{"condition","IsActorBase"},{"requiredVersion","1.0.0.0"},{"Actor base",new{pluginName="Skyrim.esm",formID="7"}}},new Dictionary<string,object>{{"condition","CompareValues"},{"requiredVersion","1.0.0.0"},{"Value A",new{form=new{pluginName="DanceStudioData.esl",formID="800"}}},{"Comparison","=="},{"Value B",new{value=token}}}];File.WriteAllText(Path.Combine(clean,"config.json"),JsonSerializer.Serialize(new{name=submod,description=p.Title,priority=DancePriority,interruptible=true,replaceOnEcho=true,hasCustomBlendTimeOnInterrupt=true,blendTimeOnInterrupt=.15,conditions},SongProject.Json));}
   if(new FileInfo(Path.Combine(clean,"music.wav")).Length>384L*1024*1024)throw new InvalidDataException(UiText.T("t0035"));
   if(inventory.Times.GetValueOrDefault(packageId)>=253402300799999L)throw new IOException("Cannot generate a newer package revision timestamp");
   var hashes=Directory.GetFiles(clean,"*",SearchOption.AllDirectories).Where(x=>!x.EndsWith("config.json")).ToDictionary(x=>Path.GetRelativePath(clean,x).Replace('\\','/'),x=>Convert.ToHexStringLower(SHA256.HashData(File.ReadAllBytes(x))));
   var manifest=new Dictionary<string,object>{{"schemaVersion",4},{"id",packageId},{"projectId",p.Id},{"packageKind",SongProject.PackageKind(dance!=null,camera!=null)},{"title",p.Title},{"token",token},{"oarSubmod",submod},{"exportId",revision},{"exportedAt",Math.Max(DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),inventory.Times.GetValueOrDefault(packageId)+1)},{"duration",p.ExportOut-p.ExportIn},{"music","music.wav"},{"hashes",hashes}};if(File.Exists(Path.Combine(clean,"facial.timeline.json")))manifest["facialTimeline"]="facial.timeline.json";if(dance!=null){manifest["dance"]="mt_idle.hkx";manifest["danceFemale"]="female/mt_idle.hkx";manifest["danceMale"]="male/mt_idle.hkx";}if(camera!=null)manifest["cameraSamples"]="camera.samples.json";File.WriteAllText(Path.Combine(clean,"song.json"),JsonSerializer.Serialize(manifest,SongProject.Json));
   string dest=Path.Combine(Path.GetFullPath(parent),FolderName(p.Title,folderToken));if(Directory.Exists(dest))dest+="-"+revision[..8];PackageUpdates.Validate(clean);var prior=newPackage?[]:PackageUpdates.Find(parent,packageId,p.Id,SongProject.PackageKind(dance!=null,camera!=null));var proposal=new ExportReview(dest,packageId,p.Id,SongProject.PackageKind(dance!=null,camera!=null),prior);var action=prior.Count>0&&review!=null?review(proposal):PreviousOutputAction.Keep;PackageUpdates.Commit(parent,clean,dest,proposal,action);original.PackageIds[SongProject.PackageKind(dance!=null,camera!=null)]=packageId;original.CameraBone=p.CameraBone;return dest;
  }finally{if(Directory.Exists(stage))Directory.Delete(stage,true);}
 }
}
