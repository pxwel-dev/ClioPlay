#!/usr/bin/env python3
"""Reproducible UBSan tests, global synthetic PBF and native framebuffer checks.

Pass --real-bundles to additionally exercise locally built Paris/Madrid bundles.
The fixtures are clearly synthetic and are never installed as real navigation.
"""
import argparse,json,math,os,shutil,struct,subprocess,sys,tempfile
from hashlib import sha256
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];B=ROOT/'navigation-builder';sys.path.insert(0,str(B/'tools'))
from global_regions import Regions
from build_cameras import build as cameras
from global_speeds import directions,global_self_test
from inspect_route import CPR2

def run(*args,**kw):return subprocess.run(list(map(str,args)),check=True,**kw)
def fixture(path,lat,lon,speed):
 nodes=[];ways=[]
 for y in range(3):
  for x in range(3):nodes.append(f'<node id="{1+y*3+x}" lat="{lat+y*.002:.7f}" lon="{lon+x*.002:.7f}"/>')
 for row in range(3):
  ids=[1+row*3+x for x in range(3)];ways.append(ids)
 for col in range(3):ways.append([1+y*3+col for y in range(3)])
 body=''.join(nodes)
 native_names=['Église Saint-Étienne','東京駅 八重洲口','شارع الملك فيصل','רחוב יפו','महात्मा गांधी मार्ग','улица Ленина']
 for i,ids in enumerate(ways):body+=f'<way id="{100+i}">'+''.join(f'<nd ref="{n}"/>' for n in ids)+f'<tag k="highway" v="primary"/><tag k="name" v="{native_names[i]}"/><tag k="maxspeed" v="{speed}"/></way>'
 body+=f'<node id="20" lat="{lat+.001}" lon="{lon+.001}"><tag k="place" v="village"/><tag k="name" v="Église 東京"/><tag k="addr:postcode" v="2000"/></node>'
 # Sort nodes before ways, as required by standard OSM tools.
 import xml.etree.ElementTree as ET
 root=ET.fromstring('<osm version="0.6">'+body+'</osm>');root[:]=sorted(root,key=lambda e:(0 if e.tag=='node' else 1,int(e.attrib['id'])))
 path.write_bytes(ET.tostring(root,encoding='utf-8',xml_declaration=True))

def compile_native(source,root,exe):
 flags=['-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-DCLIO_HOST_ROOT="'+str(root)+'/"']
 if source=='global-map.c':
  from text_engine import build
  obj=exe.with_suffix('.o');run('cc',*flags,'-c',ROOT/'tests'/source,'-o',obj);run('c++',*flags,obj,build('host-pool'),'-o',exe)
 else:run('cc',*flags,ROOT/'tests'/source,'-o',exe)

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--real-bundles',action='store_true');ap.add_argument('--cache',type=Path,default=B/'.cache');a=ap.parse_args();global_self_test()
 # Cases cover exact values beyond the old 120mph ceiling/conversion, directions,
 # lanes and unavailable conditions. Metadata never labels route heuristics legal.
 assert directions({'maxspeed':'130'})==((130,0),(130,0))
 assert directions({'maxspeed':'50','maxspeed:lanes:forward':'40|50'})[0]==(0,16)
 assert directions({'maxspeed:forward':'100','maxspeed:backward':'80'})==((100,0),(80,0))
 assert directions({'maxspeed':'50','maxspeed:forward:conditional':'30 @ (wet)'})[0]==(0,8)
 assert directions({'highway':'motorway'})==((0,0),(0,0))
 report={'native':{},'synthetic':{},'real':{}}
 r=Regions();out=ROOT/'docs/global-framebuffer';out.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='clioplay-global-') as temp:
  temp=Path(temp);world=temp/'world';p=world/'clio843/maps';p.mkdir(parents=True);r.write(p/'regions.cpg');cameras(None,[ROOT/'tests/cameras.csv'],p/'cameras.cpc',r);shutil.copyfile(ROOT/'clio843/font.bin',p.parent/'font.bin')
  if a.real_bundles:(p/'map.cpm').symlink_to(ROOT/'build/madrid/map.cpm')
  for name in ('global-units','global-cameras'):
   exe=temp/name;compile_native(name+'.c',world,exe);args=[exe]+([out/'camera.rgb565'] if name=='global-cameras' else []);result=subprocess.check_output(list(map(str,args)),text=True);print(result.strip());report['native'][name]=result.strip()
  bundles={}
  for name,lat,lon,speed in [('san-francisco',37.77,-122.42,'55 mph'),('sydney',-33.86,151.20,'130')]:
   work=ROOT/'build/global-fixtures'/name;work.mkdir(parents=True,exist_ok=True);xml=work/'fixture.osm';pbf=work/'fixture.osm.pbf';fixture(xml,lat,lon,speed);run('osmium','cat',xml,'-o',pbf,'--overwrite')
   bundle=work/'bundle'
   # Full real builder pipeline, including polygons/coastlines, Unicode labels and QAD.
   with (work/'build.log').open('w') as log:run(sys.executable,B/'build_navigation.py',pbf,'--yes','--country','US' if name=='san-francisco' else 'AU','--output',bundle,'--work',work/'work','--cache',a.cache,stdout=log,stderr=subprocess.STDOUT)
   # Native road/place labels must survive without a destination search pack.
   raw=(bundle/'map.cpm').read_bytes()
   for label in ('東京','شارع','улица'):
    assert label.encode() in raw,label
   manifest=json.loads((bundle/'build.json').read_text())
   assert manifest['builder']=='global-navigation-2'
   assert 'search.cps' not in manifest['files'] and not (bundle/'search.cps').exists()
   assert {p.name for p in bundle.iterdir() if p.is_file()}==set(manifest['files'])|{'build.json'}
   bundles[name]=bundle;report['synthetic'][name]=json.loads((bundle/'build.json').read_text())
  if a.real_bundles:
   for name in ('paris','madrid'):bundles[name]=ROOT/'build'/name
  for name,bundle in bundles.items():
   target=temp/name;p=target/'clio843/maps';p.mkdir(parents=True)
   for f in bundle.iterdir():
    if f.is_file():(p/f.name).symlink_to(f)
   shutil.copyfile(ROOT/'clio843/font.bin',p.parent/'font.bin');c=CPR2(bundle/'route.cpr');choice=None
   # A reachable, non-trivial query selected independently with the reference graph.
   for i in range(c.n):
    for j in range(c.n-1,-1,-1):
     f,t=c.node(i),c.node(j)
     if i!=j and abs(f[0]-t[0])+abs(f[1]-t[1])>250 and c.base_route(i,j):choice=(f[0],f[1],t[0],t[1]);break
    if choice:break
   assert choice,name
   exe=temp/(name+'-native');compile_native('global-map.c',target,exe);frames=out/name;frames.mkdir(exist_ok=True)
   result=subprocess.check_output([str(exe),str(frames),*map(str,choice)],text=True);print(result.strip());report['real' if name in ('paris','madrid') else 'synthetic'][name+'-native']=result.strip()
   run(sys.executable,B/'tools/inspect_route.py',bundle/'route.cpr','--validate','100')
 (out/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
 print('Global tests passed. Framebuffer output: '+str(out))
if __name__=='__main__':main()
