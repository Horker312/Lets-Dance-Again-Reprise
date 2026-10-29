using System.IO;
using System.Numerics;
using System.Text.Json;
using HKX2E;
namespace DanceStudio;
public static class RateTests {
 public static void Run(string example,string output){
  Directory.CreateDirectory(output);int checks=0;void Check(bool value,string text){checks++;if(!value)throw new Exception(text);}
  void Reject(Action action,string text){bool failed=false;try{action();}catch{failed=true;}Check(failed,text);}
  foreach(double rate in new[]{.05,.5,.8,1,1.25,2.5,8}){
   var edit=new TrackEdit{In=10,Out=15,Start=5,Speed=rate};Check(Math.Abs(edit.SourceTime(6)-(10+rate))<1e-9,"Decimal source mapping");Check(Math.Abs(edit.TimelineDuration-5/rate)<1e-9,"Rate changes displayed duration");
   double originalEnd=edit.TimelineEnd;edit.TrimStart(10,15,5,-1000,24);
   Check(edit.In>=0&&edit.Start>=0,"Left handle respects source and timeline zero");Check(Math.Abs(edit.TimelineEnd-originalEnd)<1e-8,"Left trim keeps right boundary fixed");
   edit.MoveStart(5,-1000);Check(edit.Start==0,"Whole bar cannot cross zero");
   edit.In=1;edit.Out=1.001;edit.Start=0;edit.TrimStart(1,1.001,0,-1000,24);Check(edit.In==1&&edit.Start==0,"Subframe left trim cannot underflow or throw");
   edit.TrimStart(1,1.001,0,1000,24);Check(edit.In<=edit.Out&&edit.Start>=0,"Subframe rightward left-handle movement stays valid");
   edit.TrimEnd(1,1.001,-1000,1.001);Check(edit.Out>=1&&edit.Out<=1.001,"Right trim cannot cross source start");
  }
  var oldNegative=new TrackEdit{In=1,Out=1.001,Start=-2,Speed=8};oldNegative.TrimStart(1,1.001,-2,-100,24);Check(oldNegative.Start>=0&&oldNegative.In<=oldNegative.Out,"Legacy negative start cannot corrupt trim bounds");
  var project=SongProject.Load(example);Check(project.Dance.Speed==1&&project.Camera.Speed==1,"Legacy projects default to normal speed");project.Skeleton=Rig.BuiltinId;var rig=Rig.Load(project.Skeleton);var dance=MotionClip.Load(project.Dance.Path);var camera=MotionClip.Load(project.Camera.Path);var audio=WaveAudio.Load(project.Audio.Path);
  project.CameraBone=Array.FindIndex(rig.Names,x=>x.Contains("Camera3rd"));project.Dance.Speed=1.25;project.Camera.Speed=.8;project.ExportIn=1;project.ExportOut=3;
  string file=Path.Combine(output,"rates.danceproj");project.Save(file);var loaded=SongProject.Load(file);Check(loaded.SchemaVersion==4&&loaded.Dance.Speed==1.25&&loaded.Camera.Speed==.8,"Independent decimal rates save and reload");
  var folder=Exporter.Export(output,project,dance,camera,audio,rig);var body=MotionClip.Load(Path.Combine(folder,"tracks/dance.hkx"));var cam=MotionClip.Load(Path.Combine(folder,"tracks/camera.hkx"));
  for(int frame=0;frame<=48;frame+=6){double t=frame/24.0;var expected=dance.At(project.Dance.SourceTime(project.ExportIn+t));var actual=body.At(t);for(int bone=0;bone<actual.Length;++bone){Check(Vector3.Distance(actual[bone].T,expected[bone].T)<.003,"Baked decimal-rate dance position");Check(Math.Abs(Quaternion.Dot(actual[bone].R,expected[bone].R))>.99999,"Baked decimal-rate dance orientation");}
   expected=camera.At(project.Camera.SourceTime(project.ExportIn+t));actual=cam.At(t);for(int bone=0;bone<actual.Length;++bone)Check(Vector3.Distance(actual[bone].T,expected[bone].T)<.003,"Camera has its own independent decimal rate");
  }
  var result=WaveAudio.Load(Path.Combine(folder,"tracks/music.wav"));int first=(int)Math.Round(project.Audio.SourceTime(project.ExportIn)*audio.SampleRate)*audio.BlockAlign;
  Check(result.Pcm.AsSpan().SequenceEqual(audio.Pcm.AsSpan(first,result.Pcm.Length)),"Music is neither retimed nor resampled by motion speed");
  var f=MotionClip.Read(project.Dance.Path);f.Container.animations!.Single().annotationTracks.Add(new hkaAnnotationTrack{trackName="rate-test",annotations=[new hkaAnnotationTrackAnnotation{time=2,text="rate-marker"}]});
  string annotated=Path.Combine(output,"annotated-source.hkx");using(var stream=File.Create(annotated))new PackFileSerializer().Serialize(f.Root,new BinaryWriterEx(stream),f.Header);
  string cut=Path.Combine(output,"annotation-retimed.hkx");MotionClip.Load(annotated).Bake(cut,new(){In=1,Out=5,Start=.25,Speed=2},.5,1.5,24);
  var marker=MotionClip.Read(cut).Container.animations!.Single().annotationTracks.Single(x=>x.trackName=="rate-test").annotations.Single();Check(Math.Abs(marker.time-.25)<1e-6,"Annotations retimed onto the output timeline");
  foreach(double rate in new[]{0,-1,double.NaN,.001,9}){var invalid=new TrackEdit{Out=10,Speed=rate};Reject(()=>invalid.Validate(20),"Invalid rate rejected");}
  project.Audio.Speed=.8;Reject(()=>Exporter.Export(output,project,dance,camera,audio,rig),"Unsupported audio speed rejected before writing");
  File.WriteAllText(Path.Combine(output,"validation.json"),JsonSerializer.Serialize(new{passed=true,checks,danceRate=1.25,cameraRate=.8,musicUnchanged=true,annotationRateCorrect=true,leftBoundaryFixed=true,gameTested=false},SongProject.Json));
 }
}
