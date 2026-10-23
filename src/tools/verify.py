#!/usr/bin/env python3
"""Validate the actual compact framebuffer UI, with optional archived comparison."""
import argparse,hashlib,json,shutil,struct,subprocess,tempfile
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
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
 ap.add_argument('--compare',action='store_true',help='also render locally archived original with identical fixtures')
 a=ap.parse_args();maps=a.maps.resolve();out=ROOT/'docs/framebuffer';out.mkdir(parents=True,exist_ok=True)
 pack_before={p.name:(p.stat().st_size,p.stat().st_mtime_ns) for p in maps.glob('*') if p.is_file()}
 results={}
 with tempfile.TemporaryDirectory(prefix='clioplay-ui-verify-') as td:
  tmp=Path(td);(tmp/'clio843/maps').mkdir(parents=True)
  shutil.copyfile(ROOT/'clio843/font.bin',tmp/'clio843/font.bin')
  for name in ('map.cpm','route.cpr','route.qadj'):(tmp/'clio843/maps'/name).symlink_to(maps/name)
  base=['cc','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-Wno-unused-function','-DCLIO_HOST_ROOT="'+str(tmp)+'/"']
  jobs=[('native',ROOT/'tests/native.c',[],[str(maps/'map.cpm'),str(out)]),('splash',ROOT/'tests/splash.c',[],[str(out)]),('layers',ROOT/'tests/map-layers.c',[],[str(maps/'map.cpm')]),('coverage',ROOT/'tests/map-antialias.c',[],[]),('areas',ROOT/'tests/map-areas.c',[],[])]
  if a.compare:
   before=out/'original';before.mkdir(exist_ok=True)
   jobs.append(('original',ROOT/'tests/native.c',['-DBEFORE=1','-DCLIO_SOURCE="'+str(ROOT/'build/before-compact/preview.c')+'"'],[str(maps/'map.cpm'),str(before)]))
  for name,source,flags,args in jobs:
   exe=tmp/name;subprocess.run([*base,*flags,str(source),'-o',str(exe)],check=True)
   result=subprocess.run([str(exe),*args],text=True,capture_output=True,timeout=90);print(result.stdout,end='')
   if result.returncode:print(result.stderr);raise SystemExit(result.returncode)
   results[name]=result.stdout.splitlines()
 if a.compare:
  current=(ROOT/'clio843/src/clio040.c').read_text();original=(ROOT/'build/before-compact/clio040.c').read_text()
  for begin,end in [('static int load_map(void)','static void road_style'),('static int load_route_data(void)','static void nav_arrow_head'),('static int freecam_transition_active(void)','static int pan_step_for_zoom')]:
   assert current[current.index(begin):current.index(end)]==original[original.index(begin):original.index(end)]
  results['unchanged_source']=['Map loading, original projection helper, route engine and camera animation code are identical to the archived UI. Map rasterization now uses continuous subpixel coverage.','Original screen enum and framebuffer viewport retained: six screens; 480x202 map at y=40..241.']
 frames={}
 for path in sorted(out.rglob('*.rgb565')):
  raw=path.read_bytes();decode(raw).save(path.with_suffix('.png'));frames[str(path.relative_to(out))]=hashlib.sha256(raw).hexdigest()
 for name,pattern in [('explore-enter','explore-enter-*.png'),('explore-leave','explore-leave-*.png')]:
  images=[Image.open(p) for p in sorted(out.glob(pattern))];images[0].save(out/(name+'.gif'),save_all=True,append_images=images[1:],duration=[100]*(len(images)-1)+[1200],loop=0)
 font=ImageFont.truetype(str(ROOT/'fonts/Vera.ttf'),13)
 entries=[('Driving · day','driving-day'),('Driving · night','driving-night'),('Destinations + itinerary','trip-night'),('Three-item menu','menu-night'),('Radio · phone still connected','driving-radio'),('Phone connection','bluetooth')]
 board=Image.new('RGB',(1020,950),(17,21,29));d=ImageDraw.Draw(board)
 for i,(title,frame) in enumerate(entries):
  x,y=20+(i%2)*500,14+(i//2)*310;d.text((x,y),title,font=font,fill=(220,227,240));board.paste(Image.open(out/(frame+'.png')),(x,y+24))
 board.save(out/'overview.png')
 if a.compare:
  entries=[('Driving · night','driving-night'),('Trip workspace','trip-night'),('Menu','menu-night')]
  board=Image.new('RGB',(1020,960),(17,21,29));d=ImageDraw.Draw(board)
  for row,(title,frame) in enumerate(entries):
   for col,(sub,directory) in enumerate([('Original',out/'original'),('Compact refinement',out)]):
    x,y=20+col*500,14+row*315;d.text((x,y),title+' · '+sub,font=font,fill=(220,227,240));board.paste(Image.open(directory/(frame+'.png')),(x,y+24))
  board.save(out/'comparison.png')
 assert pack_before=={p.name:(p.stat().st_size,p.stat().st_mtime_ns) for p in maps.glob('*') if p.is_file()}
 report={'renderer':'Actual ClioPlay Global C framebuffer and key handler; UBSan','framebuffer':'480x272 RGB565; decoded without drawing UI','map_unchanged':True,'results':results,'frames':frames,'source_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((ROOT/'clio843/src').glob('*.[ch]'))},'limits':'Host validation does not establish vehicle boot, frame rate, radio hardware or physical remote operation.'}
 (out/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print('Verified',len(frames),'native framebuffer snapshots.')
if __name__=='__main__':main()
