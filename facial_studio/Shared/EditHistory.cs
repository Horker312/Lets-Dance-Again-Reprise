using System.Text.Json;
namespace FacialPresets;
// Bounded JSON snapshots keep authored objects independent from preview/UI aliases.
public sealed class EditHistory<T> {
 readonly Stack<string> undo=new(),redo=new();string? pending;
 static string Encode(T data)=>JsonSerializer.Serialize(data);
 public void Begin(T data){pending??=Encode(data);}
 void Trim(){while(undo.Count>100||undo.Sum(s=>(long)s.Length)*2>32L*1024*1024){var keep=undo.Take(undo.Count-1).Reverse().ToArray();undo.Clear();foreach(var s in keep)undo.Push(s);}}
 public bool Commit(T data){if(pending==null)return false;string old=pending;pending=null;if(old==Encode(data))return false;undo.Push(old);redo.Clear();Trim();return true;}
 public T? Cancel(){if(pending==null)return default;string old=pending;pending=null;return JsonSerializer.Deserialize<T>(old);}
 public T? Undo(T data){if(pending!=null)Commit(data);if(undo.Count==0)return default;redo.Push(Encode(data));return JsonSerializer.Deserialize<T>(undo.Pop());}
 public T? Redo(T data){if(pending!=null)return default;if(redo.Count==0)return default;undo.Push(Encode(data));Trim();return JsonSerializer.Deserialize<T>(redo.Pop());}
 public void Clear(){pending=null;undo.Clear();redo.Clear();}
}
