#!/usr/bin/env python3
"""Collect bounded, identifiable release evidence; never mark hardware tests passed."""
import argparse,hashlib,json,re
from pathlib import Path
LOGS=('state.log','key.log','focus.log','bt.log','bta2dp.log','routing-status.txt')
CHECKS=('cold_boot','remote','aux_radio_sources','phone_bluetooth','navigation_frame_time','atlas_handover','camera_visual_audio','timezone_clock')
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def collect(card,out):
 card=Path(card).resolve();out=Path(out).resolve()
 if not (card/'ttn').is_file() or not (card/'clio843').is_dir():raise ValueError('Choose the SD-card root')
 if out==card or card in out.parents:raise ValueError('Collect outside the card')
 out.mkdir(parents=True,exist_ok=True);report={'hardware_status':'unverified until observations are supplied','executables':{},'logs':{}}
 for name in ('clio040','cliospl','clioa2dp','focusd','hidd'):
  p=card/'clio843'/name
  if p.is_file():report['executables'][name]={'bytes':p.stat().st_size,'sha256':sha(p)}
 p=card/'clio843/maps/build.json'
 if p.is_file():
  raw=p.read_bytes();(out/'navigation-build.json').write_bytes(raw);report['navigation_manifest_sha256']=sha(p)
 for name in LOGS:
  p=card/'clio843'/name
  if not p.is_file():continue
  with p.open('rb') as f:
   size=p.stat().st_size;f.seek(max(0,size-2*1024*1024));raw=f.read().decode('utf-8',errors='replace')
  raw=re.sub(r'\b(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}\b','[MAC REDACTED]',raw)
  raw=re.sub(r'(?m)(PHONE NAME ).*',r'\1[REDACTED]',raw);(out/name).write_text(raw);report['logs'][name]={'source_bytes':size,'tail_limited_to_bytes':2*1024*1024}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');obs=out/'observations.json'
 if not obs.exists():obs.write_text(json.dumps({key:{'status':'not tested','notes':''} for key in CHECKS},indent=2)+'\n')
 return report
if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('card',type=Path);ap.add_argument('output',type=Path);a=ap.parse_args();print(json.dumps(collect(a.card,a.output),indent=2))
