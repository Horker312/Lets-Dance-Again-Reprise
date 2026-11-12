"""Run from a source checkout: python verification/performance-package/run.py --json-include <nlohmann root>.
Requires an x64 MSVC Developer Command Prompt; no game or personal files used.
"""
from pathlib import Path
import argparse,subprocess,contextlib
p=argparse.ArgumentParser();p.add_argument('--compiler',default='cl.exe');p.add_argument('--json-include',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parent
with contextlib.nullcontext(root/'build') as temp:
 d=Path(temp)
 d.mkdir(parents=True,exist_ok=True)
 build=subprocess.run([a.compiler,'/nologo','/EHsc','/utf-8','/std:c++latest','/MD','/O2','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/I'+str(a.json_include.resolve()),str(root/'rebuild-smoke.cpp'),'/Fo'+str(d/'smoke.obj'),'/Fe'+str(d/'smoke.exe')],cwd=d,capture_output=True,text=True,errors='replace')
 if build.returncode:print(build.stdout+build.stderr);raise SystemExit(build.returncode)
 result=subprocess.run([str(d/'smoke.exe')],cwd=d,capture_output=True,text=True,errors='replace');print(result.stdout+result.stderr);raise SystemExit(result.returncode)
