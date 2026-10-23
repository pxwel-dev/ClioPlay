#!/usr/bin/env python3
"""Capture the real splash framebuffer, check boot behaviour and show both themes."""
import hashlib,json,subprocess,tempfile,time
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont,ImageChops
from verify import decode
import build_splash_icon
ROOT=Path(__file__).resolve().parents[1]

def main():
 build_splash_icon.main();out=ROOT/'docs/next-release/splash-antialias';out.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='clioplay-splash-') as name:
  temp=Path(name);(temp/'clio843').mkdir();exe=temp/'test'
  subprocess.run(['cc','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-DCLIO_HOST_ROOT="'+str(temp)+'/"',str(ROOT/'tests/splash.c'),'-o',str(exe)],check=True)
  start=time.monotonic();result=subprocess.check_output([str(exe),str(out)],text=True);elapsed=time.monotonic()-start;print(result)
 font=ImageFont.truetype(str(ROOT/'fonts/Vera.ttf'),14);board=Image.new('RGB',(1000,620),(23,26,32));draw=ImageDraw.Draw(board);changes={}
 for row,theme in enumerate(('day','night')):
  path=out/f'splash-{theme}.rgb565';after=decode(path.read_bytes());after.save(path.with_suffix('.png'))
  before=out/'before'/path.name
  original=decode(before.read_bytes()) if before.exists() else Image.open(before.with_suffix('.png')).convert('RGB') if before.with_suffix('.png').exists() else None
  if original is not None:
   original.save(before.with_suffix('.png'));box=ImageChops.difference(original,after).getbbox();assert box
   assert box[2]-box[0]<=72 and box[3]-box[1]<=72,box
   # Every changed pixel belongs to the icon: retain the existing wordmark,
   # its antialiased glyphs, spacing and whole-group centring exactly.
   assert original.crop((0,box[3],480,272)).tobytes()==after.crop((0,box[3],480,272)).tobytes()
   changes[theme]={'changed_bounds':box,'wordmark_and_layout':'unchanged outside icon'}
   for col,(caption,img) in enumerate([('Original',original),('Antialiased',after)]):
    x,y=10+col*500,10+row*305;draw.text((x,y),f'{caption} · {theme}',font=font,fill='white');board.paste(img,(x,y+23))
  else:
   draw.text((10,10+row*305),theme,font=font,fill='white');board.paste(after,(10,33+row*305))
 board.save(out/'comparison.png')
 # Nearest-neighbour enlargement exposes real pixels without adding smoothing.
 zoom=Image.new('RGB',(620,660),(23,26,32));zd=ImageDraw.Draw(zoom)
 for row,theme in enumerate(('day','night')):
  if theme not in changes:continue
  for col,folder in enumerate((out/'before',out)):
   p=folder/f'splash-{theme}.png'
   if not p.exists():continue
   box=changes[theme]['changed_bounds'];cx=(box[0]+box[2])//2;cy=(box[1]+box[3])//2
   crop=Image.open(p).crop((cx-36,cy-36,cx+36,cy+36)).resize((288,288),Image.Resampling.NEAREST)
   x,y=10+col*310,10+row*330;zd.text((x,y),('Original' if col==0 else 'Antialiased')+' · '+theme,font=font,fill='white');zoom.paste(crop,(x,y+20))
 zoom.save(out/'icon-detail.png')
 report={'renderer':'Actual native C splash, 480x272 RGB565; UBSan','native':result,'before_after':changes,'supersampling':'8x8 coverage generated offline; direct RGB565 composite at boot, no enlarged runtime framebuffer','host_test_seconds':elapsed,'hardware':'not tested; host time is not head-unit timing','source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'clio843/src/cliospl.c',ROOT/'clio843/src/splash_icon.h',ROOT/'tools/build_splash_icon.py',ROOT/'tests/splash.c']},'frame_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob('*.rgb565')}}
 (out/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
