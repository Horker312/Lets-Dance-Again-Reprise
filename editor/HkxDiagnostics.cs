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
 static object Summary(object value){var result=new Dictionary<string,object?>();foreach(var p in value.GetType().GetProperties(BindingFlags.Instance|BindingFlags.Public)){try{if(p.GetIndexParameters().Length>0)continue;var x=p.GetValue(value);if(x==null||x is string||x.GetType().IsPrimitive||x.GetType().IsEnum)result[p.Name]=x;else if(x is ICollection list)result[p.Name]=new{count=list.Count,first=list.Cast<object>().FirstOrDefault()?.ToString(),last=list.Cast<object>().LastOrDefault()?.ToString()};else if(x.GetType().IsValueType)result[p.Name]=x.ToString();}catch{result[p.Name]="확인 불가";}}return result;}
 public static HkxDiagnosticReport Inspect(string path,Rig? rig=null,int? bindingIndex=null,string? identityRigProof=null){
  var report=new HkxDiagnosticReport{Source=Path.GetFullPath(path)};
  try{
   if(new FileInfo(path).Length>256L*1024*1024)throw new InvalidDataException("진단 HKX는 256MB 이하만 지원합니다.");var bytes=File.ReadAllBytes(path);report.Sha256=Convert.ToHexStringLower(SHA256.HashData(bytes));using var stream=new MemoryStream(bytes,false);var f=MotionClip.ReadStream(stream);var c=f.Container;report.Items.Add(new("파일 헤더","부분 지원","원본 버전/레이아웃 메타데이터입니다. 플랫폼·엔진 실행 호환성을 입증하지는 않습니다.",Summary(f.Header)));
   report.Items.Add(new("구성","부분 지원","모호한 연결은 선택이 필요합니다. 복수 구성의 출력은 다른 데이터 손실을 막기 위해 차단합니다.",new{animationCount=c.animations.Count,bindingCount=c.bindings.Count,choices=HkxBinding.Choices(c)}));
   for(int i=0;i<c.animations.Count;i++){var a=c.animations[i];object frames=a is HKX2E.hkaInterleavedUncompressedAnimation u&&a.numberOfTransformTracks>0?u.transforms.Count/a.numberOfTransformTracks:a.GetType().GetProperty("numFrames")?.GetValue(a)??"확인 불가";
    report.Items.Add(new($"애니메이션 {i}",a is HKX2E.hkaInterleavedUncompressedAnimation or HKX2E.hkaSplineCompressedAnimation?"변환 후 지원":"지원되지 않음","지원 가능한 형식은 정수 프레임 디코딩 후 비압축 TRS로 재샘플링합니다. 압축 곡선 원본의 연속 시간 정확도는 독립 검증되지 않았습니다.",new{type=a.GetType().Name,a.duration,frames,a.numberOfTransformTracks,a.numberOfFloatTracks,compressionMetadata=Summary(a)}));
    report.Items.Add(new($"루트 모션 {i}",a.extractedMotion==null?"정상 지원":"부분 지원",a.extractedMotion==null?"이 애니메이션에 extractedMotion 참조가 없습니다.":"형식과 시간 메타데이터를 기록합니다. 무편집 전체 구간 바이트 복사만 허용하며 자르기·배속·포즈 편집은 차단합니다. 게임 이동 반영은 별도 미검증입니다.",a.extractedMotion==null?null:new{type=a.extractedMotion.GetType().Name,properties=Summary(a.extractedMotion)}));
    report.Items.Add(new($"Annotation {i}","부분 지원","시간·텍스트·트랙 이름을 보존/비교하지만 TRS 미리보기에서는 이벤트를 실행하지 않습니다. 이벤트의 Behavior·스크립트 의존성과 실제 실행은 의미 확인 불가이며 강제 실행하지 않습니다.",a.annotationTracks.Select(t=>new{t.trackName,events=t.annotations.Select(e=>new{e.time,e.text})}).ToArray()));
   }
   report.Items.Add(new("바인딩","부분 지원","명시적 배열을 검증합니다. 빈 Transform 매핑은 본 이름/원본 스켈레톤으로 입증되기 전까지 사용하지 않습니다.",c.bindings.Select((b,i)=>new{index=i,b.originalSkeletonName,b.blendHint,animationIndex=c.animations.ToList().FindIndex(a=>ReferenceEquals(a,b.animation)),b.transformTrackToBoneIndices,b.floatTrackToFloatSlotIndices}).ToArray()));
   report.Items.Add(new("원본 스켈레톤",c.skeletons.Count>0?"부분 지원":"의미 확인 불가",c.skeletons.Count>0?"본 이름·인덱스·부모·Float 슬롯을 기록합니다.":"컨테이너 안에 스켈레톤이 없습니다. 외부 스켈레톤 부재를 의미하지 않습니다.",c.skeletons.Select(s=>new{s.name,bones=s.bones.Select((b,i)=>new{index=i,b.name,parent=s.parentIndices.Count>i?(int?)s.parentIndices[i]:null}),s.floatSlots}).ToArray()));
   if(rig!=null)report.Items.Add(new("편집기 스켈레톤","부분 지원","내장/사용자 지정 본 구조입니다. 외부 원본 스켈레톤이 없으면 전체 동일성을 입증할 수 없습니다.",new{rig.Name,rig.SourceReference,rig.SourceHash,fingerprint=rig.Fingerprint,bones=rig.Names.Select((n,i)=>new{index=i,name=n,parent=rig.Parents[i]}),rig.FloatSlots}));
   try{var clip=MotionClip.Load(path,bindingIndex,identityRigProof);if(rig!=null)clip.ValidateRig(rig);report.Items.Add(new("디코딩/매핑","정상 지원","현재 디코더와 선택한 바인딩의 TRS를 읽었습니다. 파일을 읽었다고 실제 Skyrim 재현이 검증된 것은 아닙니다.",new{clip.Frames,clip.Tracks,clip.AnimationIndex,clip.BindingIndex,clip.ImplicitBinding,userConfirmedRig=clip.IdentityRigProof}));report.Items.Add(new("Float 데이터",clip.Animation.numberOfFloatTracks==0?"정상 지원":"부분 지원",clip.Animation.numberOfFloatTracks==0?"선택한 애니메이션의 Float 트랙 수가 0입니다.":"인덱스·바인딩·표본을 보존하지만 TRS 미리보기에서 Float를 적용하지 않습니다. 채널의 의미는 따로 확인해야 합니다. 현재 Spline은 블록별 정적 Float만 지원하며 일반 가변 Spline은 미지원입니다.",new{count=clip.Animation.numberOfFloatTracks,decodedFrames=clip.FloatSamples.Length,ranges=Enumerable.Range(0,clip.Animation.numberOfFloatTracks).Select(i=>new{track=i,min=clip.FloatSamples.Min(row=>row[i]),max=clip.FloatSamples.Max(row=>row[i])})}));}
   catch(Exception e){report.Items.Add(new("디코딩/매핑","지원되지 않음",e.Message));report.Items.Add(new("미해석 데이터","의미 확인 불가","디코딩 실패가 데이터 부재를 뜻하지 않습니다. 위 원본 구성 정보를 확인하세요."));}
   report.Items.Add(new("보존/변환 계획","변환 후 지원","바인딩과 Annotation 이름/텍스트, 지원 Float를 유지합니다. 편집 시 TRS·지원 Float는 출력 FPS로 재샘플링됩니다. 무편집 전체 구간이며 원본 표본율과 출력 FPS가 같으면 원본 압축 바이트를 그대로 복사합니다. extractedMotion은 무편집 전체 복사만 허용합니다. 지정한 반전·기준축 변환은 의도된 편집으로 비교에 반영합니다."));
   if(Convert.ToHexStringLower(SHA256.HashData(File.ReadAllBytes(path)))!=report.Sha256)report.Items.Add(new("동시 파일 변경","지원되지 않음","검사 중 원본이 변경되었습니다. 이 보고서로 출력하지 말고 다시 검사하세요."));
  }catch(Exception e){report.Items.Add(new("구조 검사 실패","의미 확인 불가",e.Message+" 해석하지 못한 항목의 존재 여부는 확인 불가입니다."));}
  return report;
 }
}
