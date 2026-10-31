using System.IO;
namespace DanceStudio;
public static class FovPolicy {
 public static double Valid(double value){if(!double.IsFinite(value)||value<5||value>140)throw new InvalidDataException(UiText.T("t0025"));return value;}
 // No verified HKX FOV binding/unit/interpolation contract was found in the
 // audited LDA files. Float channel count or numerical range is not evidence.
 public static bool HasVerifiedSource(MotionClip camera)=>false;
 public static string Analysis(MotionClip camera)=>UiText.F("t0026", camera.Animation.numberOfFloatTracks);
 public static double Value(SongProject project,MotionClip camera,double timeline){
  if(project.FovMode=="Manual")return Valid(project.Fov);
  throw new InvalidDataException(UiText.T("t0027"));
 }
}
// Explicit degree-valued curves, not an HKX detector. Reserved for verified
// source adapters; tests use synthetic values labelled as such.
public sealed class FovCurve {
 public readonly (double Time,double Degrees)[] Keys;
 public FovCurve(params (double Time,double Degrees)[] keys){
  if(keys.Length<2)throw new InvalidDataException(UiText.T("t0028"));
  Keys=keys.ToArray();for(int i=0;i<Keys.Length;i++){FovPolicy.Valid(Keys[i].Degrees);if(!double.IsFinite(Keys[i].Time)||Keys[i].Time<0||(i>0&&Keys[i].Time<=Keys[i-1].Time))throw new InvalidDataException(UiText.T("t0029"));}
 }
 public double At(double time){if(!double.IsFinite(time))throw new InvalidDataException(UiText.T("t0030"));if(time<=Keys[0].Time)return Keys[0].Degrees;for(int i=1;i<Keys.Length;i++)if(time<=Keys[i].Time){var a=Keys[i-1];var b=Keys[i];return a.Degrees+(b.Degrees-a.Degrees)*(time-a.Time)/(b.Time-a.Time);}return Keys[^1].Degrees;}
 public double Edited(TrackEdit edit,double timeline)=>At(Math.Clamp(edit.SourceTime(timeline),edit.In,edit.Out));
}
