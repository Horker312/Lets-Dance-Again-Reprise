using System.Windows;
using FacialPresets;
namespace FacialStudio;

// Only an in-process, copied preset is accepted. File paths/JSON from arbitrary drags are not loaded.
public sealed class PresetDrag {
 public const string Format="FacialStudio.PresetPlacement.v1";
 public Preset State{get;}
 public PresetDrag(Preset source){PresetFile.Validate(source);State=FacialTimeline.Copy(source);}
 public static PresetDrag? Read(IDataObject data){try{return data.GetDataPresent(Format,false)?data.GetData(Format,false) as PresetDrag:null;}catch{return null;}}
}
