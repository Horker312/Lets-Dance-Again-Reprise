from pathlib import Path
import os
TOOLS=Path(__file__).resolve().parent
msvc=TOOLS/'msvc';vc=next((msvc/'VC/Tools/MSVC').iterdir());sdk=msvc/'Windows Kits/10';version=next((sdk/'Include').iterdir()).name
CL=vc/'bin/Hostx64/x64/cl.exe'
ENV=os.environ.copy();ENV.pop('MO2_MODS_PATH',None)
ENV['PATH']=str(CL.parent)+';'+str(sdk/'bin'/version/'x64')+';'+ENV['PATH']
ENV['INCLUDE']=';'.join(str(p)for p in [vc/'include',sdk/'Include'/version/'ucrt',sdk/'Include'/version/'shared',sdk/'Include'/version/'um',sdk/'Include'/version/'winrt',sdk/'Include'/version/'cppwinrt'])
ENV['LIB']=';'.join(str(p)for p in [vc/'lib/x64',sdk/'Lib'/version/'ucrt/x64',sdk/'Lib'/version/'um/x64'])
