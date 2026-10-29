using System.IO;
using System.Runtime.InteropServices;
namespace DanceStudio;

public sealed class WaveAudio {
 public string Path="";public int SampleRate;public short Channels;public byte[] Pcm=[];public float[] Peaks=[];
 public int BlockAlign=>Channels*2;public long Frames=>Pcm.Length/BlockAlign;
 public double Duration=>(double)Frames/SampleRate;
 public static WaveAudio Load(string path) {
  using var stream=File.OpenRead(path);using var r=new BinaryReader(stream);
  if(stream.Length>512L*1024*1024)throw new InvalidDataException(UiText.T("t0338"));
  string Tag()=>System.Text.Encoding.ASCII.GetString(r.ReadBytes(4));
  if(Tag()!="RIFF")throw new NotSupportedException(UiText.T("t0339"));
  r.ReadUInt32();if(Tag()!="WAVE")throw new InvalidDataException(UiText.T("t0340"));
  ushort format=0,bits=0,channels=0,align=0;int rate=0;byte[] data=[];
  while(stream.Position+8<=stream.Length) {
   string tag=Tag();uint length=r.ReadUInt32();long end=stream.Position+length;
   if(end>stream.Length)throw new InvalidDataException(UiText.T("t0341"));
   if(tag=="fmt ") {
    if(length<16)throw new InvalidDataException(UiText.T("t0342"));
    format=r.ReadUInt16();channels=r.ReadUInt16();rate=r.ReadInt32();r.ReadUInt32();align=r.ReadUInt16();bits=r.ReadUInt16();
    if(format==0xfffe) {if(length<40)throw new InvalidDataException(UiText.T("t0343"));stream.Position+=8;format=r.ReadUInt16();}
   } else if(tag=="data")data=r.ReadBytes(checked((int)length));
   stream.Position=end+(length&1);
  }
  if(channels is <1 or >2||rate is <8000 or >192000||!((format==1&&bits is 16 or 24 or 32)||(format==3&&bits==32)))
   throw new NotSupportedException(UiText.T("t0344"));
  if(align!=channels*(bits/8)||data.Length==0||data.Length%align!=0)throw new InvalidDataException(UiText.T("t0345"));
  var result=new WaveAudio{Path=System.IO.Path.GetFullPath(path),Channels=(short)channels,SampleRate=rate};
  if(format==1&&bits==16)result.Pcm=data;
  else {
   result.Pcm=new byte[checked(data.Length/(bits/8)*2)];
   for(int i=0,j=0;i<data.Length;i+=bits/8,j+=2) {
    int value;
    if(format==3) {float v=BitConverter.ToSingle(data,i);value=float.IsFinite(v)?(int)Math.Round(Math.Clamp(v,-1,1)*32767):0;}
    else if(bits==24)value=(data[i]|data[i+1]<<8|(sbyte)data[i+2]<<16)>>8;
    else value=BitConverter.ToInt32(data,i)>>16;
    short sample=(short)Math.Clamp(value,short.MinValue,short.MaxValue);result.Pcm[j]=(byte)sample;result.Pcm[j+1]=(byte)(sample>>8);
   }
  }
  result.RebuildPeaks();
  return result;
 }
 public void RebuildPeaks(){Peaks=new float[3000];for(int i=0;i<Peaks.Length;i++){long lo=i*Frames/Peaks.Length,hi=(i+1)*Frames/Peaks.Length;for(long f=lo;f<hi;f++)for(int c=0;c<Channels;c++)Peaks[i]=Math.Max(Peaks[i],Math.Abs(BitConverter.ToInt16(Pcm,(int)(f*BlockAlign+c*2))/32768f));}}
 public void Bake(string path,TrackEdit edit,double begin,double end,double gain=1) {
  if(!double.IsFinite(gain)||gain<0||gain>1)throw new InvalidDataException(UiText.T("t0346"));
  edit.Validate(Duration);long count=(long)Math.Round((end-begin)*SampleRate);
  if(count<=0||count*BlockAlign>512L*1024*1024)throw new InvalidDataException(UiText.T("t0347"));
  using var f=File.Create(path);using var w=new BinaryWriter(f);int size=checked((int)(count*BlockAlign));
  void Tag(string s)=>w.Write(System.Text.Encoding.ASCII.GetBytes(s));
  Tag("RIFF");w.Write(size+36);Tag("WAVE");Tag("fmt ");w.Write(16);w.Write((short)1);w.Write(Channels);w.Write(SampleRate);w.Write(SampleRate*BlockAlign);w.Write((short)BlockAlign);w.Write((short)16);Tag("data");w.Write(size);
  byte[] buffer=new byte[BlockAlign*8192];long first=(long)Math.Round(edit.SourceTime(begin)*SampleRate),limit=(long)Math.Round(edit.Out*SampleRate),min=(long)Math.Round(edit.In*SampleRate);
  for(long i=0;i<count;) {
   int frames=(int)Math.Min(8192,count-i);Array.Clear(buffer);
   long from=first+i,validLo=Math.Max(from,min),validHi=Math.Min(from+frames,Math.Min(limit,Frames));
   if(validHi>validLo)Buffer.BlockCopy(Pcm,checked((int)(validLo*BlockAlign)),buffer,checked((int)((validLo-from)*BlockAlign)),checked((int)((validHi-validLo)*BlockAlign)));
   ApplyGain(buffer.AsSpan(0,frames*BlockAlign),gain);
   w.Write(buffer,0,frames*BlockAlign);i+=frames;
  }
 }
 public static void ApplyGain(Span<byte> pcm,double gain){
  if(!double.IsFinite(gain)||gain<0||gain>1||pcm.Length%2!=0)throw new InvalidDataException(UiText.T("t0348"));if(gain==1)return;if(gain==0){pcm.Clear();return;}
  for(int i=0;i<pcm.Length;i+=2){short sample=System.Buffers.Binary.BinaryPrimitives.ReadInt16LittleEndian(pcm.Slice(i,2));short adjusted=(short)Math.Clamp((int)Math.Round(sample*gain),short.MinValue,short.MaxValue);System.Buffers.Binary.BinaryPrimitives.WriteInt16LittleEndian(pcm.Slice(i,2),adjusted);}
 }
}

