"""Bounded overlapping regional atlas from one verified PBF.

Cores align with CPM Z0 tiles, so no tile straddles two owners. Smart extraction
keeps complete restriction and area relations. CH boundary states contract last;
all such states participate in the overlay, without proximity-based connectors.
"""
import hashlib,json,math,os,shutil,struct,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parents[1]
STEP=196608 # 12 Z0 tiles, approximately two degrees
MARGIN=.03
from bundle_layout import SHARED, REGIONAL, remove_obsolete
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(4*1024*1024),b''):h.update(b)
 return h.hexdigest()
def build(pbf,bounds,output,work,cache,imports=(),countries=(),speed_import=None):
 from speed_import import SpeedImport
 correction=SpeedImport(speed_import) if speed_import else None
 from global_regions import Regions
 from build_timezones import build as timezones
 from build_cameras import HDR as CHDR,REC as CREC
 from atlas_format import write as write_catalog
 staged=work/'atlas-result';staged.mkdir(parents=True,exist_ok=True);extract=work/'region.osm.pbf';childwork=work/'current-region';metadata=[]
 west,south,east,north=[round(n*1e5) for n in bounds];queue=[]
 for y in range(math.floor(south/STEP)*STEP,north+1,STEP):
  for x in range(math.floor(west/STEP)*STEP,east+1,STEP):queue.append((x/1e5,y/1e5,(x+STEP)/1e5,(y+STEP)/1e5))
 def builder_fingerprint():
  h=hashlib.sha256();files=[HERE/'build_navigation.py',HERE/'requirements.txt']+sorted(p for p in (HERE/'tools').glob('*') if p.suffix in ('.py','.cpp'))+sorted(p for p in (HERE/'data').iterdir() if p.is_file() and p.name!='.DS_Store')
  for p in files:
   h.update(str(p.relative_to(HERE)).encode()+b'\0')
   with p.open('rb') as f:
    for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
  if correction:h.update(correction.path.read_bytes())
  return h.hexdigest()
 builder_sha=builder_fingerprint();source_stat=(pbf.stat().st_size,pbf.stat().st_mtime_ns)
 def unchanged():
  if builder_fingerprint()!=builder_sha:raise RuntimeError('Builder source/data changed during the atlas build; rerun to resume with one consistent version')
  if (pbf.stat().st_size,pbf.stat().st_mtime_ns)!=source_stat:raise RuntimeError('Source PBF changed during the atlas build; rerun from a stable extract')

 cameras={};regions=Regions();seq=0
 from shapely.geometry import box
 from shapely.ops import unary_union
 coverage=unary_union([g for iso,name,g,p in regions.rows if iso in {c.upper() for c in countries}]).buffer(MARGIN) if countries else None
 while queue:
  unchanged()
  core=queue.pop(0);x,y,xx,yy=core
  if coverage is not None and not coverage.intersects(box(*core)):continue
  seq+=1;name=f'r{seq:04d}';dest=staged/name;log=work/(name+'.log');extract.unlink(missing_ok=True)
  if shutil.disk_usage(work).free<3*1024**3:raise RuntimeError('Less than 3 GiB free; atlas build paused before creating another region. Free scratch space and rerun.')
  print(f'Atlas region {seq}: '+', '.join(f'{v:.5f}' for v in core),flush=True)
  subprocess.run(['osmium','extract','--bbox',','.join(map(str,(max(-180,x-MARGIN),max(-85,y-MARGIN),min(180,xx+MARGIN),min(85,yy+MARGIN)))),'--strategy','smart','-S','types=multipolygon,restriction,enforcement',str(pbf),'-o',str(extract),'--overwrite'],check=True)
  if extract.stat().st_size<=100:continue # empty PBF header; no valid bbox
  info=json.loads(subprocess.check_output(['osmium','fileinfo','-e','-j',str(extract)],text=True))
  if not info['data']['count']['ways']:continue
  # A successful cached region must match source/core and every output hash.
  stamp=dest/'atlas-region.json';cached=None
  if stamp.exists():
   cached=json.loads(stamp.read_text())
   if cached.get('builder_sha256')!=builder_sha or cached.get('source')!=sha(extract) or cached.get('core')!=list(core) or any(not (dest/n).is_file() or sha(dest/n)!=h for n,h in cached['files'].items()):cached=None
  if not cached:
   shutil.rmtree(childwork,ignore_errors=True);shutil.rmtree(dest,ignore_errors=True)
   cmd=[sys.executable,str(HERE/'build_navigation.py'),str(extract),'--yes','--single-region','--atlas-region','--portal-core',*map(str,core),'--output',str(dest),'--work',str(childwork),'--cache',str(cache)]
   if correction:cmd+=['--speed-import',str(correction.path)]
   for p in imports:cmd+=['--camera-import',str(p)]
   with log.open('w') as f:result=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
   if result.returncode:
    text=log.read_text(errors='replace')
    if 'No routable motorcar roads in region' in text:
     shutil.rmtree(dest,ignore_errors=True);shutil.rmtree(childwork,ignore_errors=True);continue
    if '2 GiB' in text or 'too many transfer states' in text:
     # Split each dimension independently; odd tile counts need not halve evenly.
     wx=round((xx-x)*1e5);hy=round((yy-y)*1e5)
     xs=[x,xx] if wx<=16384 else [x,(round(x*1e5)+(wx//32768)*16384)/1e5,xx]
     ys=[y,yy] if hy<=16384 else [y,(round(y*1e5)+(hy//32768)*16384)/1e5,yy]
     if len(xs)==len(ys)==2:raise RuntimeError('Minimum atlas core still exceeds device limits; '+str(log))
     queue[0:0]=[(a,b,c,d) for b,d in zip(ys,ys[1:]) for a,c in zip(xs,xs[1:])];shutil.rmtree(dest,ignore_errors=True);continue
    raise RuntimeError(f'Region build failed; inspect {log}\n'+text[-1800:])
   unchanged()
   source=sha(extract);files={n:sha(dest/n) for n in (*REGIONAL,'cameras.cpc','route.cpn')};cached={'builder_sha256':builder_sha,'source':source,'core':list(core),'files':files};stamp.write_text(json.dumps(cached,indent=2)+'\n')
  raw=(dest/'cameras.cpc').read_bytes();h=CHDR.unpack_from(raw)
  if h[:3]!=(b'CPC1',2,CREC.size):raise ValueError('Camera version mismatch')
  for i in range(h[3]):
   r=CREC.unpack_from(raw,16+i*CREC.size);cameras[(r[0],r[1],r[2],r[3],r[5],r[8])]=r
  intersections=regions.intersecting(core);label='/'.join(code for code,name in intersections) or 'Region'
  metadata.append({'path':name,'name':label+' '+str(seq),'core':list(core)})
  # Country builds never retain raw nodes/SQLite/CH scratch for all regions.
  shutil.rmtree(childwork,ignore_errors=True)
 if not metadata:raise ValueError('No regional road data in this input')
 unchanged()
 report=write_catalog(staged,metadata);regions.write(staged/'regions.cpg',bounds);tz=timezones(staged/'timezones.cpt',bounds)
 font=HERE/'data/global-fonts.cpf'
 if sha(font)!=json.loads(font.with_suffix('.cpf.json').read_text())['sha256']:raise ValueError('Bundled native font pack failed checksum validation')
 shutil.copyfile(font,staged/'fonts.cpf')
 with (staged/'cameras.cpc').open('wb') as f:
  records=sorted(cameras.values());f.write(CHDR.pack(b'CPC1',2,CREC.size,len(records),CHDR.size))
  for r in records:f.write(CREC.pack(*r))
 names=['atlas.cpa',*SHARED]
 for m in metadata:names.extend(m['path']+'/'+n for n in REGIONAL)
 manifest={'builder':'global-atlas-2','speed_import':correction.identity if correction else None,'builder_sha256':builder_sha,'source':{'name':pbf.name,'sha256':sha(pbf),'bounds':bounds},'atlas':report,'timezones':tz,'regions':metadata,'files':{n:{'bytes':(staged/n).stat().st_size,'sha256':sha(staged/n)} for n in names}}
 if not output.exists():
  for path in staged.rglob('*'):
   if path.is_file() and path.relative_to(staged).as_posix() not in names:path.unlink()
  (staged/'build.json').write_text(json.dumps(manifest,indent=2)+'\n')
  output.parent.mkdir(parents=True,exist_ok=True)
  os.replace(staged,output);extract.unlink(missing_ok=True)
  print(f'Atlas ready: {output}: {report["regions"]} regions, {report["portals"]} legal transfer states.',flush=True)
  return manifest
 output.mkdir(parents=True,exist_ok=True)
 # Per-file atomic publication, manifest last. Installer commits as a transaction.
 for name in names:
  dst=output/name;dst.parent.mkdir(parents=True,exist_ok=True);tmp=dst.with_suffix(dst.suffix+'.new');shutil.copyfile(staged/name,tmp);os.replace(tmp,dst)
 remove_obsolete(output,names)
 (output/'build.json').write_text(json.dumps(manifest,indent=2)+'\n');extract.unlink(missing_ok=True)
 print(f'Atlas ready: {output}: {report["regions"]} regions, {report["portals"]} legal transfer states.',flush=True)
 return manifest
