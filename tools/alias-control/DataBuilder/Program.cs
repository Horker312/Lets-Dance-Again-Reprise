using Mutagen.Bethesda;
using Mutagen.Bethesda.Skyrim;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Plugins.Records;
using System.Text.Json;
using Noggog;
if(args.Length==1){using var verify=SkyrimMod.CreateFromBinaryOverlay(args[0],SkyrimRelease.SkyrimSE);var qv=verify.Quests.Single();var pv=verify.Packages.Single();var cv=verify.Cells.SelectMany(b=>b.SubBlocks).SelectMany(b=>b.Cells).Single();if(verify.EnumerateMajorRecords().Count()!=11||qv.Aliases.Count!=5||qv.VirtualMachineAdapter!=null||qv.Priority!=100||cv.Persistent.Count!=5||((PackageDataBool)pv.Data[4]).Data)throw new InvalidDataException("Eviction data verification failed");for(int i=0;i<5;i++){var marker=(IPlacedObjectGetter)cv.Persistent[i];if(marker.FormKey.ID!=0x806+i||marker.Base.FormKey.ID!=0x3B||(marker.MajorRecordFlagsRaw&0x400)==0||qv.Aliases[i].ForcedReference.FormKey!=marker.FormKey||(((uint)(qv.Aliases[i].Flags??0))&(1u<<18))!=0)throw new InvalidDataException("Non-actor marker / alias flags mismatch");}Console.WriteLine("PASS native-eviction ESL: original records preserved; QUST 803 PACK 804 CELL 805 REFR 806-80A, five persistent XMarkers, no ActorsOnly, no VMAD");return;}
if(args.Length!=3)throw new ArgumentException("game.esm base-DanceStudioData.esl output-DanceStudioData.esl");
using var game=SkyrimMod.CreateFromBinaryOverlay(args[0],SkyrimRelease.SkyrimSE);
var mod=SkyrimMod.CreateFromBinary(args[1],SkyrimRelease.SkyrimSE);
if(mod.Quests.Count!=0||mod.Packages.Count!=0||mod.EnumerateMajorRecords().Count()!=3)throw new InvalidDataException("Expected unchanged three-record LDA data base");
var quest=new Quest(new FormKey(mod.ModKey,0x803),SkyrimRelease.SkyrimSE){EditorID="LDA_PerformanceControl",Priority=100,Flags=Quest.Flag.StartGameEnabled|Quest.Flag.ExcludeFromDialogExport,NextAliasID=5};
var sample=game.Packages.Single(p=>p.EditorID=="MQ105DoNothing");
var package=new Package(new FormKey(mod.ModKey,0x804),SkyrimRelease.SkyrimSE){EditorID="LDA_PerformanceWait",Type=Package.Types.Package,Flags=Package.Flag.IgnoreCombat|Package.Flag.NoCombatAlert,InterruptFlags=0,ScheduleMonth=sample.ScheduleMonth,ScheduleDayOfWeek=sample.ScheduleDayOfWeek,ScheduleDate=sample.ScheduleDate,ScheduleHour=sample.ScheduleHour,ScheduleMinute=sample.ScheduleMinute,ScheduleDurationInMinutes=sample.ScheduleDurationInMinutes,DataInputVersion=sample.DataInputVersion};
if(sample.PackageTemplate.FormKey.ID!=0x654E2||sample.Data.Count!=2||!sample.Data.ContainsKey(3)||!sample.Data.ContainsKey(4))throw new InvalidDataException("Vanilla DoNothing template contract changed");
package.PackageTemplate.SetTo(sample.PackageTemplate.FormKey);package.OwnerQuest.SetTo(quest);
package.Data.Add(3,new PackageDataFloat{Data=0});
// Vanilla input 4 is StopMovement; false permits animation root motion.
package.Data.Add(4,new PackageDataBool{Data=false});
var markerBase=game.Statics.Single(x=>x.FormKey.ID==0x3B&&x.EditorID=="XMarker");
var cell=new Cell(new FormKey(mod.ModKey,0x805),SkyrimRelease.SkyrimSE){EditorID="LDA_PerformanceMarkerStorage",Flags=Cell.Flag.IsInteriorCell,Lighting=new CellLighting()};
var block=new CellBlock{BlockNumber=3,GroupType=GroupTypeEnum.InteriorCellBlock};var sub=new CellSubBlock{BlockNumber=5,GroupType=GroupTypeEnum.InteriorCellSubBlock};sub.Cells.Add(cell);block.SubBlocks.Add(sub);mod.Cells.Add(block);
for(uint i=0;i<5;i++){
 var alias=new QuestAlias{ID=i,Name=$"Performer{i+1}",Type=QuestAlias.TypeEnum.Reference,Flags=QuestAlias.Flag.Optional|QuestAlias.Flag.AllowReserved|QuestAlias.Flag.AllowReuseInQuest};
 var marker=new PlacedObject(new FormKey(mod.ModKey,0x806+i),SkyrimRelease.SkyrimSE){EditorID=$"LDA_PerformanceParking{i+1}",MajorRecordFlagsRaw=0x400,Placement=new Placement{Position=new P3Float(i*16,0,0),Rotation=new P3Float(0,0,0)}};
 // TESObjectREFR::RecordFlags::kPersistent (1<<10), verified in CommonLib.
 marker.Base.SetTo(markerBase);cell.Persistent.Add(marker);
 alias.ForcedReference.SetTo(marker.FormKey);alias.PackageData.Add(package.ToLink<IPackageGetter>());quest.Aliases.Add(alias);
}
mod.Quests.Add(quest);mod.Packages.Add(package);
Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(args[2]))!);
mod.BeginWrite.ToPath(args[2]).WithLoadOrder(new[]{ModKey.FromNameAndExtension("Skyrim.esm")}).WithDataFolder(Path.GetDirectoryName(args[0])!).Write();
using var read=SkyrimMod.CreateFromBinaryOverlay(args[2],SkyrimRelease.SkyrimSE);
var q=read.Quests.Single();var p=read.Packages.Single();
if(q.FormKey.ID!=0x803||p.FormKey.ID!=0x804||q.Aliases.Count!=5||q.Priority!=100||q.VirtualMachineAdapter!=null||p.Conditions.Count!=0||p.ProcedureTree.Count!=0||p.IdleAnimations!=null||((PackageDataBool)p.Data[4]).Data)throw new InvalidDataException("Quest/package read-back contract failed");
for(uint i=0;i<5;i++){var a=q.Aliases[(int)i];if(a.ID!=i||a.ForcedReference.FormKey.ID!=0x806+i||a.PackageData.Single().FormKey!=p.FormKey)throw new InvalidDataException("Alias read-back failed");}
Console.WriteLine(JsonSerializer.Serialize(new{Quest=q.FormKey.ToString(),Package=p.FormKey.ToString(),q.Priority,Aliases=q.Aliases.Count,Template=p.PackageTemplate.FormKey.ToString(),StopMovement=false,VMAD=false,Markers=5,Tests="Mutagen write/read PASS; Skyrim NOT TESTED"},new JsonSerializerOptions{WriteIndented=true}));
