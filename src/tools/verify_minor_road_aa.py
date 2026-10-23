#!/usr/bin/env python3
"""Capture matching native RGB565 frames before/after a thin-road raster change.

Requires Pillow. Outputs must be outside the publication tree. Host timings are
relative checks, not head-unit frame-rate measurements. Map inputs are read-only.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import statistics
import subprocess
import tempfile
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
BENCH = r'''
static double seconds(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (double)t.tv_sec+(double)t.tv_nsec/1e9;}
int main(int argc,char**argv){
 assert(argc==2);outdir=argv[1];assert(load_font());assert(load_map());
 for(int night=0;night<2;night++)for(int explore=0;explore<2;explore++)for(int z=4;z<=6;z++){
  fixture();pref.theme=night?THEME_NIGHT:THEME_DAY;apply_palette(night);screen=SCR_MAP;manual_zoom=freecam_zoom=z;
  if(explore){enter_freecam();for(int i=0;i<5;i++)freecam_orientation_anim_step();}
  draw();char name[64];snprintf(name,sizeof(name),"%s-%s-z%d",night?"night":"day",explore?"explore":"drive",z);snapshot(name);
  double samples[5];for(int sample=0;sample<5;sample++){double start=seconds();for(int i=0;i<20;i++)draw_map();samples[sample]=(seconds()-start)*1000.0/20.0;}
  printf("%s",name);for(int i=0;i<5;i++)printf(" %.6f",samples[i]);puts("");
 }
 return 0;
}
'''


def decode(raw):
    import array
    import sys
    pixels = array.array('H', raw)
    if sys.byteorder != 'little':
        pixels.byteswap()
    rgb = bytearray()
    for p in pixels:
        r, g, b = p >> 11, (p >> 5) & 63, p & 31
        rgb.extend(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
    return Image.frombytes('RGB', (480, 272), bytes(rgb))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--maps', type=Path, required=True)
    ap.add_argument('--before-source', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    a = ap.parse_args()
    out = a.out.resolve()
    if out.is_relative_to(ROOT.parent):
        ap.error('captures belong outside the publication tree')
    out.mkdir(parents=True, exist_ok=True)
    results, frames = {}, {}
    harness = (ROOT/'tests/native.c').read_text().replace('int main(int argc,char**argv)', 'int unused_test_main(int argc,char**argv)').replace('#include "fixtures/route.h"', '#include "'+str(ROOT/'tests/fixtures/route.h')+'"')
    with tempfile.TemporaryDirectory(prefix='clioplay-thin-aa-') as td:
        tmp = Path(td)
        (tmp/'clio843').mkdir()
        (tmp/'clio843/maps').symlink_to(a.maps.resolve(), target_is_directory=True)
        shutil.copyfile(ROOT/'clio843/font.bin', tmp/'clio843/font.bin')
        (tmp/'capture.c').write_text(harness+BENCH)
        for side, source in [('before', a.before_source.resolve()), ('after', ROOT/'clio843/src/clio040.c')]:
            directory = out/side
            directory.mkdir(exist_ok=True)
            exe = tmp/side
            subprocess.run(['cc', '-O2', '-Wno-unused-function', '-DCLIO_HOST_ROOT="'+str(tmp)+'/"', '-DCLIO_SOURCE="'+str(source)+'"', str(tmp/'capture.c'), '-o', str(exe)], check=True)
            run = subprocess.run([str(exe), str(directory)], text=True, capture_output=True, timeout=120, check=True)
            results[side] = {line.split()[0]:statistics.median(map(float, line.split()[1:])) for line in run.stdout.splitlines()}
            for p in directory.glob('*.rgb565'):
                raw = p.read_bytes()
                decode(raw).save(p.with_suffix('.png'))
                frames[str(p.relative_to(out))] = hashlib.sha256(raw).hexdigest()
    font = ImageFont.truetype(str(ROOT/'fonts/Vera.ttf'), 16)
    scenes = ('night-drive-z4', 'day-explore-z5')
    board = Image.new('RGB', (1008, 632), (17, 21, 29))
    d = ImageDraw.Draw(board)
    for row, scene in enumerate(scenes):
        for col, side in enumerate(('before', 'after')):
            x, y = 16+col*496, 12+row*310
            d.text((x, y), side.title()+' — '+('Night driving' if row==0 else 'Day explore'), font=font, fill=(225,231,242))
            board.paste(Image.open(out/side/(scene+'.png')), (x,y+26))
    board.save(out/'comparison.png')
    board = Image.new('RGB', (1008, 588), (17,21,29))
    d = ImageDraw.Draw(board)
    for row, scene in enumerate(scenes):
        for col, side in enumerate(('before','after')):
            x,y = 16+col*496, 12+row*288
            d.text((x,y), side.title()+' — 4× nearest-pixel crop', font=font, fill=(225,231,242))
            im = Image.open(out/side/(scene+'.png')).crop((350,45,470,105) if row==0 else (300,60,420,120)).resize((480,240),Image.Resampling.NEAREST)
            board.paste(im,(x,y+26))
    board.save(out/'detail.png')
    ratios = {k:results['after'][k]/v for k,v in results['before'].items()}
    report = {'renderer':'Production draw() and RGB565 framebuffer; identical map, camera, palette and UI state', 'host_median_ms':results, 'host_ratios':ratios, 'frames':frames, 'limits':'Host timing only; not physical Carminat validation.'}
    (out/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:round(v,3) for k,v in ratios.items()}))
    print(out/'comparison.png')


if __name__ == '__main__':
    main()
