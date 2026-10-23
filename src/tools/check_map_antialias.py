#!/usr/bin/env python3
"""Compare the actual map renderer with/without edge coverage and the saved baseline."""
import hashlib,json,shutil,subprocess,tempfile
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
from verify import decode
ROOT=Path(__file__).resolve().parents[1]
SCENES=[(mode,z,night) for night in (0,1) for mode in (0,1) for z in (4,5,6)]
BENCH=r'''
static double seconds(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (double)t.tv_sec+(double)t.tv_nsec/1e9;}
int main(int argc,char**argv){
 assert(argc==2);outdir=argv[1];assert(load_font());assert(load_map());
 for(int night=0;night<2;night++)for(int explore=0;explore<2;explore++)for(int z=4;z<=6;z++){
  fixture();pref.theme=night?THEME_NIGHT:THEME_DAY;apply_palette(night);screen=SCR_MAP;manual_zoom=freecam_zoom=z;
  if(explore){enter_freecam();for(int i=0;i<5;i++)freecam_orientation_anim_step();}
  draw_map();char name[64];snprintf(name,sizeof(name),"%s-%s-z%d",night?"night":"day",explore?"explore":"drive",z);snapshot(name);
  double best=1e9,total=0;for(int sample=0;sample<7;sample++){double start=seconds();for(int i=0;i<12;i++)draw_map();double ms=(seconds()-start)*1000.0/12.0;total+=ms;if(ms<best)best=ms;}
  printf("%s %.6f %.6f\n",name,best,total/7.0);
 }
 close(mapfd);return 0;
}
'''
def main():
 out=ROOT/'docs/map-antialias';out.mkdir(parents=True,exist_ok=True)
 maps=(ROOT/'navigation-output').resolve();pack_before={p.name:(p.stat().st_size,p.stat().st_mtime_ns) for p in maps.glob('*') if p.is_file()}
 results={};frames={}
 with tempfile.TemporaryDirectory(prefix='clioplay-map-aa-') as td:
  tmp=Path(td);(tmp/'clio843/maps').mkdir(parents=True);shutil.copyfile(ROOT/'clio843/font.bin',tmp/'clio843/font.bin')
  for n in ('map.cpm','route.cpr','route.qadj'):(tmp/'clio843/maps'/n).symlink_to(maps/n)
  flags=['cc','-O2','-Wno-unused-function','-DCLIO_HOST_ROOT="'+str(tmp)+'/"']
  test=tmp/'aa-test';subprocess.run([*flags,'-fsanitize=undefined','-fno-sanitize-recover=all',str(ROOT/'tests/map-antialias.c'),'-o',str(test)],check=True)
  checked=subprocess.run([str(test)],text=True,capture_output=True,check=True);print(checked.stdout,end='')
  # Area topology regression uses the same production parser/filler, unchanged fixture.
  areatest=tmp/'areas';subprocess.run([*flags,'-fsanitize=undefined','-fno-sanitize-recover=all',str(ROOT/'tests/map-areas.c'),'-o',str(areatest)],check=True)
  areas=subprocess.run([str(areatest)],text=True,capture_output=True,check=True);print(areas.stdout,end='')
  harness=(ROOT/'tests/native.c').read_text().replace('int main(int argc,char**argv)','int unused_test_main(int argc,char**argv)').replace('#include "fixtures/route.h"','#include "'+str(ROOT/'tests/fixtures/route.h')+'"')
  (tmp/'benchmark.c').write_text(harness+BENCH)
  for name,source,extra in [('before',ROOT/'build/before-map-antialias/clio040.c',[]),('disabled',ROOT/'clio843/src/clio040.c',['-DMAP_ANTIALIAS=0']),('after',ROOT/'clio843/src/clio040.c',[])]:
   directory=out/name;directory.mkdir(exist_ok=True);exe=tmp/name
   subprocess.run([*flags,*extra,'-DCLIO_SOURCE="'+str(source)+'"',str(tmp/'benchmark.c'),'-o',str(exe)],check=True)
   run=subprocess.run([str(exe),str(directory)],text=True,capture_output=True,timeout=90,check=True)
   results[name]={}
   for line in run.stdout.splitlines():
    scene,best,mean=line.split();results[name][scene]={'best_ms':float(best),'mean_ms':float(mean)}
   for p in directory.glob('*.rgb565'):
    raw=p.read_bytes();decode(raw).save(p.with_suffix('.png'));frames[str(p.relative_to(out))]=hashlib.sha256(raw).hexdigest()
  for p in (out/'before').glob('*.rgb565'):assert p.read_bytes()==(out/'disabled'/p.name).read_bytes(),'Disabled AA must exactly match pre-change raster'
  # Check that the bridge oracle catches an intentional return of water after roads.
  source=(ROOT/'clio843/src/clio040.c').read_text();broken=source.replace('phase==0?MAP_DRAW_AREAS:(phase==1?MAP_DRAW_CONTEXT:MAP_DRAW_ROADS)','phase==0?MAP_DRAW_AREAS:(phase==1?MAP_DRAW_ROADS:MAP_DRAW_CONTEXT)')
  broken=broken.replace('map_mask_begin(phase==1?0:1)','map_mask_begin(phase==1?1:0)')
  assert broken!=source;(tmp/'wrong-layers.c').write_text(broken)
  exe=tmp/'wrong-layers';subprocess.run([*flags,'-I'+str(ROOT/'clio843/src'),'-DCLIO_SOURCE="'+str(tmp/'wrong-layers.c')+'"',str(ROOT/'tests/map-layers.c'),'-o',str(exe)],check=True)
  mutant=subprocess.run([str(exe)],text=True,capture_output=True);assert mutant.returncode!=0,'Bridge test must reject water drawn after roads';print('Bridge oracle rejects deliberately reversed water/road order:',mutant.stdout.strip())
 font=ImageFont.truetype(str(ROOT/'fonts/Vera.ttf'),13)
 board=Image.new('RGB',(1020,950),(17,21,29));d=ImageDraw.Draw(board)
 for row,scene in enumerate(('day-drive-z4','night-drive-z4','night-explore-z5')):
  for col,side in enumerate(('before','after')):
   x,y=20+col*500,14+row*312;d.text((x,y),scene+' · '+('Original edges' if side=='before' else 'Antialiased'),font=font,fill=(225,231,242));board.paste(Image.open(out/side/(scene+'.png')),(x,y+24))
 board.save(out/'comparison.png')
 board=Image.new('RGB',(1020,656),(17,21,29));d=ImageDraw.Draw(board)
 for row,scene in enumerate(('day-drive-z4','night-drive-z4')):
  for col,side in enumerate(('before','after')):
   x,y=20+col*500,14+row*320;d.text((x,y),('Original' if side=='before' else 'Antialiased')+' · 4× nearest-pixel crop',font=font,fill=(225,231,242))
   im=Image.open(out/side/(scene+'.png')).crop((290,106,410,176)).resize((480,280),Image.Resampling.NEAREST);board.paste(im,(x,y+24))
 board.save(out/'detail.png')
 assert pack_before=={p.name:(p.stat().st_size,p.stat().st_mtime_ns) for p in maps.glob('*') if p.is_file()}
 ratios={scene:results['after'][scene]['best_ms']/results['before'][scene]['best_ms'] for scene in results['before']}
 report={'renderer':'Actual production draw_map(), identical sample states and existing coastal map','checks':checked.stdout.splitlines()+areas.stdout.splitlines(),'disabled_matches_original':True,'bridge_oracle_mutation_detected':True,'mutation_output':mutant.stdout.strip(),'map_packs_unchanged':True,'host_timing':results,'best_time_ratios':ratios,'source_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((ROOT/'clio843/src').glob('*.[ch]'))},'frames':frames,'limits':'Warm-cache host CPU timings exclude framebuffer writes. These are not ARM frame-rate or head-unit performance measurements.'}
 (out/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
 print('Warm-cache host AA / original ratios:',json.dumps({k:round(v,2) for k,v in ratios.items()}));print('Real framebuffer comparisons:',out/'comparison.png')
if __name__=='__main__':main()