public sealed class WaveOutput:IDisposable {
 [StructLayout(LayoutKind.Sequential,Pack=2)]struct Format {public ushort Tag,Channels;public uint Rate,Bytes;public ushort Align,Bits,Extra;}
 [StructLayout(LayoutKind.Sequential)]struct Header {public IntPtr Data;public uint Length,Recorded;public IntPtr User;public uint Flags,Loops;public IntPtr Next,Reserved;}
 [StructLayout(LayoutKind.Sequential)]struct Time {public uint Type,Value,Extra;}
 [DllImport("winmm.dll")]static extern uint waveOutOpen(out IntPtr handle,uint device,ref Format format,IntPtr callback,IntPtr instance,uint flags);
 [DllImport("winmm.dll")]static extern uint waveOutPrepareHeader(IntPtr handle,IntPtr header,uint size);
 [DllImport("winmm.dll")]static extern uint waveOutWrite(IntPtr handle,IntPtr header,uint size);
 [DllImport("winmm.dll")]static extern uint waveOutReset(IntPtr handle);
 [DllImport("winmm.dll")]static extern uint waveOutUnprepareHeader(IntPtr handle,IntPtr header,uint size);
 [DllImport("winmm.dll")]static extern uint waveOutClose(IntPtr handle);
 [DllImport("winmm.dll")]static extern uint waveOutGetPosition(IntPtr handle,ref Time time,uint size);
 static readonly HashSet<WaveOutput> retained=[];
 IntPtr handle,header;GCHandle pinned;int sampleRate,block;double start,sourceStart,sourceEnd,gainValue;long frames;bool prepared,faulted;WaveAudio? original;
 public bool Faulted=>faulted;
 public bool Active=>handle!=IntPtr.Zero&&!faulted;
 public double Position {
  get {if(!Active)return start;var time=new Time{Type=2};Check(waveOutGetPosition(handle,ref time,(uint)Marshal.SizeOf<Time>()));
   double samples=time.Type==2?time.Value:time.Type==4?(double)time.Value/block:time.Type==1?(double)time.Value*sampleRate/1000:0;
   return start+samples/sampleRate;}
 }
 public double End=>start+(double)frames/sampleRate;
 static void Check(uint code){if(code!=0)throw new IOException(UiText.T("t0349")+code);}
 public void SetVolume(double gain){if(!double.IsFinite(gain)||gain<0||gain>1)throw new InvalidDataException(UiText.T("t0346"));if(!Active||gain==gainValue)return;double now=Position,source=sourceStart+now-start;var wave=original!;double end=sourceEnd;Play(wave,source,now,end,gain);}
 public void Play(WaveAudio wave,double source,double timeline,double until,double gain) {
  if(!double.IsFinite(gain)||gain<0||gain>1)throw new InvalidDataException(UiText.T("t0350"));Dispose();if(faulted)throw new IOException(UiText.T("t0351"));long first=(long)Math.Round(source*wave.SampleRate),last=(long)Math.Round(until*wave.SampleRate);
  first=Math.Clamp(first,0,wave.Frames);last=Math.Clamp(last,first,wave.Frames);if(last==first)return;
  start=timeline;frames=last-first;sampleRate=wave.SampleRate;block=wave.BlockAlign;
  var format=new Format{Tag=1,Channels=(ushort)wave.Channels,Rate=(uint)sampleRate,Bytes=(uint)(sampleRate*block),Align=(ushort)block,Bits=16};
  try {
   Check(waveOutOpen(out handle,uint.MaxValue,ref format,IntPtr.Zero,IntPtr.Zero,0));
   byte[] pcm=wave.Pcm.AsSpan(checked((int)(first*block)),checked((int)(frames*block))).ToArray();WaveAudio.ApplyGain(pcm,gain);original=wave;sourceStart=(double)first/sampleRate;sourceEnd=until;gainValue=gain;
   pinned=GCHandle.Alloc(pcm,GCHandleType.Pinned);header=Marshal.AllocHGlobal(Marshal.SizeOf<Header>());
   Marshal.StructureToPtr(new Header{Data=pinned.AddrOfPinnedObject(),Length=checked((uint)(frames*block))},header,false);
   Check(waveOutPrepareHeader(handle,header,(uint)Marshal.SizeOf<Header>()));prepared=true;Check(waveOutWrite(handle,header,(uint)Marshal.SizeOf<Header>()));
  }catch{Dispose();throw;}
 }
 public void Dispose() {
  if(handle!=IntPtr.Zero){
   waveOutReset(handle);
   if(prepared&&waveOutUnprepareHeader(handle,header,(uint)Marshal.SizeOf<Header>())!=0){Retain();return;}
   prepared=false;if(waveOutClose(handle)!=0){Retain();return;}handle=IntPtr.Zero;
  }
  if(header!=IntPtr.Zero){Marshal.FreeHGlobal(header);header=IntPtr.Zero;}if(pinned.IsAllocated)pinned.Free();original=null;faulted=false;lock(retained)retained.Remove(this);
 }
 void Retain(){faulted=true;lock(retained)retained.Add(this);}
}
