using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Input;
using DanceStudio;
namespace FacialStudio;

public sealed class NifPickerDialog:Window {
 public sealed record Candidate(string FullPath){public string FileName=>Path.GetFileName(FullPath);public string DirectoryName=>Path.GetDirectoryName(FullPath)??"";}
 public ListBox Candidates{get;}=new();
 public Button UseButton{get;}=new();
 bool decision;readonly bool allowImportCancel;public bool ImportCancelled{get;private set;}
 public string? SelectedPath=>Candidates.SelectedItem is Candidate c?c.FullPath:null;
 public NifPickerDialog(IEnumerable<string> paths,bool allowCancel=false){allowImportCancel=allowCancel;
  StudioTheme.Window(this);Resources.MergedDictionaries.Add(new(){Source=new Uri("/FacialStudio;component/DialogTheme.xaml",UriKind.Relative)});
  Title=L.T("내보낸 얼굴 NIF 확인");Width=Math.Min(760,SystemParameters.WorkArea.Width);Height=Math.Min(500,SystemParameters.WorkArea.Height);MinWidth=Math.Min(460,Width);MinHeight=Math.Min(300,Height);ResizeMode=ResizeMode.CanResize;WindowStartupLocation=WindowStartupLocation.CenterOwner;
  var grid=new Grid{Margin=new Thickness(16)};grid.RowDefinitions.Add(new(){Height=GridLength.Auto});grid.RowDefinitions.Add(new(){Height=new GridLength(1,GridUnitType.Star)});grid.RowDefinitions.Add(new(){Height=GridLength.Auto});Content=grid;
  var explanation=new TextBlock{Text=L.T("기본 슬라이더를 완전히 복원하려면 RaceMenu에서 내보낸 얼굴 NIF를 선택하세요. 같은 이름만으로 동일한 얼굴임을 보장하지 않습니다. 선택 후 메시·UV 대응을 검사합니다."),TextWrapping=TextWrapping.Wrap,Margin=new Thickness(0,0,0,12)};grid.Children.Add(explanation);
  Candidates.ItemsSource=paths.Select(p=>new Candidate(p)).ToList();Candidates.SelectionMode=SelectionMode.Single;
  var stack=new FrameworkElementFactory(typeof(StackPanel));stack.SetBinding(FrameworkElement.ToolTipProperty,new Binding(nameof(Candidate.FullPath)));
  var name=new FrameworkElementFactory(typeof(TextBlock));name.SetBinding(TextBlock.TextProperty,new Binding(nameof(Candidate.FileName)));name.SetValue(TextBlock.FontWeightProperty,FontWeights.SemiBold);name.SetValue(TextBlock.TextTrimmingProperty,TextTrimming.CharacterEllipsis);stack.AppendChild(name);
  var directory=new FrameworkElementFactory(typeof(TextBlock));directory.SetBinding(TextBlock.TextProperty,new Binding(nameof(Candidate.DirectoryName)));directory.SetValue(TextBlock.FontSizeProperty,12d);directory.SetValue(TextBlock.MarginProperty,new Thickness(0,4,0,0));directory.SetValue(TextBlock.TextTrimmingProperty,TextTrimming.CharacterEllipsis);stack.AppendChild(directory);Candidates.ItemTemplate=new DataTemplate{VisualTree=stack};Grid.SetRow(Candidates,1);grid.Children.Add(Candidates);
  var buttons=new WrapPanel{Margin=new Thickness(0,12,0,0),HorizontalAlignment=HorizontalAlignment.Right};Grid.SetRow(buttons,2);grid.Children.Add(buttons);UseButton.Content=L.T("이 얼굴 NIF 사용");UseButton.IsDefault=true;UseButton.IsEnabled=false;buttons.Children.Add(UseButton);
  var skip=new Button{Content=L.T("NIF 없이 계속"),IsCancel=true};buttons.Children.Add(skip);Candidates.SelectionChanged+=(_,_)=>UseButton.IsEnabled=SelectedPath!=null;
  if(allowCancel){var cancel=new Button{Content=L.T("불러오기 취소")};buttons.Children.Add(cancel);cancel.Click+=(_,_)=>{ImportCancelled=true;DialogResult=false;};}
  UseButton.Click+=(_,_)=>{if(SelectedPath!=null){decision=true;DialogResult=true;}};skip.Click+=(_,_)=>{decision=true;DialogResult=false;};
  Closing+=(_,_)=>{if(allowImportCancel&&!decision&&DialogResult!=true)ImportCancelled=true;};Closed+=(_,_)=>{if(allowImportCancel&&DialogResult==null)ImportCancelled=true;};Loaded+=(_,_)=>Candidates.Focus();
 }
 protected override void OnPreviewKeyDown(KeyEventArgs e){if(e.Key==Key.Escape){e.Handled=true;ImportCancelled=allowImportCancel;DialogResult=false;}else if(e.Key==Key.Enter){e.Handled=true;if(SelectedPath!=null){decision=true;DialogResult=true;}}base.OnPreviewKeyDown(e);}
}
