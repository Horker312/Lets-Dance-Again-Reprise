using System.Windows;
using System.Windows.Controls;
using System.Reflection;
using DanceStudio;
using System.IO;
namespace DanceStudio;
public static class LifetimeTests {
 static object? Field(object w,string n)=>w.GetType().GetField(n,BindingFlags.NonPublic|BindingFlags.Instance)?.GetValue(w);
 static object? Call(object w,string n,params object[] args)=>w.GetType().GetMethod(n,BindingFlags.NonPublic|BindingFlags.Instance)?.Invoke(w,args);
 public static void Run(string[] args){var app=new Application();var w=new MainWindow();string scenario=args[0],report=args[1];UiText.SettingsPath=report+".language.json";bool ok=false;var notes=new List<string>();app.Exit+=(_,_)=>File.WriteAllText(report,System.Text.Json.JsonSerializer.Serialize(new{ok,scenario,notes,windows=app.Windows.Count}));
 w.Loaded+=async(_,_)=>{try{
 if(scenario=="legacy-hidden"){var hidden=new Window();hidden.Content=new TextBlock{Text="Old detached panel"};hidden.Content=null;notes.Add("hidden registered="+app.Windows.Count);w.CleanForSmoke();w.Close();await Task.Delay(350);notes.Add("Main closed; app still running with hidden Window");hidden.Close();ok=true;return;}
 if(scenario=="project"||scenario=="audio"){await w.LoadProject(SongProject.Load(args[2]));if(scenario=="audio"){await w.TransportSmoke();Call(w,"Toggle");notes.Add("playing before close="+Field(w,"playing"));}}
 if(scenario.StartsWith("facial")){var tabs=(TabControl)Field(w,"timelineTabs")!;for(int i=0;i<(scenario=="facial-repeat"?15:1);i++){tabs.SelectedIndex=1;w.UpdateLayout();tabs.SelectedIndex=0;w.UpdateLayout();}notes.Add("windows="+app.Windows.Count);if(app.Windows.Count!=1)throw new Exception("hidden Window");}
 if(scenario=="language"){Call(w,"ChangeLanguage","en");w=(MainWindow)app.MainWindow;Call(w,"ChangeLanguage","ko");w=(MainWindow)app.MainWindow;notes.Add("windows="+app.Windows.Count);}
 if(scenario=="cancel-close"){System.ComponentModel.CancelEventHandler cancel=(_,e)=>e.Cancel=true;w.Closing+=cancel;w.CleanForSmoke();w.Close();if(!w.IsVisible||!((System.Windows.Threading.DispatcherTimer)Field(w,"timer")!).IsEnabled)throw new Exception("close cancellation broke lifetime");w.Closing-=cancel;notes.Add("cancelled close kept timer and window");}
 if(scenario=="busy"){Call(w,"SetBusy",true,"test operation");w.Close();if(!w.IsVisible)throw new Exception("closed during active operation");Call(w,"SetBusy",false,"finished");notes.Add("active operation rejected close");}
 w.CleanForSmoke();ok=true;w.Close();
 }catch(Exception ex){notes.Add(ex.ToString());w.CleanForSmoke();Call(w,"SetBusy",false,"error");w.Close();}};app.Run(w);
 }
}