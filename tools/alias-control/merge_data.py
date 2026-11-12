from pathlib import Path
import struct,zlib,hashlib,json,argparse
def groups(data):
 size=24+struct.unpack_from('<I',data,4)[0];return data[:size],data[size:]
def fields(data):
 at=0
 while at<len(data):
  tag,n=struct.unpack_from('<4sH',data,at);at+=6;yield tag,data[at:at+n];at+=n
parser=argparse.ArgumentParser();parser.add_argument('--base',default='data/base/DanceStudioData.esl');parser.add_argument('--generated',default='tools/alias-control/generated/DanceStudioData.esl');parser.add_argument('--output',default='tools/alias-control/package/DanceStudioData.esl');parser.add_argument('--report',default='tools/alias-control/data-validation.json');args=parser.parse_args()
base=Path(args.base).read_bytes();new=Path(args.generated).read_bytes()
header,original=groups(base);_,extra=groups(new);kept=[];at=0
while at<len(extra):
 tag,n,label=struct.unpack_from('<4sI4s',extra,at);assert tag==b'GRUP'
 if label in (b'QUST',b'PACK',b'CELL'):kept.append(extra[at:at+n])
 at+=n
assert len(kept)==3
body=[]
for tag,value in fields(header[24:]):
 if tag==b'HEDR':version,count,nextid=struct.unpack('<fII',value);value=struct.pack('<fII',version,count+8,0x80B)
 body.append(struct.pack('<4sH',tag,len(value))+value)
b=b''.join(body);merged=header[:4]+struct.pack('<I',len(b))+header[8:24]+b+original+b''.join(kept)
# Existing groups, including 0x800/801/802, are byte-identical.
out=Path(args.output);out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(merged)
assert original in merged
quest=next(g for g in kept if g[8:12]==b'QUST');record=quest[24:];flags=struct.unpack_from('<I',record,8)[0];payload=record[24:]
if flags&0x40000:payload=zlib.decompress(payload[4:])
f=list(fields(payload));assert len([v for k,v in f if k==b'ALST'])==5
assert [struct.unpack('<I',v)[0] for k,v in f if k==b'ALFR']==[0x01000806+i for i in range(5)],'Aliases must start with their own persistent XMarker'
result={'existingGroupsByteIdentical':True,'quest':'0x803','package':'0x804','aliases':5,'initialForcedReferences':'REFR 0x806..0x80A; persistent non-actor XMarker','sha256':hashlib.sha256(merged).hexdigest()}
Path(args.report).write_text(json.dumps(result,indent=2));print(json.dumps(result))
