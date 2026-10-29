using HKX2E;
using System.IO;
using System.Numerics;
using System.Reflection;
using System.Linq.Expressions;
namespace DanceStudio;

public readonly record struct Pose(Vector3 T,Quaternion R,Vector3 S) {
 public static Pose FromMatrix(Matrix4x4 m) {
  // HKX2E packs hka QSTransform's T/R/S into matrix ROWS. This is not an
  // affine matrix and must never pass through Matrix4x4.Decompose.
  var q=new Quaternion(m.M21,m.M22,m.M23,m.M24);
  if(q.LengthSquared()<.0001)throw new InvalidDataException("회전 데이터가 올바르지 않습니다.");
  return new(new(m.M11,m.M12,m.M13),Quaternion.Normalize(q),new(m.M31,m.M32,m.M33));
 }
 public Matrix4x4 Matrix()=>new(T.X,T.Y,T.Z,0,R.X,R.Y,R.Z,R.W,S.X,S.Y,S.Z,0,0,0,0,0);
 public static Pose Blend(Pose a,Pose b,float t)=>new(Vector3.Lerp(a.T,b.T,t),Quaternion.Normalize(Quaternion.Slerp(a.R,b.R,t)),Vector3.Lerp(a.S,b.S,t));
}
public sealed class Rig {
 public const string BuiltinId="builtin://skyrim-xpmse";
 public static bool IsBuiltin(string? path)=>string.IsNullOrWhiteSpace(path)||path==BuiltinId;
 public static string ResolveReference(string? path)=>IsBuiltin(path)?BuiltinId:System.IO.Path.GetFullPath(path!);
 public static byte[] SourceBytes(string path){if(!IsBuiltin(path))return File.ReadAllBytes(path);using var stream=typeof(Rig).Assembly.GetManifestResourceStream("DanceStudio.DefaultSkeleton.hkx")??throw new InvalidDataException("내장 스켈레톤 리소스가 없습니다.");using var memory=new MemoryStream();stream.CopyTo(memory);return memory.ToArray();}

