using System.IO;
using System.Windows;
using System.Windows.Controls;
using FaceAssets;
namespace FacialStudio;
public sealed partial class FacePreview {
 readonly TextBlock rootDisplay=new(){TextWrapping=TextWrapping.Wrap,MaxWidth=620,Margin=new Thickness(5)};
 public string? ResourceRoot=>resourceRoot;
 void InitializeResourceRoot(){var saved=ResourceRootPreference.Read();if(saved!=null){if(Directory.Exists(saved)){resourceRoot=saved;try{var detected=GameResources.Detect();if(!ResourceRootPreference.MatchesEnvironment(saved)){resourceRoot=detected;importProgress.Text=L.T("실행 환경이 바뀌었습니다. 리소스 연결은 새 환경에서 다시 확인합니다.");}}catch(Exception e)when(ImportFiles.Expected(e)){LogImport("Root restore",e);}}else importProgress.Text=L.T("저장된 리소스 폴더가 없습니다. 새 위치를 선택해주세요.");}ShowResourceRoot();}
 void ShowResourceRoot(){rootDisplay.Text=L.T("리소스 기준 경로")+": "+(resourceRoot??L.T("자동 탐색"))+"\n"+L.T("TRI 파일은 직접 표시됩니다. Data 폴더는 NIF·모프·텍스처 검색에 사용됩니다.");rootDisplay.ToolTip=resourceRoot;}
 void RememberResourceRoot(string root){resourceRoot=root;ShowResourceRoot();try{ResourceRootPreference.Save(root);}catch(Exception e)when(ImportFiles.Expected(e)){LogImport("Root preference",e);importProgress.Text+= "\n"+L.T("현재 경로는 적용했지만 설정 저장에 실패했습니다.");}}
 void QueueManualResourceConnection(FaceScene? scene){if(committingImport||scene==null||scene.Resources!=null)return;Dispatcher.BeginInvoke(new Action(async()=>{if(importerClosed||Importing||!ReferenceEquals(scene,textured))return;try{var root=resourceRoot??GameResources.Detect();if(root!=null)await ChangeResourceRootAsync(root);}catch(Exception e)when(ImportFiles.Expected(e)){LogImport("Manual resource root",e);}}));}
 public async Task<bool> ChangeResourceRootAsync(string root){
  if(importerClosed)return false;
  // The RaceMenu operation reuses its normal generation, worker and cancellation policy.
  if(textured?.JslotPath is string slot)return await ImportRaceMenuAsync(slot,root,textured.ExportedNifPath,reconnect:true);
  importCancellation?.Cancel();var cancellation=new CancellationTokenSource();importCancellation=cancellation;long generation=++importGeneration;
  var old=Assets.CaptureState();FaceScene? copy=null;
  var names=current?.Channels.Values.SelectMany(c=>c.Morphs).Where(m=>m.Enabled&&m.Backend=="MFEE").Select(m=>new FacialPresets.Morph{Backend=m.Backend,Id=m.Id,Name=m.Name,Strength=m.Strength}).ToArray()??[];
  bool entered=false;cancelImport.Visibility=Visibility.Visible;Assets.IsEnabled=false;importProgress.Text=L.T("활성 리소스 환경을 확인하고 있습니다...");
  var context=new ImportContext{Cancellation=cancellation.Token};
  try{
   copy=textured?.CopyForResources();if(copy!=null){SkinPreviewColor.RestoreOriginalsInCopy(textured!,copy);copy.FreezeForTransfer();}
   await importWorker.WaitAsync(cancellation.Token);entered=true;
   var prepared=await Task.Run(()=>{var path=ResourceRootPreference.Validate(root);var resources=new GameResources(path,context);resources.Configure(copy?.ResourcePlugins??[]);context.Cancellation.ThrowIfCancellationRequested();FaceAssetPanel.Snapshot? replacement=null;MorphResources? morphs=null;FaceScene.TextureResult? textures=null;
    if(copy!=null){copy.EditOnCurrentThread();copy.Resources=resources;var requested=copy.Parts.Where(p=>p.ManualTexturePath==null&&p.TextureReference.Length>0).ToList();int unresolved=requested.Count(p=>resources.Texture(p.TextureReference)==null);if(requested.Count>0&&unresolved==requested.Count)throw new InvalidDataException("The selected root supplies none of this face's requested textures");textures=copy.ReloadTextures();if(textures.Rejected.Count>0)throw new InvalidDataException("Resource reconnect did not resolve required textures: "+string.Join(" / ",textures.Rejected.Concat(textures.Missing)));textures=new FaceScene.TextureResult(copy.Parts.Count,copy.Parts.Count-unresolved,requested.Where(p=>resources.Texture(p.TextureReference)==null).Select(p=>p.Name+": "+p.TextureReference).ToList(),textures.Rejected);replacement=FaceAssetPanel.PrepareReconnection(copy,old,context);morphs=new MorphResources(resources);foreach(var morph in names){context.Cancellation.ThrowIfCancellationRequested();morphs.Connect(copy,morph.Name);}copy.FreezeForTransfer();}
    resources.VerifyCurrent();return (path,resources,replacement,morphs,textures);
   },cancellation.Token);
   if(importerClosed||generation!=importGeneration||cancellation.IsCancellationRequested)return false;
   var priorRoot=resourceRoot;try{committingImport=true;if(prepared.replacement!=null){prepared.replacement.Scene!.EditOnCurrentThread();stagedResources=prepared.resources;stagedMorphResources=prepared.morphs;stagedMorphNames=names.Select(m=>m.Name).ToArray();Assets.RestoreState(prepared.replacement);}resourceRoot=prepared.path;ShowResourceRoot();}catch{resourceRoot=priorRoot;try{Assets.RestoreState(old);ShowResourceRoot();}catch(Exception restore){LogImport("Resource rollback",restore);}throw;}finally{committingImport=false;}
   importProgress.Text=L.T(copy==null?"리소스 경로를 설정했습니다. TRI 단독 형상에는 변화가 없으며 NIF·모프·텍스처 검색에 사용합니다.":prepared.textures?.Missing.Count>0?"리소스 경로를 변경했습니다. 일부 텍스처가 누락되어 기존 표시를 유지합니다.":"리소스 경로와 텍스처 연결을 변경했습니다. 수동 NIF 형상과 편집 내용은 유지됩니다.");RememberResourceRoot(prepared.path);return true;
  }catch(OperationCanceledException){return false;}catch(Exception e)when(ImportFiles.Expected(e)){if(!importerClosed&&generation==importGeneration){LogImport("Resource reconnect",e);importDetails.Text=e.ToString();importProgress.Text=L.T("리소스 경로를 변경하지 못했습니다. 폴더 구조·필요한 파일·접근 권한을 확인해주세요. 이전 미리보기와 편집 내용은 유지됩니다.");}return false;}
  finally{if(entered)importWorker.Release();if(!importerClosed&&generation==importGeneration){importCancellation=null;Assets.IsEnabled=true;cancelImport.Visibility=Visibility.Collapsed;}cancellation.Dispose();}
 }
}
