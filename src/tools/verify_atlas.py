#!/usr/bin/env python3
"""Two overlapping packs with an arrival-context restriction at their seam."""
import json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];B=ROOT/'navigation-builder';sys.path.insert(0,str(B/'tools'))
from atlas_format import write
from inspect_route import CPR2
X=-163840;Y=5500000
NODES={1:(Y,X-1000),2:(Y,X-200),3:(Y,X),4:(Y,X+200),5:(Y,X+1000),6:(Y+500,X+100),7:(Y+800,X-100),8:(Y+800,X+100)}
WAYS={10:[1,2],20:[2,3],30:[3,4],40:[4,5],50:[3,6],60:[6,4],70:[7,8]}
def xml(ids):
 nodes={n for i in ids for n in WAYS[i]};s=['<osm version="0.6">']
 for n in sorted(nodes):
  la,lo=NODES[n];s.append(f'<node id="{n}" lat="{la/1e5}" lon="{lo/1e5}"/>')
 for w in ids:
  s.append(f'<way id="{w}">'+''.join(f'<nd ref="{n}"/>' for n in WAYS[w])+f'<tag k="highway" v="residential"/><tag k="oneway" v="{"no" if w==70 else "yes"}"/><tag k="maxspeed" v="30 mph"/></way>')
 s.append('<relation id="900"><member type="way" ref="20" role="from"/><member type="node" ref="3" role="via"/><member type="way" ref="30" role="to"/><tag k="type" v="restriction"/><tag k="restriction" v="no_straight_on"/></relation></osm>');return ''.join(s)
def main():
 work=ROOT/'build/atlas-fixture';work.mkdir(parents=True,exist_ok=True);out=work/'bundle';out.mkdir(exist_ok=True);regions=[]
 for name,ids,core in [('west',[10,20,30,50,60,70],[(X-16384)/1e5,(Y//16384*16384)/1e5,X/1e5,(Y//16384*16384+16384)/1e5]),('east',[20,30,40,50,60,70],[X/1e5,(Y//16384*16384)/1e5,(X+16384)/1e5,(Y//16384*16384+16384)/1e5])]:
  source=work/(name+'.osm');source.write_text(xml(ids));pbf=source.with_suffix('.osm.pbf');subprocess.run(['osmium','cat',str(source),'-o',str(pbf),'--overwrite'],check=True)
  cmd=[sys.executable,str(B/'build_navigation.py'),str(pbf),'--yes','--single-region','--portal-core',*map(str,core),'--output',str(out/name),'--work',str(work/(name+'-work')),'--cache',str(ROOT/'navigation-builder/.cache')]
  with (work/(name+'.log')).open('w') as f:subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,check=True)
  regions.append({'path':name,'name':name,'core':core})
 report=write(out,regions);print(json.dumps(report,indent=2))
 for n in ('regions.cpg','timezones.cpt','fonts.cpf','cameras.cpc'):
  import shutil
  shutil.copyfile(out/'west'/n,out/n)
 # Unsplit independent reference, including the forbidden seam manoeuvre.
 full=work/'full.osm';full.write_text(xml(sorted(WAYS)));route=work/'reference.cpr'
 with (work/'reference.log').open('w') as f:subprocess.run([sys.executable,str(B/'tools/build_route.py'),'--pbf',str(full),'--out',str(route),'--work',str(work/'reference-work')],stdout=f,stderr=subprocess.STDOUT,check=True)
 # Exercise the actual atlas orchestrator, not only manually joined child packs.
 from install_nav import validate_bundle
 pipeline=work/'pipeline-bundle'
 cmd=[sys.executable,str(B/'build_navigation.py'),str(pbf),'--yes','--atlas','--country','GB','--output',str(pipeline),'--work',str(work/'pipeline-work'),'--cache',str(B/'.cache')]
 with (work/'pipeline.log').open('w') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
 manifest=validate_bundle(pipeline);assert manifest['builder']=='global-atlas-2'
 for region in manifest['regions']:
  pack=pipeline/region['path']
  assert {p.name for p in pack.iterdir()}=={'map.cpm','route.cpr','route.qadj'}
 # Publishing again also exercises cache reuse and an existing destination.
 with (work/'pipeline-resume.log').open('w') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
 validate_bundle(pipeline)
 assert {str(p.relative_to(pipeline)) for p in pipeline.rglob('*') if p.is_file()}==set(manifest['files'])|{'build.json'}
 r=CPR2(route);s=t=None
 for i in range(r.nphys):
  n=r.node(i)
  if n[:2]==NODES[1]:s=i
  if n[:2]==NODES[5]:t=i
 cost=r.base_route(s,t);assert cost is not None
 (work/'expected.json').write_text(json.dumps({'cost':cost,'start':NODES[1],'target':NODES[5],'required_detour':NODES[6]},indent=2)+'\n');print('Reference restricted cost:',cost)
 host=work/'host';(host/'clio843').mkdir(parents=True,exist_ok=True)
 maps=host/'clio843/maps'
 if not maps.exists():maps.symlink_to(out,target_is_directory=True)
 import shutil
 shutil.copyfile(ROOT/'clio843/font.bin',host/'clio843/font.bin')
 exe=work/'atlas-test';subprocess.run(['cc','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-DCLIO_HOST_ROOT="'+str(host)+'/'+'"',str(ROOT/'tests/atlas.c'),'-o',str(exe)],check=True)
 result=subprocess.check_output([str(exe),str(cost)],text=True);print(result)
 (ROOT/'docs/next-release/atlas-verification.json').write_text(json.dumps({'catalog':report,'reference_cost':cost,'native':result},indent=2)+'\n')
if __name__=='__main__':main()
