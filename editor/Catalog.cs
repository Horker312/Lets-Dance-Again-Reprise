using System.IO;
using System.Text.Json;
namespace DanceStudio;
public static class Catalog {
 sealed class Input {public int index{get;set;} public string title{get;set;}="";public string dance{get;set;}="";public string camera{get;set;}="";public string audio{get;set;}="";public double audioDuration{get;set;}}
 public static void Build(string input,string output) {
  Directory.CreateDirectory(output);var rows=JsonSerializer.Deserialize<List<Input>>(File.ReadAllText(input))!;var report=new List<object>();
  foreach(var row in rows) {
   var dance=MotionClip.Read(row.dance).Container.animations!.Single();var camera=MotionClip.Read(row.camera).Container.animations!.Single();
   var project=new SongProject{Title=row.title,Skeleton=MainWindow.DefaultSkeleton,Id="lda-"+row.index.ToString("000"),Dance=new(){Path=row.dance,Out=dance.duration},Camera=new(){Path=row.camera,Out=camera.duration},Audio=new(){Path=row.audio,Out=row.audioDuration},ExportOut=Math.Min(dance.duration,row.audioDuration)};
   string label=System.Text.RegularExpressions.Regex.Replace(row.title,@"^\d+\.?\s*","");
   string name=row.index.ToString("00")+". "+string.Concat(label.Select(c=>Path.GetInvalidFileNameChars().Contains(c)?'_':c));
   File.WriteAllText(Path.Combine(output,name+".danceproj"),JsonSerializer.Serialize(project,SongProject.Json));
   report.Add(new{index=row.index,title=row.title,danceDuration=dance.duration,cameraDuration=camera.duration,audioDuration=row.audioDuration,danceFloatTracks=dance.numberOfFloatTracks,cameraFloatTracks=camera.numberOfFloatTracks,rootMotion=dance.extractedMotion!=null||camera.extractedMotion!=null,requiresSyncReview=Math.Abs(dance.duration-row.audioDuration)>.05});
  }
  File.WriteAllText(Path.Combine(output,"catalog-validation.json"),JsonSerializer.Serialize(new{entries=rows.Count,projectsReferenceOriginalFiles=true,filesCopied=false,report},SongProject.Json));
 }
}
