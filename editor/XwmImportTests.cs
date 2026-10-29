using System.IO;
using System.Text.Json;
namespace DanceStudio;
public static class XwmImportTests {
 public static void Run(string path,string output){var header=File.ReadAllBytes(path);if(header.Length<12||System.Text.Encoding.ASCII.GetString(header,8,4)!="XWMA")throw new Exception("Real compressed XWMA fixture required");
  bool decoded=false,dependencyMessage=false;string? message=null;long frames=0;int rate=0;
  try{var wave=AudioImport.Load(path);decoded=true;frames=wave.Frames;rate=wave.SampleRate;if(frames<=0)throw new Exception("Empty decode");}
  catch(FileNotFoundException ex){message=ex.Message;dependencyMessage=message.Contains("FFmpeg")&&message.Contains("DANCESTUDIO_FFMPEG");if(!dependencyMessage)throw;}
  int malformedRejected=0;foreach(bool duplicate in new[]{false,true}){byte[] bad;if(duplicate){int size=BitConverter.ToInt32(header,16);bad=header.Concat(header.Skip(12).Take(8+size+(size&1))).ToArray();}else bad=header.Concat(new byte[]{0}).ToArray();BitConverter.GetBytes((uint)(bad.Length-8)).CopyTo(bad,4);string temp=System.IO.Path.Combine(System.IO.Path.GetTempPath(),"DanceStudio-malformed-"+Guid.NewGuid().ToString("N")+".xwm");try{File.WriteAllBytes(temp,bad);try{AudioImport.Load(temp);throw new Exception("Malformed XWM accepted");}catch(InvalidDataException){malformedRejected++;}}finally{File.Delete(temp);}}
  File.WriteAllText(output,JsonSerializer.Serialize(new{fixture=path,realCompressedFixture=true,malformedRejected,decoded,dependencyMessage,message,frames,rate,duration=rate>0?(double)frames/rate:0},SongProject.Json));
 }
}

