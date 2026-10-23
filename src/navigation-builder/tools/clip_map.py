"""Keep whole, canonically owned CPM tiles without re-rendering their payloads."""
import json,os,struct
from pathlib import Path
from build_map import HDR,IDX,SPANS

def crop_to_core(path,core):
 path=Path(path);west,south,east,north=[round(n*1e5) for n in core];rows=[]
 if any(n%16384 for n in (west,south,east,north)):raise ValueError('Atlas cores must align to CPM Z0 tile edges')
 with path.open('rb') as src:
  h=list(HDR.unpack(src.read(HDR.size)))
  if h[:4]!=[b'CPM5',5,0,6]:raise ValueError('Expected CPM5 Z0..Z6')
  src.seek(h[6])
  for i in range(h[5]):
   r=list(IDX.unpack(src.read(IDX.size)));level=r[0]-8 if 8<=r[0]<=11 else r[0]
   if not 0<=level<=6:raise ValueError("Invalid CPM source level")
   span=SPANS[level];x=r[2]*span+span//2;y=r[3]*span+span//2
   if west<=x<east and south<=y<north:rows.append(r)
  data_off=HDR.size+len(rows)*IDX.size;temp=path.with_suffix('.core-new');payload={};offset=0
  with temp.open('wb') as dst:
   dst.seek(data_off)
   for row in rows:
    key=(row[4],row[5])
    if key not in payload:
     src.seek(h[7]+row[4]);blob=src.read(row[5])
     if len(blob)!=row[5]:raise ValueError('Short CPM tile')
     payload[key]=offset;dst.write(blob);offset+=len(blob)
    row[4]=payload[key]
   h[4]|=8;h[5]=len(rows);h[6]=HDR.size;h[7]=data_off;h[8:]=[south,west,north,east];dst.seek(0);dst.write(HDR.pack(*h))
   for row in rows:dst.write(IDX.pack(*row))
   dst.flush();os.fsync(dst.fileno())
 os.replace(temp,path)
 sidecar=path.with_suffix(path.suffix+'.json')
 if sidecar.exists():
  data=json.loads(sidecar.read_text());data['atlas_core_e5']=[west,south,east,north];data['tiles']=len(rows);data['bytes']=path.stat().st_size
  from build_map import sha256_file
  data['sha256']=sha256_file(path);sidecar.write_text(json.dumps(data,indent=2)+'\n')
 return len(rows)
