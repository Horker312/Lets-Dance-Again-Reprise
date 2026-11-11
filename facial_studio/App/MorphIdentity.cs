using System.IO;
using System.Security.Cryptography;
using System.Text;
using FacialPresets;
namespace FacialStudio;
// IDs are authored data, not API registrations. Existing IDs are never migrated here.
public static class MorphIdentity {
 public static string Resolve(string name,Preset? current,IEnumerable<Preset> related,MfeeCatalog catalog){
  name=name.Trim();if(name.Length==0||name.Length>512||name.Any(char.IsControl))throw new InvalidDataException(L.T("MFEE 등록 이름이 올바르지 않습니다."));
  var states=(current==null?related:new[]{current}.Concat(related)).ToList();
  var existing=states.SelectMany(p=>p.Channels.Values).SelectMany(c=>c.Morphs).FirstOrDefault(m=>m.Backend=="MFEE"&&m.Name.Equals(name,StringComparison.OrdinalIgnoreCase));
  if(existing!=null)return existing.Id;
  var known=catalog.Entries.FirstOrDefault(e=>e.Name.Equals(name,StringComparison.OrdinalIgnoreCase));if(known!=null)return known.Id;
  string id="mfee:"+Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(name.Normalize(NormalizationForm.FormC).ToUpperInvariant()))).ToLowerInvariant();
  if(states.SelectMany(p=>p.Channels.Values).SelectMany(c=>c.Morphs).Any(m=>m.Backend=="MFEE"&&m.Id==id&&!m.Name.Equals(name,StringComparison.OrdinalIgnoreCase))||catalog.Entries.Any(e=>e.Id==id&&!e.Name.Equals(name,StringComparison.OrdinalIgnoreCase)))throw new InvalidDataException(L.T("같은 안정 ID가 다른 모프에 사용 중입니다. 기존 자료를 확인해주세요."));
  return id;
 }
}
