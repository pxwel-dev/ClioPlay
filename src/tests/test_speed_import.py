"""Real CPM5 patch versus a fresh build; geometry and metadata isolation."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'navigation-builder/tools'))
from apply_speed_import import apply, patch_tile
from build_map import build_streams, open_speed_db, SpeedResolver, open_db, parse_way_stream, emit_final_map
from global_speeds import global_speed_scan
from inspect_map import HDR, IDX
from speed_import import SpeedImport


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def values(path):
    result = {}
    with path.open('rb') as f:
        h = HDR.unpack(f.read(HDR.size))
        f.seek(h[6]);rows = [IDX.unpack(f.read(IDX.size)) for _ in range(h[5])]
        for row in rows:
            f.seek(h[7]+row[4]);raw = f.read(row[5]);ng,nl,nm,_=struct.unpack_from('<4H',raw);p=8;geoms=[]
            for _ in range(ng):
                end=p+4+raw[p+2]*4;geoms.append((raw[p:p+3]+raw[p+4:end],raw[p+3]));p=end
            for _ in range(nl):p+=8+raw[p+6]
            metas=[]
            for _ in range(nm):
                end=p+5+raw[p+4];metas.append(raw[p:end]);p=end
            for geom,mi in geoms:result[(row[0],row[2],row[3],geom)]=None if mi==255 else metas[mi]
    return result


def main():
    with tempfile.TemporaryDirectory(prefix='clioplay-speed-import-test-') as td:
        w=Path(td);maps=w/'bundle';maps.mkdir();work=w/'work';work.mkdir()
        names=['SAME NAME','Rue École','Unchanged','SAME NAME','Conditional','Directional']
        tags=[{'oneway':'yes'},{},{'maxspeed':'20 mph'},{},
              {'maxspeed':'50','maxspeed:conditional':'30 @ (wet)'},
              {'maxspeed:forward':'80','maxspeed:backward':'60'}]
        objects=[];xml=['<osm version="0.6">']
        for i in range(6):
            lon=2+i*.003;coords=[[lon,48],[lon+.001,48.001]]
            for j,(x,y) in enumerate(coords):xml.append(f'<node id="{i*2+j+1}" lat="{y}" lon="{x}"/>')
            props={'@id':i+1,'highway':'residential','name':names[i],**tags[i]}
            objects.append(json.dumps({'type':'Feature','properties':props,'geometry':{'type':'LineString','coordinates':coords}})+'\n')
        from xml.sax.saxutils import escape
        for i in range(6):
            xml.append(f'<way id="{i+1}"><nd ref="{i*2+1}"/><nd ref="{i*2+2}"/>'+''.join(f'<tag k="{k}" v="{escape(v)}"/>' for k,v in {'highway':'residential','name':names[i],**tags[i]}.items())+'</way>')
        xml.append('</osm>');source=w/'source.osm';source.write_text(''.join(xml));pbf=w/'source.osm.pbf'
        subprocess.run(['osmium','cat',str(source),'-o',str(pbf)],check=True)
        correction=w/'limits.json';correction.write_text(json.dumps({'schema':'clioplay-speed-import-1','source':'Synthetic test signs','ways':{'1':'30 mph','2':'50','3':'30 mph','5':'70','6':'100'}}))
        build_streams(iter(objects),iter(()),maps/'map.cpm',work,demo=True)
        original=(maps/'map.cpm').read_bytes();before=values(maps/'map.cpm')
        (maps/'build.json').write_text(json.dumps({'builder':'global-navigation-2','files':{'map.cpm':{'bytes':len(original),'sha256':sha(maps/'map.cpm')}}}))
        backup=w/'backup';report=apply(maps,pbf,correction,backup)
        assert report['corrected_ways']==2 and report['preserved_tagged_ways']==3
        assert (backup/'map.cpm').read_bytes()==original
        after=values(maps/'map.cpm');assert before.keys()==after.keys()
        changed=[(key,meta) for key,meta in after.items() if meta!=before[key]]
        assert changed and all(key[0]==6 for key,_ in changed)
        for key,meta in after.items():
            if key[0]!=6 or meta is None:continue
            name=meta[5:].decode()
            if name=='Rue École':assert tuple(meta[:4])==(50,0,50,0)
            if name=='Conditional':assert tuple(meta[:4])==(0,8,0,8)
            if name=='Directional':assert tuple(meta[:4])==(80,0,60,0)
            if name=='Unchanged':assert tuple(meta[:4])==(20,2,20,2)
        same=[meta for key,meta in after.items() if key[0]==6 and meta is not None and meta[5:]==b'SAME NAME']
        assert any(tuple(m[:4])==(30,2,0,16) for m in same)
        assert any(tuple(m[:4])==(0,0,0,0) for m in same) # no name-based spillover
        stable=sha(maps/'map.cpm');manifest=(maps/'build.json').read_bytes()
        assert not apply(maps,pbf,correction,w/'unused-backup')['changed_tiles']
        assert sha(maps/'map.cpm')==stable and (maps/'build.json').read_bytes()==manifest
        # Fresh scan/tile output must encode exactly the same road values.
        sdb=open_speed_db(w/'speed.sqlite');imports=SpeedImport(correction);global_speed_scan(iter(objects),sdb,imports)
        db=open_db(w/'fresh.sqlite');stats,bounds=parse_way_stream(iter(objects),db,True,SpeedResolver(sdb))
        fresh=w/'fresh.cpm';emit_final_map(db,fresh,bounds,flags=1);db.close();sdb.close()
        assert values(fresh)==after
        bad=w/'bad.json';bad.write_text(json.dumps({'schema':'clioplay-speed-import-1','source':'test','ways':{'1':'30.5 mph'}}))
        try:SpeedImport(bad)
        except ValueError:pass
        else:raise AssertionError('Fractional limit accepted')
        print('Speed imports passed: mph/km/h, one-way direction, existing/conditional/directional evidence, shared metadata isolation, unchanged geometry, backup, idempotence and equality with fresh builder output.')


if __name__=='__main__':main()
