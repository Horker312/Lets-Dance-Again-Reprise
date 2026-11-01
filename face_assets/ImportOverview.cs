namespace FaceAssets;
public static class ImportOverview {
 public sealed record Item(string Label,string State,string Detail);
 public static List<Item> Describe(FaceScene scene,IEnumerable<string> messages){
  var notes=messages.ToList();bool Part(uint type)=>scene.Parts.Any(p=>p.HeadPartType==type&&p.Reference.Length>0&&!p.Mesh.Bounds.IsEmpty&&p.Opacity>0);
  bool textured(uint type)=>scene.Parts.Where(p=>p.HeadPartType==type).All(p=>p.Diffuse!=null);
  string Details(uint type)=>string.Join(" / ",scene.Parts.Where(p=>p.HeadPartType==type).Select(p=>p.HeadPartId+" → "+p.LogicalModel));
  bool incomplete=notes.Any(n=>n.Contains("not reconstructed")||n.Contains("Sculpt not applied")||n.Contains("correspondence not proven")||n.Contains("mapping unavailable")||n.Contains("Custom TRI morph unavailable"));
  var all=scene.Parts.Where(p=>p.Tri!=null).SelectMany(p=>p.Tri!.Morphs.Keys).ToList();
  return [new("얼굴",Part(1)?"연결됨":"실패",Details(1)),new("눈",Part(2)?(textured(2)?"메시·텍스처 연결됨":"텍스처 누락"):"누락",Part(2)?Details(2):string.Join(" / ",notes.Where(n=>n.Contains("HDPT unavailable")||n.Contains("Runtime HeadPart")))),new("머리카락",Part(3)?"연결됨":"프리셋에 없거나 누락",Details(3)),new("텍스처",scene.Parts.All(p=>p.Diffuse!=null)?"연결됨 · 재질은 근사":"부분 적용",string.Join(" / ",scene.Parts.Where(p=>p.Diffuse==null).Select(p=>p.Name+": "+p.TextureReference))),new("얼굴형",scene.ExportedNifPath!=null?"내보낸 NIF 적용 · 부분 확인":incomplete?"일부 변형 누락":"지원 변형 적용",scene.ExportedNifPath??string.Join(" / ",notes.Where(n=>n.Contains("not reconstructed")||n.Contains("Sculpt not applied")).Take(3))),new("MFG 표정",all.Any(n=>n is "Aah" or "BlinkLeft" or "MoodHappy")?"TRI 연결됨":"제한 있음","눈동자·턱 노드 회전과 게임 내 합성은 별도 확인"),new("MFEE 표정",all.Any(n=>n.StartsWith("Mood")&&n is not ("MoodNeutral" or "MoodHappy" or "MoodSad" or "MoodAnger" or "MoodFear" or "MoodSurprise" or "MoodPuzzled" or "MoodDisgusted"))?"확장 TRI 연결됨":"제한 있음","게임 API 등록 여부는 미검증")];
 }
}
