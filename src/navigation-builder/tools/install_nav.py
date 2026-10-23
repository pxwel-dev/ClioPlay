#!/usr/bin/env python3
"""Install a complete, validated Global navigation bundle with a complete prepared card runtime.

All files are staged and SHA-256 checked before changing live paths. A failed
commit attempts rollback. Never remove the card or power the device while an
installation is running. The device must be off during desktop installation.
"""
import argparse,hashlib,json,os,shutil,struct,tempfile,time
from pathlib import Path
from bundle_layout import bundle_names
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(4*1024*1024),b''):h.update(b)
 return h.hexdigest()
def validate_bundle(bundle):
 bundle=Path(bundle);m=json.loads((bundle/'build.json').read_text());names=bundle_names(m)
 if set(names)!=set(m['files']):raise ValueError('incomplete or unexpected bundle files')
 for name in names:
  info=m['files'][name];p=bundle/name
  if not 0<p.stat().st_size<0x80000000 or p.stat().st_size!=info['bytes'] or sha(p)!=info['sha256']:raise ValueError('bundle checksum mismatch: '+name)
 packs=[bundle/r['path'] for r in m['regions']] if m['builder'].startswith('global-atlas-') else [bundle]
 for pack in packs:
  with (pack/'map.cpm').open('rb') as f:h=f.read(8)
  if h[:6]!=b'CPM5\x05\0' or h[6:8]!=b'\0\x06':raise ValueError('CPM5 Z0..Z6 required')
  with (pack/'route.cpr').open('rb') as f:r=struct.unpack('<4sHH'+'I'*21+'iiii',f.read(108))
  if r[:2]!=(b'CPR2',5):raise ValueError('CPR2 v5 required')
  from qadj_format import validate_qadj_header
  validate_qadj_header(pack/'route.qadj',r,(pack/'route.cpr').stat().st_size)
 if m['builder'].startswith('global-atlas-'):
  with (bundle/'atlas.cpa').open('rb') as f:h=struct.unpack('<4sHH14I',f.read(64))
  if h[:3]!=(b'CPA1',1,80) or h[3]!=len(packs) or h[4]>65534 or h[11]!=(bundle/'atlas.cpa').stat().st_size:raise ValueError('invalid atlas catalog')
 return m

def install(bundle,sd,runtime=None):
 bundle=Path(bundle).resolve();sd=Path(sd).resolve();manifest=validate_bundle(bundle);names=bundle_names(manifest)
 if not sd.is_dir():raise ValueError('Choose an existing SD-card root or local folder')
 if sd==bundle or sd in bundle.parents or bundle in sd.parents:raise ValueError('bundle and card must be separate')
 files=[(bundle/n,Path('clio843/maps')/n) for n in names]+[(bundle/'build.json',Path('clio843/maps/build.json'))]
 if runtime is None:raise ValueError('Supply the complete runtime prepared by tools/prepare_card.py')
 runtime=Path(runtime)
 prepared=json.loads((runtime/'card-runtime.json').read_text())
 expected={'ttn','ttsystem','vehicledaemon','clio843/btstart.sh','clio843/focusd','clio843/hidd','clio843/prefs.ini','clio843/clio040','clio843/cliospl','clio843/clioa2dp','clio843/font.bin'}
 if prepared.get('format')!='global-card-runtime-2' or set(prepared['files'])!=expected:raise ValueError('Incomplete prepared clean-card runtime')
 for name in sorted(expected):
  if sha(runtime/name)!=prepared['files'][name]:raise ValueError('Clean-card runtime checksum mismatch: '+name)
  if name=='clio843/prefs.ini' and (sd/name).exists():continue
  files.append((runtime/name,Path(name)))
 for n in ('clio040','cliospl','clioa2dp'):
  with (runtime/'clio843'/n).open('rb') as f:h=f.read(20)
  if h[:7]!=b'\x7fELF\x01\x01\x01' or h[18:20]!=b'\x28\0':raise ValueError('ARM runtime executable missing: '+n)
 files.append((runtime/'card-runtime.json',Path('clio843/card-runtime.json')))
 files=list({str(rel):(src,rel) for src,rel in files}.values())
 # Refuse redirects through pre-existing symbolic links before staging anything.
 for src,rel in files:
  target=sd/rel
  while target!=sd:
   if target.is_symlink():raise ValueError('Card path is a symbolic link: '+str(target))
   target=target.parent
 obsolete=[Path('clio843/maps/search.cps')] if 'search.cps' not in names else []
 old=sd/'clio843/maps/build.json'
 if old.is_file():
  try:
   prior=bundle_names(json.loads(old.read_text()))
   obsolete.extend(Path('clio843/maps')/n for n in prior if n not in names)
  except (ValueError,KeyError,TypeError):pass
 if 'atlas.cpa' not in names and (sd/'clio843/maps/atlas.cpa').exists():obsolete.append(Path('clio843/maps/atlas.cpa'))
 obsolete=list(dict.fromkeys(obsolete))
 for rel in obsolete:
  target=sd/rel
  while target!=sd:
   if target.is_symlink():raise ValueError('Obsolete card path is a symbolic link: '+str(target))
   target=target.parent
 need=sum(src.stat().st_size for src,dst in files)
 if shutil.disk_usage(sd).free<need+16*1024*1024:raise ValueError('not enough free space to stage the complete bundle')
 staging=Path(tempfile.mkdtemp(prefix='.clioplay-global-stage-',dir=sd));backup=Path(tempfile.mkdtemp(prefix='clioplay-backup-'+time.strftime('%Y%m%d-%H%M%S')+'-',dir=sd))
 installed=[];moved=[]
 try:
  for src,rel in files:
   dst=staging/rel;dst.parent.mkdir(parents=True,exist_ok=True)
   with src.open('rb') as inp,dst.open('wb') as out:
    shutil.copyfileobj(inp,out,4*1024*1024);out.flush();os.fsync(out.fileno())
   if sha(src)!=sha(dst):raise ValueError('staging verification failed: '+str(rel))
   if src.stat().st_mode&0o111:dst.chmod(0o755)
  # Save old persisted formats before the new runtime migrates them on first use.
  # The live copies and Bluetooth pairing store stay in place.
  for name in ('prefs.ini','recent.dat','faves.dat','trip.dat'):
   rel=Path('clio843')/name
   if (sd/rel).is_file():
    bak=backup/rel;bak.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(sd/rel,bak)
  for rel in obsolete:
   if (sd/rel).is_file():
    bak=backup/rel;bak.parent.mkdir(parents=True,exist_ok=True);os.replace(sd/rel,bak);moved.append(rel)
  # Preferences are intentionally retained, including pairings, places and trips.
  for src,rel in files:
   dst=sd/rel;dst.parent.mkdir(parents=True,exist_ok=True)
   if dst.exists():
    bak=backup/rel;bak.parent.mkdir(parents=True,exist_ok=True);os.replace(dst,bak);moved.append(rel)
   os.replace(staging/rel,dst);installed.append(rel)
  if hasattr(os,'sync'):os.sync()
 except BaseException:
  for rel in reversed(installed):(sd/rel).unlink(missing_ok=True)
  for rel in reversed(moved):os.replace(backup/rel,sd/rel)
  if hasattr(os,'sync'):os.sync()
  raise
 finally:shutil.rmtree(staging)
 return backup

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('bundle',type=Path);p.add_argument('sd_root',type=Path);p.add_argument('--runtime',type=Path,required=True,help='complete runtime prepared by tools/prepare_card.py');a=p.parse_args();print('Backup: '+str(install(a.bundle,a.sd_root,a.runtime)))
if __name__=='__main__':main()
