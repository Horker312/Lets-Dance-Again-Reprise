from pathlib import Path
import json,re,sys
r=Path(__file__).resolve().parent
v=json.loads((r/'versions.json').read_text(encoding='utf-8'))
for name in ['runtime','editor']:
 if not re.fullmatch(r'\d+\.\d+\.\d+',v[name]):raise SystemExit('Invalid version for '+name)
a,b,c=map(int,v['runtime'].split('.'));packed=(a<<16)|(b<<8)|c
if any(n>255 for n in (a,b,c)):raise SystemExit('Runtime version components must fit in 8 bits')
outputs={
 'runtime/Version.h':f'#pragma once\nnamespace DanceStudio {{ inline constexpr char RuntimeVersion[]="{v["runtime"]}"; inline constexpr unsigned RuntimeVersionNumber=0x{packed:X}; inline constexpr unsigned RuntimeMajor={a}, RuntimeMinor={b}, RuntimePatch={c}; }}\n',
 'editor/Brand.cs':f'namespace DanceStudio;\npublic static class Brand {{ public const string Name="Dance Studio"; public const string Version="{v["editor"]}"; }}\n',
 'versions.props':f'<Project><PropertyGroup><DanceStudioVersion>{v["editor"]}</DanceStudioVersion><RepriseVersion>{v["runtime"]}</RepriseVersion></PropertyGroup></Project>\n',
 'runtime/version.rc':f'''#include <winver.h>
1 VERSIONINFO
FILEVERSION {a},{b},{c},0
PRODUCTVERSION {a},{b},{c},0
FILEFLAGSMASK 0x3fL
FILEOS VOS_NT_WINDOWS32
FILETYPE VFT_DLL
BEGIN
 BLOCK "StringFileInfo"
 BEGIN
  BLOCK "040904b0"
  BEGIN
   VALUE "FileDescription", "LDA:R"
   VALUE "FileVersion", "{v['runtime']}"
   VALUE "ProductName", "LDA:R"
   VALUE "ProductVersion", "{v['runtime']}"
  END
 END
 BLOCK "VarFileInfo"
 BEGIN
  VALUE "Translation", 0x409, 1200
 END
END
'''}
for relative,content in outputs.items():
 p=r/relative
 if '--check' in sys.argv:
  if not p.exists() or p.read_text(encoding='utf-8')!=content:raise SystemExit('Stale version file: '+relative+'; run generate_versions.py')
 elif not p.exists() or p.read_text(encoding='utf-8')!=content:p.write_text(content,encoding='utf-8')
print('Version generation only:',v)
