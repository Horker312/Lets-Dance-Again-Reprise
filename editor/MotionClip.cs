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
  if(q.LengthSquared()<.0001)throw new InvalidDataException(UiText.T("t0254"));
  return new(new(m.M11,m.M12,m.M13),Quaternion.Normalize(q),new(m.M31,m.M32,m.M33));
 }
 public Matrix4x4 Matrix()=>new(T.X,T.Y,T.Z,0,R.X,R.Y,R.Z,R.W,S.X,S.Y,S.Z,0,0,0,0,0);
 public static Pose Blend(Pose a,Pose b,float t)=>new(Vector3.Lerp(a.T,b.T,t),Quaternion.Normalize(Quaternion.Slerp(a.R,b.R,t)),Vector3.Lerp(a.S,b.S,t));
}
public sealed class Rig {
 public const string BuiltinId="builtin://skyrim-xpmse";
 public static bool IsBuiltin(string? path)=>string.IsNullOrWhiteSpace(path)||path==BuiltinId;
 public static string ResolveReference(string? path)=>IsBuiltin(path)?BuiltinId:System.IO.Path.GetFullPath(path!);
 public static byte[] SourceBytes(string path){if(!IsBuiltin(path))return File.ReadAllBytes(path);using var stream=typeof(Rig).Assembly.GetManifestResourceStream("DanceStudio.DefaultSkeleton.hkx")??throw new InvalidDataException(UiText.T("t0255"));using var memory=new MemoryStream();stream.CopyTo(memory);return memory.ToArray();}

