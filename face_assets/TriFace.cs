using System.IO;
using System.Text;
using System.Windows.Media.Media3D;
namespace FaceAssets;
public sealed class TriFace {
 public Point3D[] Vertices {get;private set;}=[];public int[] Indices {get;private set;}=[];
 public Dictionary<string,Vector3D[]> Morphs {get;}=new(StringComparer.OrdinalIgnoreCase);
 public static TriFace Read(string path){
  if(new FileInfo(path).Length>64*1024*1024)throw new InvalidDataException("TRI 파일이 너무 큽니다.");using var r=new BinaryReader(File.OpenRead(path));
  if(Encoding.ASCII.GetString(r.ReadBytes(8))!="FRTRI003")throw new InvalidDataException("FRTRI003 얼굴 TRI만 지원합니다. NIF/PIRT/압축 파일은 지원하지 않습니다.");
  int Count(int max){int n=r.ReadInt32();if(n<0||n>max)throw new InvalidDataException("TRI 개수가 잘못되었습니다.");return n;}
  float Float(){var f=r.ReadSingle();if(!float.IsFinite(f)||Math.Abs(f)>100000)throw new InvalidDataException("잘못된 TRI 좌표입니다.");return f;}
  int v=Count(100000),t=Count(200000),q=Count(100000),lv=Count(100000),ls=Count(100000),uv=Count(500000),ext=r.ReadInt32(),md=Count(512),ms=Count(512),k=Count(100000);
  if(v==0||lv!=0||ls!=0||ms!=0||k!=0||(long)v*md>2000000)throw new InvalidDataException("라벨/정적 모프 TRI 또는 과도한 데이터는 지원하지 않습니다.");if(r.ReadBytes(16).Length!=16)throw new EndOfStreamException();
  var face=new TriFace{Vertices=new Point3D[v]};for(int i=0;i<v;i++)face.Vertices[i]=new(Float(),Float(),Float());
  int Index(){int n=r.ReadInt32();if(n<0||n>=v)throw new InvalidDataException("TRI 정점 인덱스가 잘못되었습니다.");return n;}
  var indices=new List<int>();for(int i=0;i<t;i++){indices.Add(Index());indices.Add(Index());indices.Add(Index());}for(int i=0;i<q;i++){int a=Index(),b=Index(),c=Index(),d=Index();indices.AddRange([a,b,c,a,c,d]);}face.Indices=indices.ToArray();
  if((ext&1)!=0){long skip=(long)uv*8+(long)t*12+(long)q*16;if(skip>r.BaseStream.Length-r.BaseStream.Position)throw new EndOfStreamException();r.BaseStream.Seek(skip,SeekOrigin.Current);}
  for(int i=0;i<md;i++){int n=Count(1024);if(n<1)throw new InvalidDataException("빈 모프 이름입니다.");var bytes=r.ReadBytes(n);if(bytes.Length!=n)throw new EndOfStreamException();string name=new UTF8Encoding(false,true).GetString(bytes).TrimEnd('\0');if(name.Length==0||face.Morphs.ContainsKey(name))throw new InvalidDataException("빈 이름/중복 모프입니다.");float scale=Float();var delta=new Vector3D[v];for(int j=0;j<v;j++)delta[j]=new(r.ReadInt16()*scale,r.ReadInt16()*scale,r.ReadInt16()*scale);face.Morphs.Add(name,delta);}
  if(r.BaseStream.Position!=r.BaseStream.Length)throw new InvalidDataException("지원하지 않는 TRI 후속 데이터입니다.");return face;
 }
 public bool SameTopology(TriFace extra)=>Vertices.Length==extra.Vertices.Length&&Indices.SequenceEqual(extra.Indices);
 public bool SameReference(TriFace extra)=>SameTopology(extra)&&!Vertices.Where((v,i)=>(v-extra.Vertices[i]).Length>.001).Any();
 public void AddMorphs(TriFace extra,bool allowReferenceDifference=false){if(!SameTopology(extra)||(!allowReferenceDifference&&!SameReference(extra)))throw new InvalidDataException("기본 정점·순서·면이 다른 TRI입니다. 호환성을 추정하지 않습니다.");if(extra.Morphs.Keys.Any(Morphs.ContainsKey))throw new InvalidDataException("모프 이름이 중복됩니다.");foreach(var kv in extra.Morphs)Morphs.Add(kv.Key,kv.Value);}
 public Point3D[] Evaluate(IEnumerable<(string name,double weight)> weights){var result=(Point3D[])Vertices.Clone();foreach(var (name,w) in weights){if(!double.IsFinite(w)||!Morphs.TryGetValue(name,out var delta))continue;double weight=Math.Clamp(w,0,1);for(int i=0;i<result.Length;i++)result[i]+=delta[i]*weight;}return result;}
 public static string? MfgName(string id){var parts=id.Split(':');if(parts.Length!=2||!int.TryParse(parts[1],out int n)||n<0)return null;
  string[] names=parts[0] switch {
   "expression"=>["DialogueAnger","DialogueFear","DialogueHappy","DialogueSad","DialogueSurprise","DialoguePuzzled","DialogueDisgusted","MoodNeutral","MoodAnger","MoodFear","MoodHappy","MoodSad","MoodSurprise","MoodPuzzled","MoodDisgusted","CombatAnger","CombatShout"],
   "modifier"=>["BlinkLeft","BlinkRight","BrowDownLeft","BrowDownRight","BrowInLeft","BrowInRight","BrowUpLeft","BrowUpRight","LookDown","LookLeft","LookRight","LookUp","SquintLeft","SquintRight","HeadPitch","HeadRoll","HeadYaw"],
   "phoneme"=>["Aah","BigAah","BMP","ChJSh","DST","Eee","Eh","FV","I","K","N","Oh","OohQ","R","Th","W"],_=>[]};return n<names.Length?names[n]:null;
 }
}
