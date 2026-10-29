"""Build LDA:R from a Windows x64 MSVC Developer Command Prompt.
Dependencies are supplied by the developer; this script downloads nothing.
"""
from pathlib import Path
import argparse,subprocess,hashlib,json,concurrent.futures
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(__doc__)
p.add_argument('--dependencies',type=Path,required=True,help='Prefix with include/ and lib/ for spdlog, fmt, DirectXTK')
p.add_argument('--json-include',type=Path,required=True,help='Directory containing nlohmann/json.hpp')
p.add_argument('--rapidcsv-include',type=Path,required=True,help='Directory containing rapidcsv.h')
p.add_argument('--commonlib-library',type=Path,help='Optional matching SE-only CommonLibSSE static library; omit to build vendored source')
p.add_argument('--compiler',default='cl.exe');p.add_argument('--jobs',type=int,default=4)
a=p.parse_args();b=ROOT/'build/native';b.mkdir(parents=True,exist_ok=True);sdk=ROOT/'vendor/CommonLibSSE-NG'
flags=['/nologo','/c','/EHsc','/utf-8','/permissive-','/Zc:__cplusplus','/Zc:preprocessor','/O2','/Ob2','/DNDEBUG','/std:c++latest','/MD','/Z7']
defines=['/DENABLE_SKYRIM_SE=1','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DSPDLOG_COMPILED_LIB','/DSPDLOG_FMT_EXTERNAL','/DWINVER=0x0601','/D_WIN32_WINNT=0x0601','/DCOMMONLIB_VERSION="10.1.0"']
includes=['/I'+str(x.resolve())for x in [sdk/'include',a.dependencies/'include',a.dependencies/'include/directxtk',a.json_include,a.rapidcsv_include]]
def run(label,args):
 q=subprocess.run([str(x)for x in args],capture_output=True,text=True,errors='replace');(b/(label+'.txt')).write_text(q.stdout+q.stderr,encoding='utf-8')
 if q.returncode:raise RuntimeError(label+' failed: '+(q.stdout+q.stderr)[-7000:])
cl=Path(a.compiler);linker=cl.with_name('link.exe')if cl.parent!=Path('.')else Path('link.exe');librarian=cl.with_name('lib.exe')if cl.parent!=Path('.')else Path('lib.exe')
lib=a.commonlib_library
if lib is None:
 pch=b/'sdk.pch';unit=b/'sdk_pch.cpp';unit.write_text('#include <SKSE/Impl/PCH.h>\n');pchobj=b/'sdk_pch.obj'
 run('sdk_pch',[cl,*flags,*defines,*includes,'/YcSKSE/Impl/PCH.h','/Fp'+str(pch),'/Fo'+str(pchobj),unit])
 sources=[sdk/line.strip()for line in(sdk/'cmake/sourcelist.cmake').read_text().splitlines()if line.strip().endswith('.cpp')]
 def compile_sdk(source):
  obj=b/'sdk_objects'/source.relative_to(sdk).with_suffix('.obj');obj.parent.mkdir(parents=True,exist_ok=True)
  label=source.relative_to(sdk).as_posix().replace('/','_')
  run(label,[cl,*flags,*defines,*includes,'/YuSKSE/Impl/PCH.h','/FISKSE/Impl/PCH.h','/Fp'+str(pch),'/Fo'+str(obj),source]);return obj
 with concurrent.futures.ThreadPoolExecutor(max_workers=max(1,min(a.jobs,16)))as pool:objects=list(pool.map(compile_sdk,sources))
 lib=b/'CommonLibSSE-SE.lib';rsp=b/'sdk.rsp';rsp.write_text('\n'.join('"'+str(x)+'"'for x in[pth.resolve()for pth in [*objects,pchobj]]))
 run('sdk_link',[librarian,'/NOLOGO','/OUT:'+str(lib),'@'+str(rsp)])
obj=b/'plugin.obj';dll=b/'DanceStudio.dll'
run('compile',[cl,*flags,*defines,*includes,'/Fo'+str(obj),ROOT/'runtime/plugin.cpp'])
libs=a.dependencies/'lib'
run('link',[linker,'/NOLOGO','/DLL','/OUT:'+str(dll),'/DEBUG:FULL','/PDB:'+str(b/'DanceStudio.pdb'),'/INCREMENTAL:NO','/OPT:REF','/OPT:ICF','/NODEFAULTLIB:oldnames.lib',obj,lib,libs/'spdlog.lib',libs/'fmt.lib',libs/'DirectXTK.lib',*[x+'.lib'for x in ['advapi32','bcrypt','d3d11','d3dcompiler','dbghelp','dxgi','ole32','version','kernel32','user32','gdi32','winspool','shell32','oleaut32','uuid','comdlg32','winmm']]])
(b/'provenance.json').write_text(json.dumps({'runtime':'Skyrim SE 1.5.97 ONLY','dllSha256':hashlib.sha256(dll.read_bytes()).hexdigest(),'applicationSources':{x.name:hashlib.sha256(x.read_bytes()).hexdigest()for x in(ROOT/'runtime').iterdir()if x.suffix in ['.h','.cpp']}},indent=2))
print('Built',dll)
