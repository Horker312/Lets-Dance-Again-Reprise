using System.IO;
using System.Text;
namespace DanceStudio;
public static class ProjectRules {
 public const double MaxProjectTime=3600,MaxExportDuration=1800,MaxYaw=3600,MinAspect=.25,MaxAspect=8;
 public static double NormalizeYaw(double yaw)=>((yaw+180)%360+360)%360-180;
 public static void Validate(SongProject p,bool exporting=false){
  if(p.Dance==null||p.Camera==null||p.Audio==null)throw new InvalidDataException(UiText.T("t0320"));
  if(string.IsNullOrWhiteSpace(p.Id)||Encoding.UTF8.GetByteCount(p.Id)>128||string.IsNullOrWhiteSpace(p.Title)||p.Title.Length>256)throw new InvalidDataException(UiText.T("t0321"));
  p.PackageIds??=new();
  if(p.PackageIds.Count>3||p.PackageIds.Any(x=>x.Key is not ("both" or "dance" or "camera")||string.IsNullOrWhiteSpace(x.Value)||Encoding.UTF8.GetByteCount(x.Value)>128)||p.PackageIds.Values.Distinct(StringComparer.Ordinal).Count()!=p.PackageIds.Count)throw new InvalidDataException(UiText.T("t0322"));
  PoseEdits.Validate(p);
  if(p.Verification==null)throw new InvalidDataException(UiText.T("t0323"));p.Verification.Validate();
  foreach(var track in new[]{p.Dance,p.Camera}){if(track.HkxBindingIndex is <0)throw new InvalidDataException(UiText.T("t0324"));if(track.IdentityRigProof!=null&&(track.IdentityRigProof.Length!=64||!track.IdentityRigProof.All(Uri.IsHexDigit)))throw new InvalidDataException(UiText.T("t0325"));}
  if(p.FovMode is not ("Manual" or "Auto"))throw new InvalidDataException(UiText.T("t0326"));
  if(!double.IsFinite(p.PreviewVolume)||p.PreviewVolume<0||p.PreviewVolume>1)throw new InvalidDataException(UiText.T("t0327"));
  if(!double.IsFinite(p.PreviewAspectRatio)||p.PreviewAspectRatio<MinAspect||p.PreviewAspectRatio>MaxAspect)throw new InvalidDataException(UiText.T("t0328"));
  if(!double.IsFinite(p.CameraHeightOffset)||Math.Abs(p.CameraHeightOffset)>20000)throw new InvalidDataException(UiText.T("t0329"));
  p.DanceOutputFps.Validate();p.CameraSampleFps.Validate();
  if(!double.IsFinite(p.ExportIn)||!double.IsFinite(p.ExportOut)||p.ExportIn<0||p.ExportOut<=p.ExportIn||p.ExportOut>MaxProjectTime||(exporting&&p.ExportOut-p.ExportIn>MaxExportDuration)||!double.IsFinite(p.Fov)||p.Fov is <5 or >140||!double.IsFinite(p.CameraYaw)||Math.Abs(p.CameraYaw)>MaxYaw)throw new InvalidDataException(UiText.T("t0330"));
  if(p.CameraForward is not ("+X" or "-X" or "+Y" or "-Y" or "+Z" or "-Z"))throw new InvalidDataException(UiText.T("t0311"));
  foreach(var t in new[]{p.Dance,p.Camera,p.Audio})if(t.Path==null||!double.IsFinite(t.Speed)||t.Speed<.05||t.Speed>8||!double.IsFinite(t.In)||!double.IsFinite(t.Out)||!double.IsFinite(t.Start)||t.In<0||t.Out<t.In||Math.Abs(t.Start)>MaxProjectTime)throw new InvalidDataException(UiText.T("t0331"));
  if(p.Audio.Speed!=1)throw new InvalidDataException(UiText.T("t0332"));
 }
}
