using System.IO;
using System.Windows;
namespace DanceStudio;
public sealed partial class MainWindow {
 internal async Task DropAsset(string path,int row,double start)=>await Guard(async()=>{
  if(busy)return;
  string ext=Path.GetExtension(path).ToLowerInvariant();
  bool audio=ext is ".wav" or ".xwm" or ".wxm";
  if(!audio&&ext!=".hkx")throw new InvalidDataException(UiText.T("dropUnsupported"));
  if(audio)row=2;
  else if(row is <0 or >1){var answer=MessageBox.Show(this,UiText.T("dropHkx"),Brand.Name,MessageBoxButton.YesNoCancel);if(answer==MessageBoxResult.Cancel)return;row=answer==MessageBoxResult.Yes?0:1;}
  var old=new[]{Project.Dance,Project.Camera,Project.Audio}[row];if(old.Path.Length>0&&MessageBox.Show(this,UiText.T("dropReplace"),Brand.Name,MessageBoxButton.YesNo)!=MessageBoxResult.Yes)return;
  Stop();await LoadAssetCandidate(path,row,start);
 });
}