 public string Fingerprint=>Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(System.Text.Json.JsonSerializer.Serialize(new{Name,Names,Parents,FloatSlots,reference=Reference.Select(p=>new{t=new[]{p.T.X,p.T.Y,p.T.Z},r=new[]{p.R.X,p.R.Y,p.R.Z,p.R.W},s=new[]{p.S.X,p.S.Y,p.S.Z}})}))));
 public string? SourceReference;public string SourceHash="";
 public void EnsureSourceUnchanged(){if(SourceReference!=null&&Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(SourceBytes(SourceReference)))!=SourceHash)throw new IOException("불러온 뒤 스켈레톤 원본이 변경되었습니다. 다시 불러오세요.");}
 public string Name="";public string[] FloatSlots=[];
 public string[] Names=[]; public short[] Parents=[]; public Pose[] Reference=[];
 public static Rig Load(string path) {
  path=ResolveReference(path);if(!IsBuiltin(path)&&new FileInfo(path).Length>256L*1024*1024)throw new InvalidDataException("스켈레톤 HKX는 256MB 이하만 지원합니다.");var bytes=SourceBytes(path);using var skeletonStream=new MemoryStream(bytes,false);var f=MotionClip.ReadStream(skeletonStream);var candidates=f.Container.skeletons?.Where(x=>x.bones.Count>0&&!x.name.StartsWith("Ragdoll",StringComparison.OrdinalIgnoreCase)).ToArray()??[];if(candidates.Length!=1)throw new InvalidDataException("스켈레톤 후보가 없거나 여러 개입니다. 사용할 단일 스켈레톤 파일을 선택하세요.");var s=candidates[0];
  var r=new Rig{SourceReference=path,SourceHash=Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(bytes)),Name=s.name,FloatSlots=s.floatSlots.ToArray(),Names=s.bones.Select(b=>b.name).ToArray(),Parents=s.parentIndices.ToArray(),Reference=s.referencePose.Select(Pose.FromMatrix).ToArray()};
  if(r.Names.Length!=r.Parents.Length||r.Names.Length!=r.Reference.Length||r.Names.Distinct().Count()!=r.Names.Length)throw new InvalidDataException("스켈레톤 본·부모·기준 자세의 개수 또는 이름이 올바르지 않습니다.");
  for(int i=0;i<r.Parents.Length;i++)if(r.Parents[i]>=i||r.Parents[i]<-1)throw new InvalidDataException("스켈레톤 계층이 올바르지 않습니다.");
  return r;
 }
 public Pose[] World(Pose[] local) {
  var w=new Pose[Names.Length];
  for(int i=0;i<w.Length;i++) {
   var x=local[i];int parent=Parents[i];
   if(parent>=0){var p=w[parent];x=new(p.T+Vector3.Transform(x.T*p.S,p.R),Quaternion.Normalize(p.R*x.R),p.S*x.S);}
   w[i]=x;
  }
  return w;
 }
}
public sealed class MotionClip {
 byte[] sourceSnapshot=[];
 public string SourceHash="";
 public string MappingProofFor(Rig rig)=>Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(rig.Fingerprint+":"+SourceHash)));
 public void EnsureSourceUnchanged(){if(!File.Exists(Path)||Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(File.ReadAllBytes(Path)))!=SourceHash)throw new IOException("불러온 뒤 원본 HKX가 변경되었거나 없어졌습니다. 다시 불러오세요.");}

 public string Path=""; public double Duration; public int Tracks; public int Frames=>Samples.Length;
 float[][]? staticFloatBlocks;int staticFloatSpan;
 public Pose[][] Samples=[];public int[] BoneMap=[];public float[][] FloatSamples=[];
 public hkRootLevelContainer Root=null!;public hkaAnimationContainer Container=null!;public HKXHeader Header=null!;
 public string SupportSummary=>Container==null?"원본 메타데이터 없음":$"{Animation.GetType().Name} · {Frames}프레임 / {Tracks} Transform / {Animation.numberOfFloatTracks} Float · 출력 시 재샘플링 · extractedMotion {(Animation.extractedMotion==null?"참조 없음":"있음: 편집 제한")} · 외부 이벤트 실행/독립 Havok 곡선 검증은 미확인";
 public int AnimationIndex,BindingIndex;public bool ImplicitBinding;public string? IdentityRigProof;
 public hkaAnimation Animation=>Container.animations![AnimationIndex];
 Rig? validatedRig;public void ValidateRig(Rig rig){if(validatedRig==rig)return;HkxBinding.ValidateRig(this,rig);validatedRig=rig;}
 public static (hkRootLevelContainer Root,hkaAnimationContainer Container,HKXHeader Header) Read(string path) {
  if(new FileInfo(path).Length>256L*1024*1024)throw new InvalidDataException("HKX는 256MB 이하만 지원합니다.");
  using var f=File.OpenRead(path);return ReadStream(f);
 }
 public static (hkRootLevelContainer Root,hkaAnimationContainer Container,HKXHeader Header) ReadStream(Stream f) {
  if(f.Length>256L*1024*1024)throw new InvalidDataException("HKX는 256MB 이하만 지원합니다.");
  var reader=new PackFileDeserializer();
  var root=reader.Deserialize(new BinaryReaderEx(f)) as hkRootLevelContainer??throw new InvalidDataException("Skyrim SE HKX가 아닙니다.");
  var containers=root.namedVariants.Select(v=>v!.variant).OfType<hkaAnimationContainer>().ToArray();if(containers.Length!=1)throw new InvalidDataException($"애니메이션 컨테이너 {containers.Length}개: 이 구성은 선택 관계를 확정할 수 없어 지원하지 않습니다.");return(root,containers[0],reader._header);
 }
 static Func<object,T> Access<T>(System.Type type,string name) {
  var x=Expression.Parameter(typeof(object));var member=Expression.PropertyOrField(Expression.Convert(x,type),name);
  return Expression.Lambda<Func<object,T>>(member,x).Compile();
 }
 public static MotionClip Load(string path,int? bindingIndex=null,string? identityRigProof=null) {
  using var input=File.OpenRead(path);if(input.Length>256L*1024*1024)throw new InvalidDataException("HKX 파일이 너무 큽니다.");using var memory=new MemoryStream();input.CopyTo(memory);byte[] snapshot=memory.ToArray();using var source=new MemoryStream(snapshot,false);var f=ReadStream(source);var selection=HkxBinding.Select(f.Container,bindingIndex);var a=f.Container.animations![selection.AnimationIndex];var b=f.Container.bindings![selection.BindingIndex];
  if(a.duration<=0||!float.IsFinite(a.duration)||a.duration>1800||a.numberOfTransformTracks<1||a.numberOfTransformTracks>512)throw new InvalidDataException("길이·트랙 수가 지원 범위를 벗어났습니다.");
  var result=new MotionClip{Path=System.IO.Path.GetFullPath(path),Duration=a.duration,Tracks=a.numberOfTransformTracks,Root=f.Root,Container=f.Container,Header=f.Header,sourceSnapshot=snapshot,SourceHash=Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(snapshot)),AnimationIndex=selection.AnimationIndex,BindingIndex=selection.BindingIndex,ImplicitBinding=b.transformTrackToBoneIndices.Count==0,IdentityRigProof=identityRigProof};
  if(b.transformTrackToBoneIndices.Count!=0&&b.transformTrackToBoneIndices.Count!=result.Tracks)throw new InvalidDataException("부분적으로 누락된 Transform 바인딩은 인덱스로 보충할 수 없습니다.");
  result.BoneMap=result.ImplicitBinding?Enumerable.Range(0,result.Tracks).ToArray():b.transformTrackToBoneIndices.Select(x=>(int)x).ToArray();HkxBinding.ValidateIndices(result.BoneMap,result.Tracks);
  if(a is hkaInterleavedUncompressedAnimation u) {
   if(u.transforms.Count%result.Tracks!=0||u.transforms.Count/result.Tracks<2||u.transforms.Count>4000000)throw new InvalidDataException("비압축 Transform 프레임 배열 길이가 올바르지 않습니다.");
   int n=u.transforms.Count/result.Tracks;result.Samples=new Pose[n][];
   for(int i=0;i<n;i++)result.Samples[i]=u.transforms.Skip(i*result.Tracks).Take(result.Tracks).Select(Pose.FromMatrix).ToArray();
  } else if(a is hkaSplineCompressedAnimation spline) {
   var num=spline.GetType().GetProperty("numFrames")?.GetValue(spline);
   if(num!=null&&Convert.ToInt64(num)*result.Tracks>4000000)throw new InvalidDataException("미리보기 데이터가 너무 큽니다. 더 짧은 HKX를 선택하세요.");
   var decoder=Assembly.LoadFrom(System.IO.Path.Combine(AppContext.BaseDirectory,"Matcher.dll")).GetType("MotionEditor.Core.SplinePreview")!.GetMethod("Decode",BindingFlags.Static|BindingFlags.Public|BindingFlags.NonPublic)!;
   var decoded=(Array)(decoder.Invoke(null,new object[]{a})??throw new InvalidDataException("HKX 디코딩 결과가 비어 있습니다."));
   if(decoded.Length<2||decoded.Length>60000)throw new InvalidDataException("프레임 수가 지원 범위를 벗어났습니다.");
   var first=(Array)decoded.GetValue(0)!;var type=first.GetValue(0)!.GetType();
   var t=Access<Vector3>(type,"Translation");var q=Access<Quaternion>(type,"Rotation");var s=Access<Vector3>(type,"Scale");
   result.Samples=new Pose[decoded.Length][];
   for(int i=0;i<decoded.Length;i++) {
    var row=(Array)decoded.GetValue(i)!;var poses=new Pose[result.Tracks];
    for(int j=0;j<poses.Length;j++){var v=row.GetValue(j)!;poses[j]=new(t(v),Quaternion.Normalize(q(v)),s(v));}
    result.Samples[i]=poses;
   }
  } else throw new NotSupportedException("Spline 압축 또는 비압축 Skyrim SE 애니메이션만 지원합니다.");
  foreach(var row in result.Samples)foreach(var p in row)
   if(!float.IsFinite(p.T.X+p.T.Y+p.T.Z+p.R.X+p.R.Y+p.R.Z+p.R.W+p.S.X+p.S.Y+p.S.Z))throw new InvalidDataException("유효하지 않은 좌표가 있습니다.");
  int floats=a.numberOfFloatTracks;
  if(floats>0) {
   if(a is hkaInterleavedUncompressedAnimation plain) {
    if(plain.floats.Count!=result.Frames*floats)throw new InvalidDataException("Float 트랙 길이가 잘못되었습니다.");
    result.FloatSamples=Enumerable.Range(0,result.Frames).Select(i=>plain.floats.Skip(i*floats).Take(floats).ToArray()).ToArray();
   } else if(a is hkaSplineCompressedAnimation c) {
    // Static float channels per block (the legacy LDA camera files use this
    // layout). Reject spline-valued channels rather than silently discarding.
    var blocks=new List<float[]>();
    if(c.endian!=0||c.numBlocks<1||c.maxFramesPerBlock<2||c.blockOffsets.Count!=c.numBlocks||c.floatBlockOffsets.Count!=c.numBlocks)throw new NotSupportedException("이 Float 트랙 배치는 아직 지원하지 않습니다.");
    for(int block=0;block<c.numBlocks;block++) {
     long start64=c.blockOffsets[block],end64=block+1<c.numBlocks?c.blockOffsets[block+1]:c.data.Count,offset64=start64+c.floatBlockOffsets[block];
     if(start64<0||end64<=start64||end64>c.data.Count||start64+result.Tracks*4+floats>end64||offset64<start64||offset64+floats*4>end64)throw new InvalidDataException("Float 블록 오프셋/마스크/데이터 범위를 벗어났습니다.");int start=(int)start64,offset=(int)offset64;
     var masks=c.data.Skip(start+result.Tracks*4).Take(floats).ToArray();
     if(masks.Length!=floats||masks.Any(x=>x!=3)||offset+floats*4>c.data.Count)throw new NotSupportedException($"Spline Float 마스크 {string.Join(",",masks.Select(x=>x.ToString("X2")))}는 검증된 디코더가 없어 지원하지 않습니다. 0으로 대체하지 않았습니다.");
     var values=Enumerable.Range(0,floats).Select(i=>BitConverter.ToSingle(c.data.Skip(offset+i*4).Take(4).ToArray())).ToArray();
     if(values.Any(x=>!float.IsFinite(x)))throw new InvalidDataException("Float 트랙 값이 올바르지 않습니다.");blocks.Add(values);
    }
    result.staticFloatBlocks=blocks.ToArray();result.staticFloatSpan=(int)c.maxFramesPerBlock-1;
    result.FloatSamples=Enumerable.Range(0,result.Frames).Select(i=>blocks[Math.Min(i/Math.Max(1,(int)c.maxFramesPerBlock-1),blocks.Count-1)].ToArray()).ToArray();
   }
  }
  if(b.floatTrackToFloatSlotIndices.Count!=0&&(b.floatTrackToFloatSlotIndices.Count!=floats||b.floatTrackToFloatSlotIndices.Any(x=>x<0)||b.floatTrackToFloatSlotIndices.Distinct().Count()!=floats))throw new InvalidDataException("Float 바인딩 수·음수·중복을 확인하세요.");
  if(result.FloatSamples.Any(row=>row.Any(x=>!float.IsFinite(x))))throw new InvalidDataException("Float 값이 유한하지 않습니다.");
  foreach(var track in a.annotationTracks)foreach(var ev in track.annotations)if(!float.IsFinite(ev.time)||ev.time<0||ev.time>a.duration+.0001)throw new InvalidDataException("Annotation 이벤트 시간이 모션 범위를 벗어납니다.");
  return result;
 }
 Rig? editRig;MotionEdits? edits;
 public MotionEdits Edits(Rig rig){if(edits==null||editRig!=rig){edits=new MotionEdits(rig,this);editRig=rig;}return edits;}
 public Pose[] EditedWorld(Rig rig,double time,bool mirror,double yaw)=>!mirror&&yaw==0?World(rig,time):Edits(rig).World(Edits(rig).Dance(time,mirror,yaw));
 public float[] FloatsAt(double time) {
  if(!double.IsFinite(time))throw new InvalidDataException("Float 샘플 시간이 유한하지 않습니다.");
  if(FloatSamples.Length==0)return [];
  if(staticFloatBlocks!=null){double frame=Math.Clamp(time/Duration,0,1)*(Frames-1);return staticFloatBlocks[Math.Min((int)Math.Floor(frame/staticFloatSpan+1e-7),staticFloatBlocks.Length-1)].ToArray();}
  double f=Math.Clamp(time/Duration,0,1)*(Frames-1);int lo=(int)f,hi=Math.Min(lo+1,Frames-1);float blend=(float)(f-lo);
  return FloatSamples[lo].Zip(FloatSamples[hi],(a,b)=>a+(b-a)*blend).ToArray();
 }
 public Pose[] At(double time) {
  if(!double.IsFinite(time))throw new InvalidDataException("모션 샘플 시간이 유한하지 않습니다.");
  double f=Math.Clamp(time/Duration,0,1)*(Frames-1);int lo=(int)f,hi=Math.Min(lo+1,Frames-1);float blend=(float)(f-lo);
  return Enumerable.Range(0,Tracks).Select(i=>Pose.Blend(Samples[lo][i],Samples[hi][i],blend)).ToArray();
 }
 public Pose[] World(Rig rig,double time) {
  ValidateRig(rig);var local=rig.Reference.ToArray();var pose=At(time);
  for(int i=0;i<Tracks;i++){int b=BoneMap[i];if(b>=0&&b<local.Length)local[b]=pose[i];else throw new InvalidDataException("선택한 스켈레톤과 HKX의 본 연결이 맞지 않습니다.");}
  return rig.World(local);
 }
 public void Bake(string path,TrackEdit edit,double begin,double end,double fps,Func<double,Pose[]>? editPose=null,Rig? validationRig=null,MotionCheckOptions? checkOptions=null,bool preserveOriginal=false) {
  edit.Validate(Duration);if(!double.IsFinite(begin)||!double.IsFinite(end)||begin<0)throw new InvalidDataException("출력 시간 범위가 올바르지 않습니다.");EnsureSourceUnchanged();double duration=end-begin;
  if(duration<=0||duration>1800||!double.IsFinite(fps)||fps is <1 or >120)throw new InvalidDataException("내보내기 길이 또는 FPS를 확인하세요.");
  if(!edit.HoldEnd&&end>edit.TimelineEnd+.0001)throw new InvalidDataException("트랙 끝 자세 유지가 꺼져 있습니다. 출력 끝을 트랙 범위 안으로 옮기세요.");
  if(System.IO.Path.GetFullPath(path).Equals(System.IO.Path.GetFullPath(Path),StringComparison.OrdinalIgnoreCase))throw new IOException("원본 HKX를 덮어쓸 수 없습니다.");
  if(validationRig!=null)ValidateRig(validationRig);
  if(Container.animations.Count!=1||Container.bindings.Count!=1)throw new NotSupportedException("복수 구성은 선택한 모션의 미리보기·진단만 지원합니다. 다른 데이터를 조용히 제거하지 않기 위해 출력은 차단했습니다.");
  bool exact=edit.In==0&&Math.Abs(edit.Out-Duration)<1e-6&&edit.Start==0&&edit.Speed==1&&begin==0&&Math.Abs(end-Duration)<1e-6&&editPose==null;
  if(Animation.extractedMotion!=null){if(!exact)throw new NotSupportedException("루트 모션의 자르기·배속·포즈 편집은 미지원입니다. 삭제하지 않았습니다.");File.WriteAllBytes(path,sourceSnapshot);File.WriteAllText(path+".preservation.json",System.Text.Json.JsonSerializer.Serialize(new{rootMotion="exact original byte copy; not resampled; game actor movement unverified",source=Path}));return;}
  if(exact&&(preserveOriginal||Math.Abs((Frames-1)/Duration-fps)<.0001)){File.WriteAllBytes(path,sourceSnapshot);var preserved=System.Text.Json.JsonSerializer.SerializeToNode(MotionComparison.Compare(this,Load(path),edit,begin,end,null,validationRig,checkOptions??new()),SongProject.Json)!;preserved["preservationMode"]="byte-identical original; no resampling";File.WriteAllText(path+".verification.json",preserved.ToJsonString(SongProject.Json));return;}
  int count=Math.Max(2,(int)Math.Round(duration*fps)+1);
  if((long)count*Tracks>4000000)throw new InvalidDataException("내보낼 프레임이 너무 많습니다. 구간을 줄이세요.");
  using var snapshotStream=new MemoryStream(sourceSnapshot,false);var f=ReadStream(snapshotStream);var a=f.Container.animations![AnimationIndex];var transforms=new List<Matrix4x4>(count*Tracks);var outputFloats=new List<float>();
  for(int i=0;i<count;i++) {
   double source=Math.Clamp(edit.SourceTime(begin+duration*i/(count-1)),edit.In,edit.Out);
   var row=editPose==null?At(source):editPose(source);if(row.Length!=Tracks)throw new InvalidDataException("편집된 본 트랙 수가 다릅니다.");transforms.AddRange(row.Select(p=>p.Matrix()));
   outputFloats.AddRange(FloatsAt(source));
  }
  var annotations=a.annotationTracks.Select(track=>new hkaAnnotationTrack{trackName=track.trackName,annotations=track.annotations.Where(x=>x.time>=edit.In&&x.time<=edit.Out).Select(x=>new hkaAnnotationTrackAnnotation{time=(float)((x.time-edit.In)/edit.Speed+edit.Start-begin),text=x.text}).Where(x=>x.time>=0&&x.time<=duration).ToList()}).ToList();
  var output=new hkaInterleavedUncompressedAnimation{type=1,duration=(float)duration,numberOfTransformTracks=Tracks,numberOfFloatTracks=a.numberOfFloatTracks,annotationTracks=annotations,transforms=transforms,floats=outputFloats};
  f.Container.animations[0]=output;f.Container.bindings![0].animation=output;
  var serializer=new PackFileSerializer();using(var stream=File.Create(path))serializer.Serialize(f.Root,new BinaryWriterEx(stream),f.Header);
  var check=Read(path).Container.animations![0];
  if(Math.Abs(check.duration-duration)>.0001||check.numberOfTransformTracks!=Tracks||!Read(path).Container.bindings![0].transformTrackToBoneIndices.SequenceEqual(Container.bindings![0].transformTrackToBoneIndices))throw new InvalidDataException("HKX 저장 검증에 실패했습니다.");
  var comparison=MotionComparison.Compare(this,Load(path),edit,begin,end,editPose,validationRig,checkOptions??new());File.WriteAllText(path+".verification.json",System.Text.Json.JsonSerializer.Serialize(comparison,SongProject.Json));
 }
}
