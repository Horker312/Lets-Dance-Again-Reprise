using System.IO;
using System.Numerics;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using System.Windows.Media.Imaging;
using NiflySharp;
using NiflySharp.Blocks;
using NiflySharp.Structs;
using FacialPresets;
namespace FaceAssets;

public sealed class FacePart {
 public string Name="",Source="",TexturePath="",TextureReference="";public string[] TextureReferences=[];
 public Vector3[] Reference=[];public int[] Indices=[];public Matrix4x4 ToHead=Matrix4x4.Identity;
 internal Matrix4x4 OriginalWorld=Matrix4x4.Identity;internal bool HasOwnHeadBind;public TriFace? Tri;internal string? LastWeights;public bool SculptConfirmed,Alpha;public byte Threshold;public float Opacity=1;
 public MeshGeometry3D Mesh=new();public Material Material=new DiffuseMaterial(Brushes.Bisque);
 public override string ToString()=>Name;
}
public sealed class FaceScene {
 public List<FacePart> Parts{get;}=[];public List<string> Warnings{get;}=[];public string? TextureRoot;
 public Matrix4x4 GlobalToHead=Matrix4x4.Identity;public bool HasHeadBind;
 string signature="";public int GeometryUpdates{get;private set;}
 public static Point3D Point(Vector3 v)=>new(v.X,v.Z,-v.Y);
 public static Vector3 Vector(Point3D v)=>new((float)v.X,(float)-v.Z,(float)v.Y);
 public static FaceScene Load(string path,string? root=null){var scene=new FaceScene{TextureRoot=root};scene.AddNif(path);return scene;}
 public Model3DGroup CreateVisual(){var group=new Model3DGroup();foreach(var p in Parts)group.Children.Add(new GeometryModel3D(p.Mesh,p.Material){BackMaterial=p.Material});return group;}
 public void AddNif(string path){
  path=Path.GetFullPath(path);if(Parts.Any(p=>p.Source.Equals(path,StringComparison.OrdinalIgnoreCase)))throw new InvalidDataException("This NIF is already loaded; duplicate HeadParts were not added.");
  using var snapshot=NifGuard.Open(path);var file=new NifFile();if(file.Load(snapshot)!=0||!file.Valid||file.HasUnknownBlocks)throw new InvalidDataException("Unsupported or damaged NIF blocks.");
  var staged=new List<FacePart>();var localWarnings=new List<string>();Matrix4x4? globalBind=HasHeadBind?GlobalToHead:null;
  Matrix4x4 World(NiAVObject n){var seen=new HashSet<NiAVObject>();Matrix4x4 m=Matrix4x4.Identity;for(NiAVObject? at=n;at!=null;at=file.GetParentNode(at)){if(!seen.Add(at)||seen.Count>128)throw new InvalidDataException("Cyclic/deep NIF nodes.");m*=Transform(at.Rotation,at.Translation,at.Scale);}return m;}
  // A rigid HeadPart parented below an explicit Head node is already in that
  // file's skeleton space. Preserve its full hierarchy, not another NIF's bind.
  Matrix4x4? nodeBind=null;var heads=file.Blocks.OfType<NiNode>().Where(n=>n.Name.String=="NPC Head [Head]").ToList();
  if(heads.Count==1&&Matrix4x4.Invert(World(heads[0]),out var inverseHead)){nodeBind=inverseHead;globalBind??=nodeBind;}
  var shapes=file.Blocks.OfType<BSTriShape>().ToList();if(shapes.Count==0||shapes.Count+Parts.Count>32)throw new InvalidDataException("Only head BSTriShape/BSDynamicTriShape (up to 32 parts) are supported.");
  foreach(var s in shapes){try{
   if(s.VertexCount==0||s.VertexCount>65535||s.Triangles.Count>200000)throw new InvalidDataException("Invalid NIF geometry size.");
   var raw=s is BSDynamicTriShape dynamic&&dynamic.Vertices.Count==s.VertexCount?dynamic.Vertices.Select(v=>new Vector3(v.X,v.Y,v.Z)).ToArray():s.VertexPositions.ToArray();
   if(raw.Length!=s.VertexCount||raw.Any(v=>!Finite(v)||v.Length()>100000))throw new InvalidDataException("Invalid NIF vertex coordinates.");
   var indices=s.Triangles.SelectMany(t=>new[]{(int)t.V1,t.V2,t.V3}).ToArray();if(indices.Any(i=>i<0||i>=raw.Length))throw new InvalidDataException("Invalid NIF triangle index.");
   Matrix4x4? bind=null;
   if(file.GetBlock<NiSkinInstance>(s.SkinInstanceRef) is {} skin){var data=file.GetBlock<NiSkinData>(skin.Data);var bones=skin.Bones.Indices.ToArray();if(bones.Length>256||data==null||data.BoneList.Count!=bones.Length)throw new InvalidDataException("Invalid head skin data.");for(int i=0;i<bones.Length;i++){var bone=file.Blocks.ElementAtOrDefault(bones[i]) as NiNode;var name=bone?.Name.String??"";if(!IsHeadBone(name))throw new InvalidDataException("Body/armor skinning is excluded: "+name);if(name=="NPC Head [Head]"){var b=data.BoneList[i].SkinTransform;bind=Transform(b.Rotation,b.Translation,b.Scale);if(Matrix4x4.Invert(World(s),out var inv))globalBind??=inv*bind;}}
    localWarnings.Add(s.Name.String+": neutral mesh retained; rigid head attachment, no animated jaw/eye/neck skinning.");}
   var part=new FacePart{OriginalWorld=World(s),HasOwnHeadBind=bind!=null||nodeBind!=null,Name=s.Name.String,Source=Path.GetFullPath(path),Reference=raw,Indices=indices,ToHead=bind??(World(s)*(nodeBind??Matrix4x4.Identity)),Alpha=s.HasAlphaProperty};
   if(file.GetBlock<NiAlphaProperty>(s.AlphaPropertyRef) is {} ap)part.Threshold=(ap.Flags.Value&512)!=0?ap.Threshold:(byte)0;
   if(file.GetShader(s) is BSLightingShaderProperty shader){part.Opacity=float.IsFinite(shader.Alpha)?Math.Clamp(shader.Alpha,0,1):1;var set=file.GetBlock<BSShaderTextureSet>(shader.TextureSetRef);part.TextureReferences=set?.Textures.Select(t=>t.Content??"").ToArray()??[];part.TextureReference=part.TextureReferences.FirstOrDefault()??"";}
   var neutralBrush=new SolidColorBrush(Color.FromArgb((byte)(part.Opacity*255),206,176,153));neutralBrush.Freeze();var neutralMaterial=new DiffuseMaterial(neutralBrush);neutralMaterial.Freeze();part.Material=neutralMaterial;
   if(s.UVs.Count!=0&&s.UVs.Count!=raw.Length)throw new InvalidDataException("NIF UV/vertex counts differ.");
   part.Mesh.TriangleIndices=new Int32Collection(indices);part.Mesh.TextureCoordinates=new PointCollection(s.UVs.Select(uv=>{if(!float.IsFinite(uv.U)||!float.IsFinite(uv.V)||Math.Abs(uv.U)>10000||Math.Abs(uv.V)>10000)throw new InvalidDataException("Invalid NIF UV.");return new Point(uv.U,uv.V);}));
   if(s.Normals.Count==raw.Length)part.Mesh.Normals=new Vector3DCollection(s.Normals.Select(n=>{if(!Finite(n))throw new InvalidDataException("Invalid NIF normal.");var q=Vector3.TransformNormal(n,part.ToHead);return new Vector3D(q.X,q.Z,-q.Y);}));
   staged.Add(part);
  }catch(InvalidDataException e){localWarnings.Add(s.Name.String+": skipped HeadPart: "+e.Message);}}
  if(staged.Count==0)throw new InvalidDataException("No supported face HeadPart was found. "+string.Join(" / ",localWarnings));
  if(staged.Sum(p=>p.Reference.Length)+Parts.Sum(p=>p.Reference.Length)>200000)throw new InvalidDataException("Combined head vertex limit exceeded.");
  foreach(var p in staged){if(!p.HasOwnHeadBind&&globalBind!=null)p.ToHead=p.OriginalWorld*globalBind.Value;TryTexture(p,localWarnings);}
  if(staged.Concat(Parts).Where(p=>p.Material is DiffuseMaterial {Brush:ImageBrush}).Select(p=>((ImageBrush)((DiffuseMaterial)p.Material).Brush).ImageSource).Distinct().Sum(i=>i is BitmapSource b?(long)b.PixelWidth*b.PixelHeight*4:0)>128L*1024*1024)throw new InvalidDataException("Face texture budget exceeds 128 MiB.");Parts.AddRange(staged);Warnings.AddRange(localWarnings);if(globalBind!=null){foreach(var p in Parts.Where(p=>!p.HasOwnHeadBind)){p.ToHead=p.OriginalWorld*globalBind.Value;p.LastWeights=null;}}if(globalBind!=null){GlobalToHead=globalBind.Value;HasHeadBind=true;}else Warnings.Add("NPC Head bind unavailable: standalone NIF viewing only; Dance Studio keeps its simple head.");signature="";Apply(null);
 }
 public void SetTextureRoot(string root){if(!Directory.Exists(root))throw new DirectoryNotFoundException(root);TextureRoot=Path.GetFullPath(root);foreach(var p in Parts)TryTexture(p,Warnings);}
 void TryTexture(FacePart part,List<string> warnings){var path=ResolveTexture(part.Source,part.TextureReference,TextureRoot);if(path==null){warnings.Add(part.Name+": diffuse texture not found; specify DDS manually (tint slot is not diffuse). "+part.TextureReference);return;}try{SetTexture(part,path);}catch(Exception e){warnings.Add(part.Name+": "+e.Message);}}
 public void SetTexture(FacePart p,string path){var image=DdsTexture.Load(path,p.Alpha,p.Threshold);var brush=new ImageBrush(image){Opacity=p.Opacity,ViewportUnits=BrushMappingMode.Absolute,Viewport=new Rect(0,0,1,1),TileMode=TileMode.Tile,Stretch=Stretch.Fill};brush.Freeze();var material=new DiffuseMaterial(brush);material.Freeze();long cost=(long)image.PixelWidth*image.PixelHeight*4;if(Parts.Where(other=>other!=p&&other.Material is DiffuseMaterial {Brush:ImageBrush}).Select(other=>((ImageBrush)((DiffuseMaterial)other.Material).Brush).ImageSource).Distinct().Sum(i=>i is BitmapSource b?(long)b.PixelWidth*b.PixelHeight*4:0)+cost>128L*1024*1024)throw new InvalidDataException("Face texture budget exceeds 128 MiB.");p.Material=material;p.TexturePath=Path.GetFullPath(path);Warnings.RemoveAll(w=>w.StartsWith(p.Name+": diffuse texture not found"));}
 public static string? ResolveTexture(string nif,string reference,string? root){if(string.IsNullOrWhiteSpace(reference))return null;string rel=reference.Replace('\\','/');if(Path.IsPathRooted(rel)||rel.Split('/').Any(p=>p==".."||p.Contains(':')))return null;if(rel.StartsWith("Data/",StringComparison.OrdinalIgnoreCase))rel=rel[5..];if(!rel.StartsWith("textures/",StringComparison.OrdinalIgnoreCase))rel="textures/"+rel;var roots=new List<string>();if(root!=null)roots.Add(root);for(var dir=new DirectoryInfo(Path.GetDirectoryName(nif)!);dir!=null;dir=dir.Parent)if(dir.Name.Equals("meshes",StringComparison.OrdinalIgnoreCase)){if(dir.Parent!=null)roots.Add(dir.Parent.FullName);break;}roots.Add(Path.GetDirectoryName(nif)!);foreach(var folder in roots){string target=Path.GetFullPath(Path.Combine(folder,Path.GetFileName(folder).Equals("textures",StringComparison.OrdinalIgnoreCase)?rel[9..]:rel));if(target.StartsWith(Path.GetFullPath(folder).TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase)&&File.Exists(target))return target;}return null;}
 public string Compatibility(FacePart part,TriFace tri){if(part.Reference.Length!=tri.Vertices.Length)return "Vertex count differs; morph connection rejected.";if(!Topology(part.Indices).SequenceEqual(Topology(tri.Indices)))return "Indexed topology differs; morph connection rejected (no vertex reordering).";return part.Reference.Where((p,i)=>Vector3.Distance(p,new Vector3((float)tri.Vertices[i].X,(float)tri.Vertices[i].Y,(float)tri.Vertices[i].Z))>.005).Any()?"SCULPT: topology matches but reference positions differ. Confirm the same HeadPart vertex correspondence; count alone is not proof.":"EXACT: indexed topology and reference positions match.";}
 public void AttachTri(FacePart part,TriFace tri,bool confirmSculpt=false){string check=Compatibility(part,tri);if(!check.StartsWith("EXACT")&&!(check.StartsWith("SCULPT")&&confirmSculpt))throw new InvalidDataException(check);if(part.Tri!=null){var copy=tri;var existing=part.Tri;if(!existing.SameTopology(copy))throw new InvalidDataException("TRI extension topology differs.");if(copy.Morphs.Keys.Any(existing.Morphs.ContainsKey))throw new InvalidDataException("Duplicate TRI morph name.");if(!existing.SameReference(copy))throw new InvalidDataException("TRI extension reference differs from the existing TRI.");existing.AddMorphs(copy);}else part.Tri=tri;part.SculptConfirmed=confirmSculpt;part.LastWeights=null;signature="";}
 public void Apply(Preset? preset){var morphs=preset==null?[]:MorphSelection.Resolve(preset,true);string next=string.Join(";",morphs.Select(m=>m.Backend+":"+m.Id+":"+m.Name+"="+m.Strength.ToString("R",System.Globalization.CultureInfo.InvariantCulture)));if(signature==next&&Parts.All(p=>p.Mesh.Positions.Count>0))return;signature=next;foreach(var p in Parts){var relevant=morphs.Where(m=>p.Tri!=null&&p.Tri.Morphs.ContainsKey((m.Backend=="MFG"?TriFace.MfgName(m.Id):m.Name)??"" )).ToList();string state=string.Join(";",relevant.Select(m=>m.Backend+":"+m.Id+":"+m.Name+"="+m.Strength.ToString("R",System.Globalization.CultureInfo.InvariantCulture)));if(p.LastWeights==state&&p.Mesh.Positions.Count>0)continue;p.LastWeights=state;var vertices=(Vector3[])p.Reference.Clone();foreach(var m in relevant){string? name=m.Backend=="MFG"?TriFace.MfgName(m.Id):m.Name;if(name==null||p.Tri==null||!p.Tri.Morphs.TryGetValue(name,out var deltas))continue;float gain=(float)(m.Strength/100);for(int i=0;i<vertices.Length;i++)vertices[i]+=new Vector3((float)deltas[i].X,(float)deltas[i].Y,(float)deltas[i].Z)*gain;}p.Mesh.Normals=new Vector3DCollection();p.Mesh.Positions=new Point3DCollection(vertices.Select(v=>Point(Vector3.Transform(v,p.ToHead))));}GeometryUpdates++;}
 public static bool Finite(Vector3 v)=>float.IsFinite(v.X)&&float.IsFinite(v.Y)&&float.IsFinite(v.Z);
 static bool IsHeadBone(string n)=>n is "NPC Head [Head]" or "NPC Neck [Neck]" or "NPC Spine2 [Spn2]"||n.Contains("Eye",StringComparison.OrdinalIgnoreCase)||n.Contains("Jaw",StringComparison.OrdinalIgnoreCase)||n.Contains("Hair",StringComparison.OrdinalIgnoreCase);
 static string[] Topology(int[] indices)=>Enumerable.Range(0,indices.Length/3).Select(i=>{int a=indices[i*3],b=indices[i*3+1],c=indices[i*3+2];return a<=b&&a<=c?$"{a},{b},{c}":b<=a&&b<=c?$"{b},{c},{a}":$"{c},{a},{b}";}).OrderBy(x=>x,StringComparer.Ordinal).ToArray();
 static Matrix4x4 Transform(Matrix33 r,Vector3 t,float scale){if(!Finite(t)||!float.IsFinite(scale)||Math.Abs(scale)<.00001||Math.Abs(scale)>1000)throw new InvalidDataException("Invalid NIF transform.");if(new[]{r.M11,r.M12,r.M13,r.M21,r.M22,r.M23,r.M31,r.M32,r.M33}.Any(x=>!float.IsFinite(x)))throw new InvalidDataException("Non-finite NIF rotation.");var m=new Matrix4x4(r.M11,r.M21,r.M31,0,r.M12,r.M22,r.M32,0,r.M13,r.M23,r.M33,0,0,0,0,1);if(!Matrix4x4.Invert(m,out _))throw new InvalidDataException("Singular NIF rotation.");return Matrix4x4.CreateScale(scale)*m*Matrix4x4.CreateTranslation(t);}
}


