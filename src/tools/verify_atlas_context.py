#!/usr/bin/env python3
"""A long active via-way restriction crossing two pack boundaries."""
import json,subprocess,sys,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];B=ROOT/'navigation-builder';sys.path.insert(0,str(B/'tools'))
from atlas_format import write
from inspect_route import CPR2
work=ROOT/'build/atlas-context';work.mkdir(parents=True,exist_ok=True);out=work/'bundle';out.mkdir(exist_ok=True);x=-163840;y=5500000
nodes={1:(y,x-5000),2:(y,x-4500),3:(y,x-4000),4:(y,x+20000),5:(y,x+21384),6:(y+500,x+8000)};ways={10:[1,2],20:[2,3],30:[3,6,4],40:[4,5],90:[3,5]}
parts=['<osm version="0.6">']
for n,(la,lo) in nodes.items():parts.append(f'<node id="{n}" lat="{la/1e5}" lon="{lo/1e5}"/>')
for w,ids in ways.items():parts.append(f'<way id="{w}">'+''.join(f'<nd ref="{n}"/>' for n in ids)+f'<tag k="highway" v="residential"/><tag k="oneway" v="yes"/><tag k="maxspeed" v="{100 if w==90 else 30} mph"/></way>')
parts.append('<relation id="900"><member type="way" ref="20" role="from"/><member type="way" ref="30" role="via"/><member type="way" ref="40" role="to"/><tag k="type" v="restriction"/><tag k="restriction" v="only_straight_on"/></relation></osm>');source=work/'source.osm';source.write_text(''.join(parts));pbf=work/'source.osm.pbf';subprocess.run(['osmium','cat',str(source),'-o',str(pbf),'--overwrite'],check=True);regions=[]
for i,name in enumerate(('west','middle','east')):
 west=x+(i-1)*16384;south=y//16384*16384;core=[west/1e5,south/1e5,(west+16384)/1e5,(south+16384)/1e5];regional=work/(name+'.osm.pbf');a,b,c,d=core
 subprocess.run(['osmium','extract','--bbox',','.join(map(str,(a-.03,b-.03,c+.03,d+.03))),'--strategy','smart','-S','types=multipolygon,restriction,enforcement',str(pbf),'-o',str(regional),'--overwrite'],check=True)
 with (work/(name+'.log')).open('w') as f:subprocess.run([sys.executable,str(B/'build_navigation.py'),str(regional),'--yes','--single-region','--portal-core',*map(str,core),'--output',str(out/name),'--work',str(work/(name+'-work')),'--cache',str(ROOT/'navigation-builder/.cache')],stdout=f,stderr=subprocess.STDOUT,check=True)
 regions.append({'path':name,'name':name,'core':core})
report=write(out,regions)
for name in ('regions.cpg','timezones.cpt','fonts.cpf','cameras.cpc'):shutil.copyfile(out/'middle'/name,out/name)
reference=work/'reference.cpr'
with (work/'reference.log').open('w') as f:subprocess.run([sys.executable,str(B/'tools/build_route.py'),'--pbf',str(pbf),'--out',str(reference),'--work',str(work/'reference-work')],stdout=f,stderr=subprocess.STDOUT,check=True)
r=CPR2(reference);ids={r.node(i)[:2]:i for i in range(r.nphys)};source_ctx=None
r.f.seek(r.go+r.ng*8);wayids=[int.from_bytes(r.f.read(8),'little') for i in range(r.nb)]
for i,w in enumerate(wayids):
 if w==20:source_ctx=r.base(i)[3]
assert source_ctx is not None and source_ctx>=r.nphys
costs=[r.base_route(ids[nodes[1]],ids[nodes[5]]),r.base_route(source_ctx,ids[nodes[5]])];assert all(c is not None for c in costs)
host=work/'host';(host/'clio843').mkdir(parents=True,exist_ok=True)
if not (host/'clio843/maps').exists():(host/'clio843/maps').symlink_to(out,target_is_directory=True)
shutil.copyfile(ROOT/'clio843/font.bin',host/'clio843/font.bin');exe=work/'context-test';subprocess.run(['cc','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-DCLIO_HOST_ROOT="'+str(host)+'/"',str(ROOT/'tests/atlas-context.c'),'-o',str(exe)],check=True);result=subprocess.check_output([str(exe),*map(str,costs)],text=True);print(result);(ROOT/'docs/next-release/atlas-context-verification.json').write_text(json.dumps({'catalog':report,'reference_costs':costs,'native':result},indent=2)+'\n')
