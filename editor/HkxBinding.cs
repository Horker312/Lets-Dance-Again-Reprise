using HKX2E;
using System.IO;
namespace DanceStudio;
public sealed record HkxChoice(int BindingIndex,int AnimationIndex,string Type,double Duration,int Tracks);
public sealed class HkxSelectionRequired:Exception {
 public HkxChoice[] Choices {get;}
 public HkxSelectionRequired(HkxChoice[] choices):base("복수의 유효한 애니메이션/바인딩이 있습니다. 사용할 바인딩을 직접 선택하세요."){Choices=choices;}
}
public sealed class HkxMappingRequired:Exception {public HkxMappingRequired(string reason="빈 Transform 바인딩의 본 순서를 입증할 메타데이터가 없습니다. 사용할 스켈레톤 순서를 직접 확인해야 합니다."):base(reason){} }
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
  if(choices.Length==0)throw new InvalidDataException("컨테이너의 애니메이션과 연결되는 바인딩이 없습니다. 임의로 첫 항목을 선택하지 않았습니다.");
  if(bindingIndex.HasValue)return choices.SingleOrDefault(x=>x.BindingIndex==bindingIndex.Value)??throw new InvalidDataException("선택한 바인딩이 애니메이션 목록에 연결되지 않습니다.");
  if(choices.Length!=1)throw new HkxSelectionRequired(choices);
  return choices[0];
 }
 public static void ValidateIndices(int[] map,int tracks){
  if(map.Length!=tracks)throw new InvalidDataException("Transform 바인딩 수와 트랙 수가 다릅니다.");
  if(map.Any(x=>x<0)||map.Distinct().Count()!=map.Length)throw new InvalidDataException("Transform 바인딩에 음수 또는 중복 본 인덱스가 있습니다.");
 }
 public static void ValidateRig(MotionClip clip,Rig rig){
  ValidateIndices(clip.BoneMap,clip.Tracks);
  if(clip.BoneMap.Any(x=>x>=rig.Names.Length))throw new InvalidDataException("Transform 바인딩이 선택한 스켈레톤 범위를 벗어납니다.");
  if(clip.Container==null)return; // Explicitly constructed test clips have no source metadata.
  var binding=clip.Container.bindings[clip.BindingIndex];
  if(Convert.ToInt32(binding.blendHint)!=0)throw new NotSupportedException("Additive 모션은 기준 포즈와 합성해야 하므로 단독 춤/카메라로 미리보기·변환하지 않습니다.");
  var source=clip.Container.skeletons?.FirstOrDefault(x=>x.name==binding.originalSkeletonName);
  if(source==null&&!string.IsNullOrEmpty(binding.originalSkeletonName)&&binding.originalSkeletonName!=rig.Name&&!string.Equals(clip.IdentityRigProof,clip.MappingProofFor(rig),StringComparison.OrdinalIgnoreCase))throw new HkxMappingRequired("원본 외부 스켈레톤 이름과 선택한 스켈레톤 이름이 다릅니다. 같은 본 순서라는 확인 없이 자동 재타겟하지 않습니다.");
  if(source!=null){foreach(int b in clip.BoneMap)if(b>=source.bones.Count||b>=source.parentIndices.Count||source.bones[b].name!=rig.Names[b]||source.parentIndices[b]!=rig.Parents[b])throw new InvalidDataException("원본과 선택한 스켈레톤의 본 이름·순서·부모가 다릅니다. 자동 재매핑하지 않았습니다.");}
  if(clip.ImplicitBinding){
   bool proven=source!=null||string.Equals(clip.IdentityRigProof,clip.MappingProofFor(rig),StringComparison.OrdinalIgnoreCase);
   if(!proven&&clip.Animation.annotationTracks.Count>=clip.Tracks)proven=Enumerable.Range(0,clip.Tracks).All(i=>clip.Animation.annotationTracks[i].trackName==rig.Names[i]);
   if(!proven)throw new HkxMappingRequired();
  }
 }
}


