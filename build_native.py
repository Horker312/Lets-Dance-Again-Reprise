from pathlib import Path
import importlib.util,subprocess,hashlib,json
r=Path(__file__).resolve().parent
base=Path('C:/Users/mamas/Documents/Codex/imefix-src')
spec=importlib.util.spec_from_file_location('df_build_env',r/'build-tools/build_env.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
sdk=base/'extern/CommonLibSSE-NG'
lib=r/'update-0.5.0/native_build_multirt/CommonLibSSE-SE-AE.lib'
cache=json.loads((lib.parent/'sdk-cache.json').read_text('utf-8'))
assert cache['lib_sha256']==hashlib.sha256(lib.read_bytes()).hexdigest()
current={p.relative_to(sdk).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for sub in ['include','src'] for p in (sdk/sub).rglob('*') if p.is_file()}
assert current==cache['sdk_sources_sha256'],'SE SDK source fingerprint changed; rebuild matching SDK before use'
build=r/'native_build';build.mkdir(exist_ok=True);(r/'logs').mkdir(exist_ok=True)
includes=['/I'+str(m.TOOLS/'json/json-3.12.0/single_include'),'/I'+str(sdk/'include'),'/I'+str(sdk/'extern/openvr/headers'),'/I'+str(base/'local-deps/installed/include'),'/I'+str(base/'local-deps/installed/include/directxtk'),'/I'+str(m.TOOLS/'rapidcsv/rapidcsv-master/src')]
defines=['/DENABLE_SKYRIM_SE=1','/DENABLE_SKYRIM_AE=1','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DSPDLOG_COMPILED_LIB','/DSPDLOG_FMT_EXTERNAL','/DWINVER=0x0601','/D_WIN32_WINNT=0x0601','/DCOMMONLIB_VERSION="10.1.0"']
flags=['/nologo','/c','/EHsc','/utf-8','/permissive-','/Zc:__cplusplus','/Zc:preprocessor','/O2','/Ob2','/DNDEBUG','/std:c++latest','/MD','/Z7','/W3']
subprocess.run(['python',str(r/'generate_versions.py')],check=True)
subprocess.run(['python',str(r/'generate_locales.py')],check=True)
sources={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (r/'runtime').glob('*') if p.is_file() and p.suffix in ['.h','.cpp']}
def run(name,args):
 p=subprocess.run([str(x) for x in args],env=m.ENV,capture_output=True,text=True,errors='replace')
 (r/'logs'/('native_'+name+'.txt')).write_text(p.stdout+p.stderr,'utf-8')
 if p.returncode:print((p.stdout+p.stderr)[-9000:]);raise SystemExit(p.returncode)
 print('PASS',name)
obj=build/'plugin.obj';dll=build/'DanceStudio.dll'
run('compile',[m.CL,*flags,*defines,*includes,'/Fo'+str(obj),r/'runtime/plugin.cpp'])
rc=next((m.TOOLS/'msvc/Windows Kits/10/bin').glob('*/x64/rc.exe'))
run('resource',[rc,'/nologo','/fo'+str(build/'version.res'),r/'runtime/version.rc'])
deps=base/'local-deps/installed/lib'
run('link',[m.CL.parent/'link.exe','/NOLOGO','/DLL','/OUT:'+str(dll),'/DEBUG:FULL','/PDB:'+str(build/'DanceStudio.pdb'),'/MAP:'+str(build/'DanceStudio.map'),'/INCREMENTAL:NO','/OPT:REF','/OPT:ICF','/NODEFAULTLIB:oldnames.lib',obj,build/'version.res',lib,deps/'spdlog.lib',deps/'fmt.lib',deps/'DirectXTK.lib',*['advapi32.lib','bcrypt.lib','d3d11.lib','d3dcompiler.lib','dbghelp.lib','dxgi.lib','ole32.lib','version.lib','kernel32.lib','user32.lib','gdi32.lib','winspool.lib','shell32.lib','oleaut32.lib','uuid.lib','comdlg32.lib','winmm.lib']])
assert sources=={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (r/'runtime').glob('*') if p.is_file() and p.suffix in ['.h','.cpp']}
(build/'build-provenance.json').write_text(json.dumps({'sources':sources,'dll_sha256':hashlib.sha256(dll.read_bytes()).hexdigest(),'runtime':'1.5.97 / Steam 1.6.640 / Steam 1.6.1170 (AE game unverified)','defines':defines,'sdk_fingerprint':cache['fingerprint'],'sdk_sha256':cache['lib_sha256']},indent=2),'utf-8')
