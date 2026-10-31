using System.IO;
using System.Security.Cryptography;
using System.Text.Json;
using System.Reflection;
using System.Collections;
namespace DanceStudio;
public sealed record HkxDiagnosticItem(string Item,string Status,string Detail,object? Data=null);
public sealed class HkxDiagnosticReport {
 public string Source {get;set;}="";public string? Sha256 {get;set;}public List<HkxDiagnosticItem> Items {get;set;}=[];
 public string Json()=>JsonSerializer.Serialize(this,new JsonSerializerOptions(SongProject.Json){Encoder=System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping,NumberHandling=System.Text.Json.Serialization.JsonNumberHandling.AllowNamedFloatingPointLiterals});
}
public static class HkxDiagnostics {
 static object Summary(object value){var result=new Dictionary<string,object?>();foreach(var p in value.GetType().GetProperties(BindingFlags.Instance|BindingFlags.Public)){try{if(p.GetIndexParameters().Length>0)continue;var x=p.GetValue(value);if(x==null||x is string||x.GetType().IsPrimitive||x.GetType().IsEnum)result[p.Name]=x;else if(x is ICollection list)result[p.Name]=new{count=list.Count,first=list.Cast<object>().FirstOrDefault()?.ToString(),last=list.Cast<object>().LastOrDefault()?.ToString()};else if(x.GetType().IsValueType)result[p.Name]=x.ToString();}catch{result[p.Name]=UiText.T("t0046");}}return result;}
 public static HkxDiagnosticReport Inspect(string path,Rig? rig=null,int? bindingIndex=null,string? identityRigProof=null){
  var report=new HkxDiagnosticReport{Source=Path.GetFullPath(path)};
  try{
   if(new FileInfo(path).Length>256L*1024*1024)throw new InvalidDataException(UiText.T("t0047"));var bytes=File.ReadAllBytes(path);report.Sha256=Convert.ToHexStringLower(SHA256.HashData(bytes));using var stream=new MemoryStream(bytes,false);var f=MotionClip.ReadStream(stream);var c=f.Container;report.Items.Add(new(UiText.T("t0048"),UiText.T("t0049"),UiText.T("t0050"),Summary(f.Header)));
   report.Items.Add(new(UiText.T("t0051"),UiText.T("t0049"),UiText.T("t0052"),new{animationCount=c.animations.Count,bindingCount=c.bindings.Count,choices=HkxBinding.Choices(c)}));
   for(int i=0;i<c.animations.Count;i++){var a=c.animations[i];object frames=a is HKX2E.hkaInterleavedUncompressedAnimation u&&a.numberOfTransformTracks>0?u.transforms.Count/a.numberOfTransformTracks:a.GetType().GetProperty("numFrames")?.GetValue(a)??UiText.T("t0046");
    report.Items.Add(new(UiText.F("t0053", i),a is HKX2E.hkaInterleavedUncompressedAnimation or HKX2E.hkaSplineCompressedAnimation?UiText.T("t0054"):UiText.T("t0055"),UiText.T("t0056"),new{type=a.GetType().Name,a.duration,frames,a.numberOfTransformTracks,a.numberOfFloatTracks,compressionMetadata=Summary(a)}));
    report.Items.Add(new(UiText.F("t0057", i),a.extractedMotion==null?UiText.T("t0058"):UiText.T("t0049"),a.extractedMotion==null?UiText.T("t0059"):UiText.T("t0060"),a.extractedMotion==null?null:new{type=a.extractedMotion.GetType().Name,properties=Summary(a.extractedMotion)}));
    report.Items.Add(new($"Annotation {i}",UiText.T("t0049"),UiText.T("t0061"),a.annotationTracks.Select(t=>new{t.trackName,events=t.annotations.Select(e=>new{e.time,e.text})}).ToArray()));
   }
   report.Items.Add(new(UiText.T("t0062"),UiText.T("t0049"),UiText.T("t0063"),c.bindings.Select((b,i)=>new{index=i,b.originalSkeletonName,b.blendHint,animationIndex=c.animations.ToList().FindIndex(a=>ReferenceEquals(a,b.animation)),b.transformTrackToBoneIndices,b.floatTrackToFloatSlotIndices}).ToArray()));
   report.Items.Add(new(UiText.T("t0064"),c.skeletons.Count>0?UiText.T("t0049"):UiText.T("t0065"),c.skeletons.Count>0?UiText.T("t0066"):UiText.T("t0067"),c.skeletons.Select(s=>new{s.name,bones=s.bones.Select((b,i)=>new{index=i,b.name,parent=s.parentIndices.Count>i?(int?)s.parentIndices[i]:null}),s.floatSlots}).ToArray()));
   if(rig!=null)report.Items.Add(new(UiText.T("t0068"),UiText.T("t0049"),UiText.T("t0069"),new{rig.Name,rig.SourceReference,rig.SourceHash,fingerprint=rig.Fingerprint,bones=rig.Names.Select((n,i)=>new{index=i,name=n,parent=rig.Parents[i]}),rig.FloatSlots}));
   try{var clip=MotionClip.Load(path,bindingIndex,identityRigProof);if(rig!=null)clip.ValidateRig(rig);report.Items.Add(new(UiText.T("t0070"),UiText.T("t0058"),UiText.T("t0071"),new{clip.Frames,clip.Tracks,clip.AnimationIndex,clip.BindingIndex,clip.ImplicitBinding,userConfirmedRig=clip.IdentityRigProof}));report.Items.Add(new(UiText.T("t0072"),clip.Animation.numberOfFloatTracks==0?UiText.T("t0058"):UiText.T("t0049"),clip.Animation.numberOfFloatTracks==0?UiText.T("t0073"):UiText.T("t0074"),new{count=clip.Animation.numberOfFloatTracks,decodedFrames=clip.FloatSamples.Length,ranges=Enumerable.Range(0,clip.Animation.numberOfFloatTracks).Select(i=>new{track=i,min=clip.FloatSamples.Min(row=>row[i]),max=clip.FloatSamples.Max(row=>row[i])})}));}
   catch(Exception e){report.Items.Add(new(UiText.T("t0070"),UiText.T("t0055"),e.Message));report.Items.Add(new(UiText.T("t0075"),UiText.T("t0065"),UiText.T("t0076")));}
   report.Items.Add(new(UiText.T("t0077"),UiText.T("t0054"),UiText.T("t0078")));
   if(Convert.ToHexStringLower(SHA256.HashData(File.ReadAllBytes(path)))!=report.Sha256)report.Items.Add(new(UiText.T("t0079"),UiText.T("t0055"),UiText.T("t0080")));
  }catch(Exception e){report.Items.Add(new(UiText.T("t0081"),UiText.T("t0065"),e.Message+UiText.T("t0082")));}
  return report;
 }
}
