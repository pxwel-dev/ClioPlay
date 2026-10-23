#!/usr/bin/env python3
"""Stage a complete card runtime; verified TomTom files are downloaded locally.
The vendor CAB and its extracted files are never included in release archives.
"""
import argparse,hashlib,json,shutil,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
from prepare_vendor import CAB_URL,CAB_SHA,extract,obtain_cab
BOOTSTRAP=('ttn','clio843/btstart.sh','clio843/focusd','clio843/hidd','clio843/prefs.ini')
PROGRAMS=('clio843/clio040','clio843/cliospl','clio843/clioa2dp','clio843/font.bin')
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(4*1024*1024),b''):h.update(b)
 return h.hexdigest()
def verify(p,h):
 if sha(p)!=h:raise ValueError('Checksum mismatch: '+str(p))
def prepare(output,cab=None,cache=None):
 output=Path(output).resolve()
 if output.exists() and any(output.iterdir()):raise ValueError('Use an empty staging output folder')
 bootstrap=json.loads((ROOT/'bootstrap-SHA256.json').read_text())
 for name in BOOTSTRAP:verify(ROOT/'card-template'/name,bootstrap[name])
 for name in PROGRAMS:
  p=ROOT/name
  if name.endswith('font.bin'):continue
  b=p.read_bytes()[:52]
  if b[:7]!=b'\x7fELF\x01\x01\x01' or b[18:20]!=b'\x28\0':raise ValueError('Missing ARM runtime: '+name)
  report=ROOT/'build'/(p.name+'.build.json')
  if not report.exists():report=ROOT/'build-reports'/(p.name+'.build.json')
  verify(p,json.loads(report.read_text())['sha256'])
 cab=Path(cab) if cab else obtain_cab(cache)
 verify(cab,CAB_SHA)
 with tempfile.TemporaryDirectory(prefix='clioplay-card-') as t:
  t=Path(t);stage=t/'runtime'
  extract(cab,stage)
  for name in (*BOOTSTRAP,*PROGRAMS):
   dst=stage/name;dst.parent.mkdir(parents=True,exist_ok=True)
   source=ROOT/'card-template'/name if name in BOOTSTRAP else ROOT/name
   shutil.copyfile(source,dst)
  for name in ('ttn','vehicledaemon','clio843/btstart.sh','clio843/focusd','clio843/hidd','clio843/clio040','clio843/cliospl','clio843/clioa2dp'):(stage/name).chmod(0o755)
  manifest={'format':'global-card-runtime-2','navcore':'8.843 non-LIVE','files':{str(p.relative_to(stage)):sha(p) for p in sorted(stage.rglob('*')) if p.is_file()},'vendor_source':CAB_URL,'vendor_archive_sha256':CAB_SHA}
  (stage/'card-runtime.json').write_text(json.dumps(manifest,indent=2)+'\n')
  output.mkdir(parents=True,exist_ok=True)
  shutil.copytree(stage,output,dirs_exist_ok=True)
 return manifest
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('output',type=Path);a.add_argument('--cache',type=Path);args=a.parse_args();print(json.dumps(prepare(args.output,cache=args.cache),indent=2))
