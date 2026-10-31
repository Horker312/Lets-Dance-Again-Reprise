using System.IO;
using System.Security.Cryptography;
using System.Text.Json;
namespace DanceStudio;
public static class IndependentRateTests {
 public static void Run(string project,string output){Directory.CreateDirectory(output);var p=SongProject.Load(project);var rig=Rig.Load(p.Skeleton);var d=MotionClip.Load(p.Dance.Path,p.Dance.HkxBindingIndex,p.Dance.IdentityRigProof);var c=MotionClip.Load(p.Camera.Path,p.Camera.HkxBindingIndex,p.Camera.IdentityRigProof);var audio=WaveAudio.Load(p.Audio.Path);int checks=0;void Check(bool ok,string message){if(!ok)throw new Exception(message);checks++;}
  p.DanceOutputFps=new();p.CameraSampleFps=new(){Mode="Manual",Value=30};p.Save(Path.Combine(output,"roundtrip.danceproj"));var loaded=SongProject.Load(Path.Combine(output,"roundtrip.danceproj"));Check(loaded.DanceOutputFps.Mode=="Auto"&&loaded.CameraSampleFps.Value==30&&loaded.CameraSampleFps.Mode=="Manual","independent settings roundtrip");
  Check(OutputRate.Source(d)==24,"source metadata rate");Check(new OutputRate().Resolve(d,new TrackEdit{Speed=2})==48,"speed-adjusted Auto rate");
  string copy=Path.Combine(output,"original.hkx");d.Bake(copy,new TrackEdit{Out=d.Duration},0,d.Duration,24,null,rig,null,true);Check(SHA256.HashData(File.ReadAllBytes(copy)).SequenceEqual(SHA256.HashData(File.ReadAllBytes(d.Path))),"byte identical original");
  p.ExportIn=30;p.ExportOut=31.03;p.DanceOutputFps=new(){Mode="Manual",Value=24};p.CameraSampleFps=new(){Mode="Manual",Value=30};string first=Exporter.Export(output,p,d,c,audio,rig);p.CameraSampleFps.Value=60;string second=Exporter.Export(output,p,d,c,audio,rig);
  Check(File.ReadAllBytes(Path.Combine(first,"tracks/dance.hkx")).SequenceEqual(File.ReadAllBytes(Path.Combine(second,"tracks/dance.hkx"))),"camera rate must not affect dance bytes");
  foreach(var item in new[]{(first,30.0),(second,60.0)}){using var doc=JsonDocument.Parse(File.ReadAllText(Path.Combine(item.Item1,"tracks/camera.samples.json")));var rows=doc.RootElement.GetProperty("samples");Check(Math.Abs(rows[1].GetProperty("time").GetDouble()-1/item.Item2)<1e-10,"exact camera period");Check(Math.Abs(rows[rows.GetArrayLength()-1].GetProperty("time").GetDouble()-1.03)<1e-10,"terminal camera time");}
  File.WriteAllText(Path.Combine(output,"validation.json"),JsonSerializer.Serialize(new{passed=true,checks,sourceDanceFps=OutputRate.Source(d),sourceCameraFps=OutputRate.Source(c),gameTested=false},SongProject.Json));
 }
}
