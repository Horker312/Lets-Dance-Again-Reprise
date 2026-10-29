using System.IO;
namespace DanceStudio;
public static class FovPolicy {
 public static double Valid(double value){if(!double.IsFinite(value)||value<5||value>140)throw new InvalidDataException("FOV는 5~140도의 유한한 값이어야 합니다.");return value;}
 // No verified HKX FOV binding/unit/interpolation contract was found in the
 // audited LDA files. Float channel count or numerical range is not evidence.
 public static bool HasVerifiedSource(MotionClip camera)=>false;
 public static string Analysis(MotionClip camera)=>$"원본 FOV 확인 불가 · Auto 미지원 (Float {camera.Animation.numberOfFloatTracks}개). 수동값을 사용합니다.";
 public static double Value(SongProject project,MotionClip camera,double timeline){
  if(project.FovMode=="Manual")return Valid(project.Fov);
  throw new InvalidDataException("원본 HKX의 FOV 바인딩·단위가 확인되지 않아 Auto를 사용할 수 없습니다. 수동 FOV를 선택하세요.");
 }
}
// Explicit degree-valued curves, not an HKX detector. Reserved for verified
// source adapters; tests use synthetic values labelled as such.
public sealed class FovCurve {
 public readonly (double Time,double Degrees)[] Keys;
 public FovCurve(params (double Time,double Degrees)[] keys){
  if(keys.Length<2)throw new InvalidDataException("FOV 키가 부족합니다.");
  Keys=keys.ToArray();for(int i=0;i<Keys.Length;i++){FovPolicy.Valid(Keys[i].Degrees);if(!double.IsFinite(Keys[i].Time)||Keys[i].Time<0||(i>0&&Keys[i].Time<=Keys[i-1].Time))throw new InvalidDataException("FOV 키 시간이 올바르지 않습니다.");}
 }
 public double At(double time){if(!double.IsFinite(time))throw new InvalidDataException("FOV 시간이 올바르지 않습니다.");if(time<=Keys[0].Time)return Keys[0].Degrees;for(int i=1;i<Keys.Length;i++)if(time<=Keys[i].Time){var a=Keys[i-1];var b=Keys[i];return a.Degrees+(b.Degrees-a.Degrees)*(time-a.Time)/(b.Time-a.Time);}return Keys[^1].Degrees;}
 public double Edited(TrackEdit edit,double timeline)=>At(Math.Clamp(edit.SourceTime(timeline),edit.In,edit.Out));
}
