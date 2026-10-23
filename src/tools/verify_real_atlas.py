#!/usr/bin/env python3
"""Actual overlapping Hendaye/Irun packs versus the unsplit native route."""
import json,sys,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];B=ROOT/'navigation-builder';sys.path.insert(0,str(B/'tools'))
from atlas_format import write
work=ROOT/'build/real-border/atlas';work.mkdir(parents=True,exist_ok=True);out=work/'bundle';out.mkdir(exist_ok=True)
regions=[{'path':'west','name':'Irun','core':[-1.96608,43.25376,-1.80224,43.41760]},{'path':'east','name':'Hendaye','core':[-1.80224,43.25376,-1.63840,43.41760]}]
for r in regions:
 x,y,xx,yy=r['core'];pbf=work/(r['path']+'.osm.pbf');subprocess.run(['osmium','extract','--bbox',','.join(map(str,(x-.03,y-.03,xx+.03,yy+.03))),'--strategy','smart','-S','types=multipolygon,restriction,enforcement',str(ROOT/'build/real-border/border.osm.pbf'),'-o',str(pbf),'--overwrite'],check=True)
 with (work/(r['path']+'.log')).open('w') as f:subprocess.run([sys.executable,str(B/'build_navigation.py'),str(pbf),'--yes','--single-region','--portal-core',*map(str,r['core']),'--output',str(out/r['path']),'--work',str(work/(r['path']+'-work')),'--cache',str(ROOT/'navigation-builder/.cache')],stdout=f,stderr=subprocess.STDOUT,check=True)
report=write(out,regions)
for n in ('regions.cpg','timezones.cpt','cameras.cpc','fonts.cpf'):shutil.copyfile(out/'east'/n,out/n)
host=work/'host';(host/'clio843').mkdir(parents=True,exist_ok=True)
if not (host/'clio843/maps').exists():(host/'clio843/maps').symlink_to(out,target_is_directory=True)
shutil.copyfile(ROOT/'clio843/font.bin',host/'clio843/font.bin');exe=work/'real-atlas-test';subprocess.run(['cc','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-DCLIO_HOST_ROOT="'+str(host)+'/"',str(ROOT/'tests/atlas-real.c'),'-o',str(exe)],check=True)
# Select a connected actual road node beyond the source pack's overlap.
west=__import__('inspect_route').CPR2(out/'west/route.cpr');east=__import__('inspect_route').CPR2(out/'east/route.cpr')
from collections import Counter
component=Counter(west.node(i)[7] for i in range(west.nphys)).most_common(1)[0][0]
points=[west.node(i) for i in range(west.nphys)];points=[n for n in points if n[7]==component and 4330000<n[0]<4337000 and n[1]<east.bounds[1]-1000]
assert points,'No connected destination beyond overlap';target=min(points,key=lambda n:abs(n[0]-4333500)+abs(n[1]+187000))
r=subprocess.check_output([str(exe),str(ROOT/'build/real-border/bundle'),str(target[0]),str(target[1])],text=True);print(r);(ROOT/'docs/next-release/real-atlas-verification.json').write_text(json.dumps({'catalog':report,'native':r},indent=2)+'\n')
