"""Validate external runtime resources and embed matching offline fallbacks."""
from pathlib import Path
import json,re
root=Path(__file__).resolve().parent
maps={lang:json.loads((root/f'localization/runtime/{lang}.json').read_text(encoding='utf-8')) for lang in ('ko','en')}
assert maps['ko'].keys()==maps['en'].keys(),'Locale key mismatch'
for key,ko in maps['ko'].items():
 en=maps['en'][key]
 assert ko and en,key
 assert ko.partition('##')[2]==en.partition('##')[2],key
 assert not re.search('[가-힣]',en),key
lines=['#pragma once','namespace DanceStudio::L10n {']
for lang,entries in maps.items():
 pairs=','.join('{'+json.dumps(k)+','+json.dumps(v,ensure_ascii=False)+'}' for k,v in entries.items())
 lines.append(f'inline const std::unordered_map<std::string,std::string> fallback_{lang}={{{pairs}}};')
lines.append('}')
(root/'runtime/LocalizationDefaults.h').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('Runtime locale keys:',len(maps['ko']))
