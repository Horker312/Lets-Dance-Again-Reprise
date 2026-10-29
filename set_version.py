"""Explicit independent product version update. Builds never increment versions."""
from pathlib import Path
import argparse,json,re,subprocess,sys
p=argparse.ArgumentParser()
p.add_argument('product',choices=['runtime','editor'])
p.add_argument('version')
args=p.parse_args()
if not re.fullmatch(r'\d+\.\d+\.\d+',args.version) or any(int(x)>255 for x in args.version.split('.')):p.error('Use major.minor.patch with components 0..255')
root=Path(__file__).resolve().parent
path=root/'versions.json'
versions=json.loads(path.read_text(encoding='utf-8'))
versions[args.product]=args.version
temp=path.with_suffix('.tmp')
temp.write_text(json.dumps(versions,indent=2)+'\n',encoding='utf-8')
temp.replace(path)
subprocess.run([sys.executable,str(root/'generate_versions.py')],check=True)
