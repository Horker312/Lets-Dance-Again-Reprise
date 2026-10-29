using System.IO;
using System.Text.Json;
namespace DanceStudio;
public static class VolumeTests {
 public static void Run(string output){
  Directory.CreateDirectory(output);int checks=0;void Check(bool ok,string message){checks++;if(!ok)throw new Exception(message);}
  var samples=new short[]{short.MinValue,-12345,-1,0,1,12345,short.MaxValue};
  var bytes=samples.SelectMany(BitConverter.GetBytes).ToArray();
  foreach(double gain in new[]{0,.25,.5,.7,1}){
   var copy=(byte[])bytes.Clone();WaveAudio.ApplyGain(copy,gain);
   for(int i=0;i<samples.Length;i++)Check(BitConverter.ToInt16(copy,i*2)==(short)Math.Round(samples[i]*gain),"PCM gain");
  }
  foreach(double bad in new[]{-.1,1.01,double.NaN,double.PositiveInfinity}){bool rejected=false;try{WaveAudio.ApplyGain((byte[])bytes.Clone(),bad);}catch(InvalidDataException){rejected=true;}Check(rejected,"Invalid gain rejected");}
  var pcm=Enumerable.Range(0,8000).SelectMany(i=>BitConverter.GetBytes(samples[i%samples.Length])).ToArray();
  var wave=new WaveAudio{SampleRate=8000,Channels=1,Pcm=pcm};var edit=new TrackEdit{In=0,Out=1};
  foreach(double gain in new[]{0,.35,1}){
   string path=Path.Combine(output,$"gain-{gain}.wav");wave.Bake(path,edit,0,1,gain);var baked=WaveAudio.Load(path);
   Check(baked.SampleRate==8000&&baked.Frames==8000&&baked.Channels==1,"WAV format and timing");
   for(int i=0;i<8000;i++)Check(BitConverter.ToInt16(baked.Pcm,i*2)==(short)Math.Round(samples[i%samples.Length]*gain),"WAV gain");
  }
  Check(pcm.SequenceEqual(Enumerable.Range(0,8000).SelectMany(i=>BitConverter.GetBytes(samples[i%samples.Length]))),"Source unchanged");
  var project=new SongProject{PreviewVolume=.35,ApplyVolumeInGame=true};string projectPath=Path.Combine(output,"volume.danceproj");project.Save(projectPath);var loaded=SongProject.Load(projectPath);Check(loaded.PreviewVolume==.35&&loaded.ApplyVolumeInGame&&loaded.SchemaVersion==5,"Round trip");
  File.WriteAllText(projectPath,"{\"SchemaVersion\":1}");bool legacyRejected=false;try{SongProject.Load(projectPath);}catch(InvalidDataException){legacyRejected=true;}Check(legacyRejected,"old project schema explicitly rejected");
  File.WriteAllText(projectPath,"{\"SchemaVersion\":5,\"PreviewVolume\":1.1}");bool invalid=false;try{SongProject.Load(projectPath);}catch(InvalidDataException){invalid=true;}Check(invalid,"Invalid project volume rejected");
  File.WriteAllText(Path.Combine(output,"validation.json"),JsonSerializer.Serialize(new{passed=true,checks},SongProject.Json));
 }
}
