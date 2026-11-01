using System.Numerics;
using System.Text.Json;
namespace FaceAssets;

public static class DefaultFaceMorphs {
 public static int Apply(JsonElement morphs,IReadOnlyList<TriFace> sources,Vector3[] neutral,List<string> notes,string part){
  if(morphs.ValueKind!=JsonValueKind.Object||!morphs.TryGetProperty("default",out var d)||d.ValueKind!=JsonValueKind.Object)return 0;
  // The jslot stores the engine's 19-option/4-preset numeric arrays, not named TRI keys.
  // Without the current race's engine FaceMorphList/FacePresetList, applying similar
  // names would silently sculpt a different face. An explicitly confirmed exported
  // face NIF can supply the already evaluated neutral geometry instead.
  bool used=false;
  foreach(string key in new[]{"morphs","presets"})if(d.TryGetProperty(key,out var values)&&values.ValueKind==JsonValueKind.Array){if(values.GetArrayLength()>64)throw new System.IO.InvalidDataException("Default morph array limit");used|=values.EnumerateArray().Any(v=>v.ValueKind==JsonValueKind.Number&&v.TryGetDouble(out var n)&&n!=0&&n!=255&&n!=uint.MaxValue&&Math.Abs(n)<1e20);}
  if(used)notes.Add("Default face sliders/presets not reconstructed for "+part+": numeric engine/race mapping unavailable. Select a compatible RaceMenu exported face NIF for evaluated geometry.");return 0;
 }
}
