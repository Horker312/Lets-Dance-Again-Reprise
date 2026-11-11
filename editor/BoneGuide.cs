using System.Text.RegularExpressions;using System.Windows.Media;
namespace DanceStudio;
public enum BoneSide{Neutral,Left,Right}
// Name tokens only: never infer anatomical side from the bone's world position.
public static class BoneGuide {
 public static readonly Color Left=Color.FromRgb(101,200,221),Right=Color.FromRgb(164,149,238),Neutral=Color.FromRgb(175,188,211),Head=Color.FromRgb(211,200,191);
 static readonly Regex Camel=new("([a-z0-9])([A-Z])",RegexOptions.CultureInvariant),Acronym=new("([A-Z])([A-Z][a-z])",RegexOptions.CultureInvariant),Words=new("[^\\p{L}\\p{N}]+",RegexOptions.CultureInvariant);
 static string[] Tokens(string name)=>Words.Split(Camel.Replace(Acronym.Replace(name,"$1 $2"),"$1 $2")).Where(x=>x.Length>0).Select(x=>x.ToLowerInvariant()).ToArray();
 public static BoneSide Side(string name){var words=Tokens(name);bool left=words.Any(x=>x is "l" or "left"||x.StartsWith("왼",StringComparison.Ordinal));bool right=words.Any(x=>x is "r" or "right"||x.StartsWith("오른",StringComparison.Ordinal));return left==right?BoneSide.Neutral:left?BoneSide.Left:BoneSide.Right;}
 public static int HeadIndex(Rig rig){foreach(string alias in new[]{"NPC Head [Head]","Head","mixamorig:Head","Bip01 Head","Bip001 Head"}){int i=Array.FindIndex(rig.Names,n=>n.Equals(alias,StringComparison.OrdinalIgnoreCase));if(i>=0)return i;}string[] excluded=["cme","pa","hdt","pre","end","tip","nub","ear","ears","eye","eyes","jaw","hair","face","helmet"];
  var candidates=rig.Names.Select((n,i)=>(Tokens:Tokens(n),Index:i)).Where(x=>x.Tokens.Contains("head")&&!x.Tokens.Intersect(excluded).Any()).ToArray();return candidates.Length==1?candidates[0].Index:-1;}
 public static int RootIndex(Rig rig){int npc=Array.FindIndex(rig.Names,n=>n.Equals("NPC Root [Root]",StringComparison.OrdinalIgnoreCase));return npc>=0?npc:Array.FindIndex(rig.Parents,p=>p<0);}
}
