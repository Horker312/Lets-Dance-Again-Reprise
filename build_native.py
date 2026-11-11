from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parent/'tools/build_native.py'),run_name='__main__')
