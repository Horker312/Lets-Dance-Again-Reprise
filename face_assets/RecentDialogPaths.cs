using Microsoft.Win32;
using System.IO;
using System.Windows;

namespace FaceAssets;

// Shared by Dance Studio's asset pickers, including its FaceAssetPanel.
public static class RecentDialogPaths {
 public static class Groups {
  public const string Dance="dance-hkx",Camera="camera-hkx",Audio="audio",Skeleton="skeleton",Project="project";
  public const string FacialPreset="facial-preset",FacialClip="facial-clip";
  public const string FaceNif="face-nif",FaceTri="face-tri",FaceDds="face-dds",FaceTextureFolder="face-textures",FaceProfile="face-profile";
  public const string EditorExport="editor-export",GameExport="game-export",DiagnosticsHkx="diagnostics-hkx",DiagnosticsReport="diagnostics-report",ModRoot="mod-root";
 }
 static readonly RecentPathStore Store=new(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"DanceStudio","recent-paths.json"));
 public static string DirectoryFor(string group,string? fallback=null)=>Store.Resolve(group,fallback);
 public static void RememberFile(string group,string path)=>Store.RecordFile(group,path);
 public static void RememberFolder(string group,string path)=>Store.RecordDirectory(group,path);
 public static bool Open(OpenFileDialog dialog,Window? owner,string group,string? fallback=null){
  dialog.InitialDirectory=DirectoryFor(group,fallback??dialog.InitialDirectory);
  if(dialog.ShowDialog(owner)!=true)return false;
  RememberFile(group,dialog.FileName);return true;
 }
 public static bool Save(SaveFileDialog dialog,Window? owner,string group,string? fallback=null){
  dialog.InitialDirectory=DirectoryFor(group,fallback??dialog.InitialDirectory);
  if(dialog.ShowDialog(owner)!=true)return false;
  RememberFile(group,dialog.FileName);return true;
 }
 public static bool Folder(OpenFolderDialog dialog,Window? owner,string group,string? fallback=null){
  dialog.InitialDirectory=DirectoryFor(group,fallback??dialog.InitialDirectory);
  if(dialog.ShowDialog(owner)!=true)return false;
  RememberFolder(group,dialog.FolderName);return true;
 }
}
