#!/usr/bin/env python3
"""Render actual roundabout guidance framebuffers and optional before/after views.

Requires Pillow and a host C compiler. Capture output must stay outside the
publication. Existing map data is read-only; these are controlled host states.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
from PIL import Image, ImageDraw, ImageFont
from verify_minor_road_aa import decode

ROOT=Path(__file__).resolve().parents[1]
CAPTURE=r'''
int main(int argc,char**argv){
 assert(argc==2);outdir=argv[1];assert(load_font());assert(load_map());
 for(int night=0;night<2;night++)for(int clockwise=0;clockwise<2;clockwise++){
  fixture();pref.theme=night?THEME_NIGHT:THEME_DAY;apply_palette(night);screen=SCR_MAP;
  region_flags=clockwise?4:0;nav_man_type=MAN_ROUNDABOUT;nav_man_exit=3;nav_man_idx=8;nav_man_m=180;
  draw_map();char name[80];snprintf(name,sizeof(name),"%s-%s",night?"night":"day",clockwise?"clockwise":"counterclockwise");snapshot(name);
  fill(0,0,W,H,C_HEADER);
  const int exits[]={0,1,2,3,4,12};
  for(int i=0;i<6;i++){nav_man_exit=exits[i];draw_manoeuvre_icon(40+i*80,40,MAN_ROUNDABOUT);}
  snprintf(name,sizeof(name),"icons-%s-%s",night?"night":"day",clockwise?"clockwise":"counterclockwise");snapshot(name);
 }
 return 0;
}
'''


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--maps',type=Path,required=True)
    ap.add_argument('--before-source',type=Path)
    ap.add_argument('--out',type=Path,required=True)
    a=ap.parse_args();out=a.out.resolve()
    if out.is_relative_to(ROOT.parent):ap.error('Keep renders outside the publication tree')
    out.mkdir(parents=True,exist_ok=True)
    harness=(ROOT/'tests/native.c').read_text().replace('int main(int argc,char**argv)','int unused_test_main(int argc,char**argv)').replace('#include "fixtures/route.h"','#include "'+str(ROOT/'tests/fixtures/route.h')+'"')
    sources=[('after',ROOT/'clio843/src/clio040.c')]
    if a.before_source:sources.insert(0,('before',a.before_source.resolve()))
    with tempfile.TemporaryDirectory(prefix='clioplay-roundabout-host-') as td:
        tmp=Path(td);(tmp/'clio843').mkdir()
        (tmp/'clio843/maps').symlink_to(a.maps.resolve(),target_is_directory=True)
        shutil.copyfile(ROOT/'clio843/font.bin',tmp/'clio843/font.bin')
        (tmp/'capture.c').write_text(harness+CAPTURE)
        for side,source in sources:
            directory=out/side;directory.mkdir(exist_ok=True);exe=tmp/side
            subprocess.run(['cc','-O2','-Wno-unused-function','-DCLIO_HOST_ROOT="'+str(tmp)+'/"','-DCLIO_SOURCE="'+str(source)+'"',str(tmp/'capture.c'),'-lm','-o',str(exe)],check=True)
            subprocess.run([str(exe),str(directory)],check=True,timeout=90)
            for p in directory.glob('*.rgb565'):decode(p.read_bytes()).save(p.with_suffix('.png'))
    font=ImageFont.truetype(str(ROOT/'fonts/Vera.ttf'),16)
    sides=[side for side,_ in sources]
    scenes=('night-clockwise','day-counterclockwise')
    board=Image.new('RGB',(16+496*len(sides),632),(17,21,29));d=ImageDraw.Draw(board)
    detail=Image.new('RGB',(16+496*len(sides),348),(17,21,29));dd=ImageDraw.Draw(detail)
    for row,scene in enumerate(scenes):
        for col,side in enumerate(sides):
            x,y=16+496*col,12+310*row
            title=side.title()+' · '+('UK / clockwise' if row==0 else 'Right-hand traffic')
            d.text((x,y),title,font=font,fill=(225,231,242))
            im=Image.open(out/side/(scene+'.png'));board.paste(im,(x,y+26))
            dy=12+170*row;dd.text((x,dy),title+' · 4× crop',font=font,fill=(225,231,242))
            detail.paste(im.crop((374,242,480,272)).resize((424,120),Image.Resampling.NEAREST),(x,dy+28))
    board.save(out/'comparison.png');detail.save(out/'detail.png')
    print('Production framebuffer renders:',out)


if __name__=='__main__':
    main()
