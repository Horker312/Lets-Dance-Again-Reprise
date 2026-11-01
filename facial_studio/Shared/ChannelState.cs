namespace FacialPresets;
public static class ChannelState {
 public static bool Equal(Preset? a,Preset? b){if(a==null||b==null)return ReferenceEquals(a,b);if(a.Channels.Count!=b.Channels.Count)return false;foreach(var (name,c) in a.Channels){if(!b.Channels.TryGetValue(name,out var other)||c.Priority!=other.Priority||c.Morphs.Count!=other.Morphs.Count)return false;for(int i=0;i<c.Morphs.Count;i++){var x=c.Morphs[i];var y=other.Morphs[i];if(x.Backend!=y.Backend||x.Id!=y.Id||x.Name!=y.Name||x.Enabled!=y.Enabled||x.Strength!=y.Strength)return false;}}return true;}
}
