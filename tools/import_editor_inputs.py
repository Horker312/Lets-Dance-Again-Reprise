"""Import privately supplied editor dependencies without publishing external assets."""
from pathlib import Path
import argparse,json,hashlib,shutil
r=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser(__doc__);p.add_argument('--libraries',type=Path,required=True);p.add_argument('--skeleton',type=Path,required=True);a=p.parse_args();expected=json.loads((r/'docs/external-editor-inputs.json').read_text())
for name,h in expected['editorLibraries'].items():
 src=a.libraries/name
 if not src.is_file()or hashlib.sha256(src.read_bytes()).hexdigest()!=h:raise SystemExit('Missing or different library: '+name)
if not a.skeleton.is_file()or hashlib.sha256(a.skeleton.read_bytes()).hexdigest()!=expected['skeleton_female.hkx']:raise SystemExit('Skeleton differs from the release input; supply the original build input')
(r/'editor/lib').mkdir(exist_ok=True);(r/'editor/Resources').mkdir(exist_ok=True)
for name in expected['editorLibraries']:shutil.copy2(a.libraries/name,r/'editor/lib'/name)
shutil.copy2(a.skeleton,r/'editor/Resources/skeleton_female.hkx')
print('Imported private build inputs; .gitignore excludes them from publication.')