 public string Fingerprint=>Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(System.Text.Json.JsonSerializer.Serialize(new{Name,Names,Parents,FloatSlots,reference=Reference.Select(p=>new{t=new[]{p.T.X,p.T.Y,p.T.Z},r=new[]{p.R.X,p.R.Y,p.R.Z,p.R.W},s=new[]{p.S.X,p.S.Y,p.S.Z}})}))));
 public string? SourceReference;public string SourceHash="";
 public void EnsureSourceUnchanged(){if(SourceReference!=null&&Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(SourceBytes(SourceReference)))!=SourceHash)throw new IOException(UiText.T("t0256"));}
 public string Name="";public string[] FloatSlots=[];
 public string[] Names=[]; public short[] Parents=[]; public Pose[] Reference=[];
 public static Rig Load(string path) {
  path=ResolveReference(path);if(!IsBuiltin(path)&&new FileInfo(path).Length>256L*1024*1024)throw new InvalidDataException(UiText.T("t0257"));var bytes=SourceBytes(path);using var skeletonStream=new MemoryStream(bytes,false);var f=MotionClip.ReadStream(skeletonStream);var candidates=f.Container.skeletons?.Where(x=>x.bones.Count>0&&!x.name.StartsWith("Ragdoll",StringComparison.OrdinalIgnoreCase)).ToArray()??[];if(candidates.Length!=1)throw new InvalidDataException(UiText.T("t0258"));var s=candidates[0];
  var r=new Rig{SourceReference=path,SourceHash=Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(bytes)),Name=s.name,FloatSlots=s.floatSlots.ToArray(),Names=s.bones.Select(b=>b.name).ToArray(),Parents=s.parentIndices.ToArray(),Reference=s.referencePose.Select(Pose.FromMatrix).ToArray()};
  if(r.Names.Length!=r.Parents.Length||r.Names.Length!=r.Reference.Length||r.Names.Distinct().Count()!=r.Names.Length)throw new InvalidDataException(UiText.T("t0259"));
  for(int i=0;i<r.Parents.Length;i++)if(r.Parents[i]>=i||r.Parents[i]<-1)throw new InvalidDataException(UiText.T("t0260"));
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
 public void EnsureSourceUnchanged(){if(!File.Exists(Path)||Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(File.ReadAllBytes(Path)))!=SourceHash)throw new IOException(UiText.T("t0261"));}

 public string Path=""; public double Duration; public int Tracks; public int Frames=>Samples.Length;
 float[][]? staticFloatBlocks;int staticFloatSpan;
 public Pose[][] Samples=[];public int[] BoneMap=[];public float[][] FloatSamples=[];
 public hkRootLevelContainer Root=null!;public hkaAnimationContainer Container=null!;public HKXHeader Header=null!;
 public string SupportSummary=>Container==null?UiText.T("t0262"):UiText.F("t0265", Animation.GetType().Name, Frames, Tracks, Animation.numberOfFloatTracks, (Animation.extractedMotion==null?UiText.T("t0263"):UiText.T("t0264")));
 public int AnimationIndex,BindingIndex;public bool ImplicitBinding;public string? IdentityRigProof;
 public hkaAnimation Animation=>Container.animations![AnimationIndex];
 Rig? validatedRig;public void ValidateRig(Rig rig){if(validatedRig==rig)return;HkxBinding.ValidateRig(this,rig);validatedRig=rig;}
 public static (hkRootLevelContainer Root,hkaAnimationContainer Container,HKXHeader Header) Read(string path) {
  if(new FileInfo(path).Length>256L*1024*1024)throw new InvalidDataException(UiText.T("t0266"));
  using var f=File.OpenRead(path);return ReadStream(f);
 }
 public static (hkRootLevelContainer Root,hkaAnimationContainer Container,HKXHeader Header) ReadStream(Stream f) {
  if(f.Length>256L*1024*1024)throw new InvalidDataException(UiText.T("t0266"));
  var reader=new PackFileDeserializer();
  var root=reader.Deserialize(new BinaryReaderEx(f)) as hkRootLevelContainer??throw new InvalidDataException(UiText.T("t0267"));
  var containers=root.namedVariants.Select(v=>v!.variant).OfType<hkaAnimationContainer>().ToArray();if(containers.Length!=1)throw new InvalidDataException(UiText.F("t0268", containers.Length));return(root,containers[0],reader._header);
 }
 static Func<object,T> Access<T>(System.Type type,string name) {
  var x=Expression.Parameter(typeof(object));var member=Expression.PropertyOrField(Expression.Convert(x,type),name);
  return Expression.Lambda<Func<object,T>>(member,x).Compile();
 }
 public static MotionClip Load(string path,int? bindingIndex=null,string? identityRigProof=null) {
  using var input=File.OpenRead(path);if(input.Length>256L*1024*1024)throw new InvalidDataException(UiText.T("t0269"));using var memory=new MemoryStream();input.CopyTo(memory);byte[] snapshot=memory.ToArray();using var source=new MemoryStream(snapshot,false);var f=ReadStream(source);var selection=HkxBinding.Select(f.Container,bindingIndex);var a=f.Container.animations![selection.AnimationIndex];var b=f.Container.bindings![selection.BindingIndex];
  if(a.duration<=0||!float.IsFinite(a.duration)||a.duration>1800||a.numberOfTransformTracks<1||a.numberOfTransformTracks>512)throw new InvalidDataException(UiText.T("t0270"));
  var result=new MotionClip{Path=System.IO.Path.GetFullPath(path),Duration=a.duration,Tracks=a.numberOfTransformTracks,Root=f.Root,Container=f.Container,Header=f.Header,sourceSnapshot=snapshot,SourceHash=Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(snapshot)),AnimationIndex=selection.AnimationIndex,BindingIndex=selection.BindingIndex,ImplicitBinding=b.transformTrackToBoneIndices.Count==0,IdentityRigProof=identityRigProof};
  if(b.transformTrackToBoneIndices.Count!=0&&b.transformTrackToBoneIndices.Count!=result.Tracks)throw new InvalidDataException(UiText.T("t0271"));
  result.BoneMap=result.ImplicitBinding?Enumerable.Range(0,result.Tracks).ToArray():b.transformTrackToBoneIndices.Select(x=>(int)x).ToArray();HkxBinding.ValidateIndices(result.BoneMap,result.Tracks);
  if(a is hkaInterleavedUncompressedAnimation u) {
   if(u.transforms.Count%result.Tracks!=0||u.transforms.Count/result.Tracks<2||u.transforms.Count>4000000)throw new InvalidDataException(UiText.T("t0272"));
   int n=u.transforms.Count/result.Tracks;result.Samples=new Pose[n][];
   for(int i=0;i<n;i++)result.Samples[i]=u.transforms.Skip(i*result.Tracks).Take(result.Tracks).Select(Pose.FromMatrix).ToArray();
  } else if(a is hkaSplineCompressedAnimation spline) {
   var num=spline.GetType().GetProperty("numFrames")?.GetValue(spline);
   if(num!=null&&Convert.ToInt64(num)*result.Tracks>4000000)throw new InvalidDataException(UiText.T("t0273"));
   var decoder=Assembly.LoadFrom(System.IO.Path.Combine(AppContext.BaseDirectory,"Matcher.dll")).GetType("MotionEditor.Core.SplinePreview")!.GetMethod("Decode",BindingFlags.Static|BindingFlags.Public|BindingFlags.NonPublic)!;
   var decoded=(Array)(decoder.Invoke(null,new object[]{a})??throw new InvalidDataException(UiText.T("t0274")));
   if(decoded.Length<2||decoded.Length>60000)throw new InvalidDataException(UiText.T("t0275"));
   var first=(Array)decoded.GetValue(0)!;var type=first.GetValue(0)!.GetType();
   var t=Access<Vector3>(type,"Translation");var q=Access<Quaternion>(type,"Rotation");var s=Access<Vector3>(type,"Scale");
   result.Samples=new Pose[decoded.Length][];
   for(int i=0;i<decoded.Length;i++) {
    var row=(Array)decoded.GetValue(i)!;var poses=new Pose[result.Tracks];
    for(int j=0;j<poses.Length;j++){var v=row.GetValue(j)!;poses[j]=new(t(v),Quaternion.Normalize(q(v)),s(v));}
    result.Samples[i]=poses;
   }
  } else throw new NotSupportedException(UiText.T("t0276"));
  foreach(var row in result.Samples)foreach(var p in row)
   if(!float.IsFinite(p.T.X+p.T.Y+p.T.Z+p.R.X+p.R.Y+p.R.Z+p.R.W+p.S.X+p.S.Y+p.S.Z))throw new InvalidDataException(UiText.T("t0277"));
  int floats=a.numberOfFloatTracks;
  if(floats>0) {
   if(a is hkaInterleavedUncompressedAnimation plain) {
    if(plain.floats.Count!=result.Frames*floats)throw new InvalidDataException(UiText.T("t0278"));
    result.FloatSamples=Enumerable.Range(0,result.Frames).Select(i=>plain.floats.Skip(i*floats).Take(floats).ToArray()).ToArray();
   } else if(a is hkaSplineCompressedAnimation c) {
    // Static float channels per block (the legacy LDA camera files use this
    // layout). Reject spline-valued channels rather than silently discarding.
    var blocks=new List<float[]>();
    if(c.endian!=0||c.numBlocks<1||c.maxFramesPerBlock<2||c.blockOffsets.Count!=c.numBlocks||c.floatBlockOffsets.Count!=c.numBlocks)throw new NotSupportedException(UiText.T("t0279"));
    for(int block=0;block<c.numBlocks;block++) {
     long start64=c.blockOffsets[block],end64=block+1<c.numBlocks?c.blockOffsets[block+1]:c.data.Count,offset64=start64+c.floatBlockOffsets[block];
     if(start64<0||end64<=start64||end64>c.data.Count||start64+result.Tracks*4+floats>end64||offset64<start64||offset64+floats*4>end64)throw new InvalidDataException(UiText.T("t0280"));int start=(int)start64,offset=(int)offset64;
     var masks=c.data.Skip(start+result.Tracks*4).Take(floats).ToArray();
     if(masks.Length!=floats||masks.Any(x=>x!=3)||offset+floats*4>c.data.Count)throw new NotSupportedException(UiText.F("t0281", string.Join(",",masks.Select(x=>x.ToString("X2")))));
     var values=Enumerable.Range(0,floats).Select(i=>BitConverter.ToSingle(c.data.Skip(offset+i*4).Take(4).ToArray())).ToArray();
     if(values.Any(x=>!float.IsFinite(x)))throw new InvalidDataException(UiText.T("t0282"));blocks.Add(values);
    }
    result.staticFloatBlocks=blocks.ToArray();result.staticFloatSpan=(int)c.maxFramesPerBlock-1;
    result.FloatSamples=Enumerable.Range(0,result.Frames).Select(i=>blocks[Math.Min(i/Math.Max(1,(int)c.maxFramesPerBlock-1),blocks.Count-1)].ToArray()).ToArray();
   }
  }
  if(b.floatTrackToFloatSlotIndices.Count!=0&&(b.floatTrackToFloatSlotIndices.Count!=floats||b.floatTrackToFloatSlotIndices.Any(x=>x<0)||b.floatTrackToFloatSlotIndices.Distinct().Count()!=floats))throw new InvalidDataException(UiText.T("t0283"));
  if(result.FloatSamples.Any(row=>row.Any(x=>!float.IsFinite(x))))throw new InvalidDataException(UiText.T("t0284"));
  foreach(var track in a.annotationTracks)foreach(var ev in track.annotations)if(!float.IsFinite(ev.time)||ev.time<0||ev.time>a.duration+.0001)throw new InvalidDataException(UiText.T("t0285"));
  return result;
 }
 Rig? editRig;MotionEdits? edits;
 public MotionEdits Edits(Rig rig){if(edits==null||editRig!=rig){edits=new MotionEdits(rig,this);editRig=rig;}return edits;}
 public Pose[] EditedWorld(Rig rig,double time,bool mirror,double yaw)=>!mirror&&yaw==0?World(rig,time):Edits(rig).World(Edits(rig).Dance(time,mirror,yaw));
 public float[] FloatsAt(double time) {
  if(!double.IsFinite(time))throw new InvalidDataException(UiText.T("t0286"));
  if(FloatSamples.Length==0)return [];
  if(staticFloatBlocks!=null){double frame=Math.Clamp(time/Duration,0,1)*(Frames-1);return staticFloatBlocks[Math.Min((int)Math.Floor(frame/staticFloatSpan+1e-7),staticFloatBlocks.Length-1)].ToArray();}
  double f=Math.Clamp(time/Duration,0,1)*(Frames-1);int lo=(int)f,hi=Math.Min(lo+1,Frames-1);float blend=(float)(f-lo);
  return FloatSamples[lo].Zip(FloatSamples[hi],(a,b)=>a+(b-a)*blend).ToArray();
 }
 public Pose[] At(double time) {
  if(!double.IsFinite(time))throw new InvalidDataException(UiText.T("t0287"));
  double f=Math.Clamp(time/Duration,0,1)*(Frames-1);int lo=(int)f,hi=Math.Min(lo+1,Frames-1);float blend=(float)(f-lo);
  return Enumerable.Range(0,Tracks).Select(i=>Pose.Blend(Samples[lo][i],Samples[hi][i],blend)).ToArray();
 }
 public Pose[] World(Rig rig,double time) {
  ValidateRig(rig);var local=rig.Reference.ToArray();var pose=At(time);
  for(int i=0;i<Tracks;i++){int b=BoneMap[i];if(b>=0&&b<local.Length)local[b]=pose[i];else throw new InvalidDataException(UiText.T("t0288"));}
  return rig.World(local);
 }
 public void Bake(string path,TrackEdit edit,double begin,double end,double fps,Func<double,Pose[]>? editPose=null,Rig? validationRig=null,MotionCheckOptions? checkOptions=null,bool preserveOriginal=false) {
  edit.Validate(Duration);if(!double.IsFinite(begin)||!double.IsFinite(end)||begin<0)throw new InvalidDataException(UiText.T("t0289"));EnsureSourceUnchanged();double duration=end-begin;
  if(duration<=0||duration>1800||!double.IsFinite(fps)||fps is <1 or >120)throw new InvalidDataException(UiText.T("t0290"));
  if(!edit.HoldEnd&&end>edit.TimelineEnd+.0001)throw new InvalidDataException(UiText.T("t0291"));
  if(System.IO.Path.GetFullPath(path).Equals(System.IO.Path.GetFullPath(Path),StringComparison.OrdinalIgnoreCase))throw new IOException(UiText.T("t0292"));
  if(validationRig!=null)ValidateRig(validationRig);
  if(Container.animations.Count!=1||Container.bindings.Count!=1)throw new NotSupportedException(UiText.T("t0293"));
  bool exact=edit.In==0&&Math.Abs(edit.Out-Duration)<1e-6&&edit.Start==0&&edit.Speed==1&&begin==0&&Math.Abs(end-Duration)<1e-6&&editPose==null;
  if(Animation.extractedMotion!=null){if(!exact)throw new NotSupportedException(UiText.T("t0294"));File.WriteAllBytes(path,sourceSnapshot);File.WriteAllText(path+".preservation.json",System.Text.Json.JsonSerializer.Serialize(new{rootMotion="exact original byte copy; not resampled; game actor movement unverified",source=Path}));return;}
  if(exact&&(preserveOriginal||Math.Abs((Frames-1)/Duration-fps)<.0001)){File.WriteAllBytes(path,sourceSnapshot);var preserved=System.Text.Json.JsonSerializer.SerializeToNode(MotionComparison.Compare(this,Load(path),edit,begin,end,null,validationRig,checkOptions??new()),SongProject.Json)!;preserved["preservationMode"]="byte-identical original; no resampling";File.WriteAllText(path+".verification.json",preserved.ToJsonString(SongProject.Json));return;}
  int count=Math.Max(2,(int)Math.Round(duration*fps)+1);
  if((long)count*Tracks>4000000)throw new InvalidDataException(UiText.T("t0295"));
  using var snapshotStream=new MemoryStream(sourceSnapshot,false);var f=ReadStream(snapshotStream);var a=f.Container.animations![AnimationIndex];var transforms=new List<Matrix4x4>(count*Tracks);var outputFloats=new List<float>();
  for(int i=0;i<count;i++) {
   double source=Math.Clamp(edit.SourceTime(begin+duration*i/(count-1)),edit.In,edit.Out);
   var row=editPose==null?At(source):editPose(source);if(row.Length!=Tracks)throw new InvalidDataException(UiText.T("t0296"));transforms.AddRange(row.Select(p=>p.Matrix()));
   outputFloats.AddRange(FloatsAt(source));
  }
  var annotations=a.annotationTracks.Select(track=>new hkaAnnotationTrack{trackName=track.trackName,annotations=track.annotations.Where(x=>x.time>=edit.In&&x.time<=edit.Out).Select(x=>new hkaAnnotationTrackAnnotation{time=(float)((x.time-edit.In)/edit.Speed+edit.Start-begin),text=x.text}).Where(x=>x.time>=0&&x.time<=duration).ToList()}).ToList();
  var output=new hkaInterleavedUncompressedAnimation{type=1,duration=(float)duration,numberOfTransformTracks=Tracks,numberOfFloatTracks=a.numberOfFloatTracks,annotationTracks=annotations,transforms=transforms,floats=outputFloats};
  f.Container.animations[0]=output;f.Container.bindings![0].animation=output;
  var serializer=new PackFileSerializer();using(var stream=File.Create(path))serializer.Serialize(f.Root,new BinaryWriterEx(stream),f.Header);
  var check=Read(path).Container.animations![0];
  if(Math.Abs(check.duration-duration)>.0001||check.numberOfTransformTracks!=Tracks||!Read(path).Container.bindings![0].transformTrackToBoneIndices.SequenceEqual(Container.bindings![0].transformTrackToBoneIndices))throw new InvalidDataException(UiText.T("t0297"));
  var comparison=MotionComparison.Compare(this,Load(path),edit,begin,end,editPose,validationRig,checkOptions??new());File.WriteAllText(path+".verification.json",System.Text.Json.JsonSerializer.Serialize(comparison,SongProject.Json));
 }
}
