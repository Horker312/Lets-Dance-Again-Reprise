using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
namespace DanceStudio;
public static class StudioTheme {
 public static readonly Brush Background=Brush("#0C111A"),Panel=Brush("#161F2D"),Input=Brush("#1B2838"),Line=Brush("#26384C"),Text=Brush("#E1E8F0"),Muted=Brush("#95A7BB"),Accent=Brush("#60C6D8"),Danger=Brush("#E27A83");
 public static Brush Brush(string hex){var b=(SolidColorBrush)new BrushConverter().ConvertFromString(hex)!;b.Freeze();return b;}
 public static void Install(){var resources=Application.Current.Resources;if(resources.Contains("Studio.Background"))return;resources.MergedDictionaries.Add(new(){Source=new Uri("/"+typeof(StudioTheme).Assembly.GetName().Name+";component/StudioTheme.xaml",UriKind.Relative)});}
 public static void Window(Window w){Install();w.Background=Background;w.Foreground=Text;w.FontFamily=new("Malgun Gothic");w.FontSize=13;w.UseLayoutRounding=true;w.SnapsToDevicePixels=true;}
 public static Border Card(UIElement content,double padding=10)=>new(){Child=content,Padding=new(padding),Background=Panel,BorderBrush=Line,BorderThickness=new(1),CornerRadius=new(7)};
}
