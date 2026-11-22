#!/usr/bin/env python3
"""Validate the actual compact framebuffer UI."""
import argparse,hashlib,json,shutil,struct,subprocess,tempfile
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
from text_engine import build as build_text
ROOT=Path(__file__).resolve().parents[1]
def decode(raw):
 assert len(raw)==480*272*2
 rgb=bytearray()
 for v, in struct.iter_unpack('<H',raw):
  r,g,b=(v>>11)&31,(v>>5)&63,v&31;rgb.extend(((r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2)))
 return Image.frombytes('RGB',(480,272),bytes(rgb))
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--maps',type=Path,default=ROOT/'navigation-output')
 ap.add_argument('--output',type=Path,help='temporary native capture folder');
 a=ap.parse_args();maps=a.maps.resolve();out=a.output or Path(tempfile.mkdtemp(prefix='clioplay-native-captures-'));out.mkdir(parents=True,exist_ok=True)
 pack_before={p.name:(p.stat().st_size,p.stat().st_mtime_ns) for p in maps.glob('*') if p.is_file()}
 results={}
 with tempfile.TemporaryDirectory(prefix='clioplay-ui-verify-') as td:
  tmp=Path(td);(tmp/'clio843').mkdir();(tmp/'clio843/maps').symlink_to(maps,target_is_directory=True)
  shutil.copyfile(ROOT/'clio843/font.bin',tmp/'clio843/font.bin')
  base=['cc','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-Wno-unused-function','-DCLIO_HOST_ROOT="'+str(tmp)+'/"']
  # The layer regression uses synthetic current-format fixtures; it is independent of map coverage.
  jobs=[('native',ROOT/'tests/native.c',[],[str(maps/'map.cpm'),str(out)]),('splash',ROOT/'tests/splash.c',[],[str(out)]),('layers',ROOT/'tests/map-layers.c',[],[]),('coverage',ROOT/'tests/map-antialias.c',[],[]),('areas',ROOT/'tests/map-areas.c',[],[]),('car-snap',ROOT/'tests/map-car-snap.c',[],[])]
  for name,source,flags,args in jobs:
   exe=tmp/name
   if name=='native':
    obj=tmp/'native.o';subprocess.run([*base,*flags,'-DCLIO_UNICODE=1','-c',str(source),'-o',str(obj)],check=True);subprocess.run(['c++','-fsanitize=undefined',str(obj),str(build_text('host-pool')),'-o',str(exe)],check=True)
   else:subprocess.run([*base,*flags,str(source),'-o',str(exe)],check=True)
   result=subprocess.run([str(exe),*args],text=True,capture_output=True,timeout=90);print(result.stdout,end='')
   if result.returncode:print(result.stderr);raise SystemExit(result.returncode)
   results[name]=result.stdout.splitlines()
 frames={}
 for path in sorted(out.rglob('*.rgb565')):
  raw=path.read_bytes();decode(raw).save(path.with_suffix('.png'));frames[str(path.relative_to(out))]=hashlib.sha256(raw).hexdigest()
 for name,pattern in [('explore-enter','explore-enter-*.png'),('explore-leave','explore-leave-*.png')]:
  images=[Image.open(p) for p in sorted(out.glob(pattern))];images[0].save(out/(name+'.gif'),save_all=True,append_images=images[1:],duration=[100]*(len(images)-1)+[1200],loop=0)
 font=ImageFont.truetype(str(ROOT/'fonts/Vera.ttf'),13)
 entries=[('Driving · day','driving-day'),('Driving · night','driving-night'),('Destinations + itinerary','trip-night'),('Three-item menu','menu-night'),('Radio · phone still connected','driving-radio'),('Phone connection','bluetooth-night')]
 board=Image.new('RGB',(1020,950),(17,21,29));d=ImageDraw.Draw(board)
 for i,(title,frame) in enumerate(entries):
  x,y=20+(i%2)*500,14+(i//2)*310;d.text((x,y),title,font=font,fill=(220,227,240));board.paste(Image.open(out/(frame+'.png')),(x,y+24))
 board.save(out/'overview.png')
 assert pack_before=={p.name:(p.stat().st_size,p.stat().st_mtime_ns) for p in maps.glob('*') if p.is_file()}
 report={'renderer':'Actual ClioPlay Global C framebuffer and key handler; UBSan','framebuffer':'480x272 RGB565; decoded without drawing UI','map_unchanged':True,'results':results,'frames':frames,'source_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((ROOT/'clio843/src').glob('*.[ch]'))},'limits':'Host validation does not establish vehicle boot, frame rate, radio hardware or physical remote operation.'}
 (out/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print('Verified',len(frames),'native framebuffer snapshots.')
if __name__=='__main__':main()
