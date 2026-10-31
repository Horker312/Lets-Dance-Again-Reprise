using HKX2E;
using System.IO;
namespace DanceStudio;
public sealed record HkxChoice(int BindingIndex,int AnimationIndex,string Type,double Duration,int Tracks);
public sealed class HkxSelectionRequired:Exception {
 public HkxChoice[] Choices {get;}
 public HkxSelectionRequired(HkxChoice[] choices):base(UiText.T("t0036")){Choices=choices;}
}
public sealed class HkxMappingRequired:Exception {public HkxMappingRequired(string? reason=null):base(reason??UiText.T("t0037")){} }
public static class HkxBinding {
 public static HkxChoice[] Choices(hkaAnimationContainer container){
  var result=new List<HkxChoice>();
  for(int b=0;b<(container.bindings?.Count??0);b++){
   var binding=container.bindings![b];int a=container.animations?.ToList().FindIndex(x=>ReferenceEquals(x,binding.animation))??-1;
   if(a>=0){var anim=container.animations![a];result.Add(new(b,a,anim.GetType().Name,anim.duration,anim.numberOfTransformTracks));}
  }
  return result.ToArray();
 }
 public static HkxChoice Select(hkaAnimationContainer container,int? bindingIndex){
  var choices=Choices(container);
  if(choices.Length==0)throw new InvalidDataException(UiText.T("t0038"));
  if(bindingIndex.HasValue)return choices.SingleOrDefault(x=>x.BindingIndex==bindingIndex.Value)??throw new InvalidDataException(UiText.T("t0039"));
  if(choices.Length!=1)throw new HkxSelectionRequired(choices);
  return choices[0];
 }
 public static void ValidateIndices(int[] map,int tracks){
  if(map.Length!=tracks)throw new InvalidDataException(UiText.T("t0040"));
  if(map.Any(x=>x<0)||map.Distinct().Count()!=map.Length)throw new InvalidDataException(UiText.T("t0041"));
 }
 public static void ValidateRig(MotionClip clip,Rig rig){
  ValidateIndices(clip.BoneMap,clip.Tracks);
  if(clip.BoneMap.Any(x=>x>=rig.Names.Length))throw new InvalidDataException(UiText.T("t0042"));
  if(clip.Container==null)return; // Explicitly constructed test clips have no source metadata.
  var binding=clip.Container.bindings[clip.BindingIndex];
  if(Convert.ToInt32(binding.blendHint)!=0)throw new NotSupportedException(UiText.T("t0043"));
  var source=clip.Container.skeletons?.FirstOrDefault(x=>x.name==binding.originalSkeletonName);
  if(source==null&&!string.IsNullOrEmpty(binding.originalSkeletonName)&&binding.originalSkeletonName!=rig.Name&&!string.Equals(clip.IdentityRigProof,clip.MappingProofFor(rig),StringComparison.OrdinalIgnoreCase))throw new HkxMappingRequired(UiText.T("t0044"));
  if(source!=null){foreach(int b in clip.BoneMap)if(b>=source.bones.Count||b>=source.parentIndices.Count||source.bones[b].name!=rig.Names[b]||source.parentIndices[b]!=rig.Parents[b])throw new InvalidDataException(UiText.T("t0045"));}
  if(clip.ImplicitBinding){
   bool proven=source!=null||string.Equals(clip.IdentityRigProof,clip.MappingProofFor(rig),StringComparison.OrdinalIgnoreCase);
   if(!proven&&clip.Animation.annotationTracks.Count>=clip.Tracks)proven=Enumerable.Range(0,clip.Tracks).All(i=>clip.Animation.annotationTracks[i].trackName==rig.Names[i]);
   if(!proven)throw new HkxMappingRequired();
  }
 }
}


