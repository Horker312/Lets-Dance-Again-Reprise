using System.Windows;
using System.Windows.Controls;
using FacialPresets;
namespace FacialStudio;
// Rows are created/recycled only for the visible viewport. Values live in Morph, not in controls.
public sealed class MorphRow:UserControl {
 public MorphRow(){HorizontalContentAlignment=HorizontalAlignment.Stretch;DataContextChanged+=(_,_)=>Rebuild();Loaded+=(_,_)=>Rebuild();}
 void Rebuild(){if(DataContext is not Morph m)return;var owner=Window.GetWindow(this) as MainWindow;var root=new StackPanel{Margin=new Thickness(0,4,0,8)};Content=root;var title=new DockPanel();root.Children.Add(title);var on=new CheckBox{IsChecked=m.Enabled,VerticalAlignment=VerticalAlignment.Center,ToolTip=L.T("강도 보존 · 표정 계산에 참여")};DockPanel.SetDock(on,Dock.Left);title.Children.Add(on);var remove=new Button{Content=L.T("제거"),ToolTip=L.T("제거")};DockPanel.SetDock(remove,Dock.Right);title.Children.Add(remove);var label=new TextBlock{Text=(m.Backend=="MFEE"?"MFEE / ":"")+(owner?.MorphLabel(m)??m.Backend+" / "+m.Name),TextTrimming=TextTrimming.CharacterEllipsis,TextWrapping=TextWrapping.NoWrap,ToolTip=m.Backend+" / "+m.Name+"\nID: "+m.Id,MinWidth=0};title.Children.Add(label);var value=new TextBlock{Text=m.Strength.ToString("0.0")+"%"};root.Children.Add(value);var slider=new Slider{Minimum=0,Maximum=100,Value=m.Strength,TickFrequency=1,ToolTip=m.Backend+" / "+m.Id};root.Children.Add(slider);
  void Enable(){m.Enabled=on.IsChecked==true;label.Opacity=m.Enabled?1:.55;owner?.MorphEdited();}
  on.Checked+=(_,_)=>Enable();on.Unchecked+=(_,_)=>Enable();label.Opacity=m.Enabled?1:.55;
  slider.ValueChanged+=(_,_)=>{m.Strength=slider.Value;value.Text=m.Strength.ToString("0.0")+"%";owner?.MorphEdited();};remove.Click+=(_,_)=>owner?.DeleteMorph(m);
 }
}
