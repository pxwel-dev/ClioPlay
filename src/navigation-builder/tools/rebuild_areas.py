#!/usr/bin/env python3
"""Replace CPM5 area boundaries from OSM polygons; preserve existing road/label data."""
import argparse
import hashlib
import itertools
import json
import math
import os
from pathlib import Path
import shutil
import sqlite3
import struct
import subprocess
import time
import xml.etree.ElementTree as ET

from shapely import make_valid, set_precision
from shapely.affinity import scale
from shapely.geometry import box, shape
from shapely.prepared import prep

from build_map import HDR, IDX, SPANS, SIMPLIFY_TOL, MAX_TILE_BYTES, geom_blob, qlocal, dedupe_points

AREA = 128
FILTERS = ['wr/natural=water,wood','wr/leisure=park,nature_reserve',
           'wr/landuse=forest','wr/waterway=riverbank']


def log(text):
    print(text,flush=True)


def fingerprint(path):
    s=path.stat()
    return {'path':str(path.resolve()),'bytes':s.st_size,'mtime_ns':s.st_mtime_ns}


def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(4*1024*1024),b''): h.update(chunk)
    return h.hexdigest()


def classification(props):
    if props.get('natural')=='water' or props.get('waterway')=='riverbank': return 1,4,3
    if props.get('leisure')=='nature_reserve': return 2,3,3
    if props.get('leisure')=='park': return 2,1,4
    if props.get('natural')=='wood' or props.get('landuse')=='forest': return 2,2,4
    return None


def polygons(geometry):
    if geometry.is_empty: return
    if geometry.geom_type=='Polygon': yield geometry
    elif hasattr(geometry,'geoms'):
        for part in geometry.geoms: yield from polygons(part)


def contour(poly,tx,ty,span):
    """One closed even/odd contour; doubled bridges cancel without visible seams."""
    def ring(coords):
        pts=dedupe_points([(qlocal(x,tx*span,span),qlocal(y,ty*span,span)) for x,y in coords])
        if len(pts)<4: return []
        if pts[-1]!=pts[0]: pts.append(pts[0])
        area=sum(a[0]*b[1]-a[1]*b[0] for a,b in zip(pts,pts[1:]))
        return pts if area else []
    outer=ring(poly.exterior.coords)
    if not outer: return []
    points=list(outer)
    for interior in poly.interiors:
        hole=ring(interior.coords)
        if hole: points.extend(hole+[outer[0]])
    return points


def area_blob(layer,style,points):
    blob=bytearray(geom_blob(layer,style|AREA,points))
    blob[3]=255  # non-road geometry has no road-metadata reference
    return bytes(blob)


def bounded_contours(poly,tx,ty,span,depth=0):
    """Split overly complex clipped polygons geometrically, never by vertex count."""
    points=contour(poly,tx,ty,span)
    if not points: return
    if len(points)<=255:
        yield points
        return
    if depth>=16: raise ValueError('polygon cannot fit the 255-point CPM limit')
    x0,y0,x1,y1=poly.bounds
    if x1-x0>=y1-y0:
        mid=(x0+x1)/2;clips=(box(x0,y0,mid,y1),box(mid,y0,x1,y1))
    else:
        mid=(y0+y1)/2;clips=(box(x0,y0,x1,mid),box(x0,mid,x1,y1))
    for clip in clips:
        for part in polygons(poly.intersection(clip,grid_size=span/4095)):
            yield from bounded_contours(part,tx,ty,span,depth+1)


def tile_polygon(poly,minlevel,maxlevel=6,fine=False):
    for level,span in enumerate(SPANS):
        if level<minlevel or level>maxlevel: continue
        # Simplify once per complete polygon, so adjacent tiles share boundaries.
        tolerance=SIMPLIFY_TOL[6]*SPANS[6]/4095 if fine and level==maxlevel else SIMPLIFY_TOL[level]*span/4095
        simplified=poly.simplify(tolerance,preserve_topology=True)
        for region in polygons(simplified):
            prepared=prep(region);x0,y0,x1,y1=region.bounds
            for tx in range(math.floor(x0/span),math.ceil(x1/span)):
                for ty in range(math.floor(y0/span),math.ceil(y1/span)):
                    if not(-100000<=tx<=100000 and -100000<=ty<=100000):
                        raise ValueError('area outside CPM5 global tile-coordinate range')
                    clip=box(tx*span,ty*span,(tx+1)*span,(ty+1)*span)
                    if prepared.covers(clip):
                        yield level,tx,ty,[(0,0),(4095,0),(4095,4095),(0,4095),(0,0)]
                    elif prepared.intersects(clip):
                        clipped=set_precision(region.intersection(clip,grid_size=span/4095),span/4095)
                        for part in polygons(clipped):
                            for points in bounded_contours(part,tx,ty,span):
                                yield level,tx,ty,points


def relation_outer_members(filtered):
    """Suppress same-kind outer ways that would paint over a relation's holes."""
    members=set()
    proc=subprocess.Popen(['osmium','cat',str(filtered),'-t','relation','-f','osm'],stdout=subprocess.PIPE)
    try:
        for _,element in ET.iterparse(proc.stdout,events=('end',)):
            if element.tag!='relation':continue
            props={t.attrib['k']:t.attrib['v'] for t in element.findall('tag')}
            kind=classification(props)
            if props.get('type')=='multipolygon' and kind:
                for member in element.findall('member'):
                    if member.attrib.get('type')=='way' and member.attrib.get('role','') in ('','outer'):
                        members.add((int(member.attrib['ref']),kind[0],kind[1]))
            element.clear()
    except BaseException:
        proc.terminate();proc.wait();raise
    finally:proc.stdout.close()
    if proc.wait()!=0:raise RuntimeError('relation membership scan failed')
    log(f'  authoritative multipolygon outer memberships={len(members):,}')
    return members


def build_index(lines,database,suppressed=()):
    conn=sqlite3.connect(database)
    conn.execute('pragma journal_mode=off');conn.execute('pragma synchronous=off')
    conn.execute('pragma temp_store=file');conn.execute('pragma cache_size=-131072')
    conn.execute('create table areas(z int,tx int,ty int,layer int,style int,blob blob)')
    stats={'features':0,'polygons':0,'repaired':0,'records':0,'holes':0,'suppressed_duplicate_members':0,'bounds_e5':None}
    batch=[];last=time.monotonic()
    try:
        for line in lines:
            line=line.strip('\x1e\r\n ')
            if not line: continue
            obj=json.loads(line);kind=classification(obj.get('properties') or {})
            if not kind: continue
            props=obj.get('properties') or {}
            if props.get('@type')=='way' and (props.get('@id'),kind[0],kind[1]) in suppressed:
                stats['suppressed_duplicate_members']+=1
                continue
            geometry=shape(obj['geometry'])
            if not geometry.is_valid:
                geometry=make_valid(geometry);stats['repaired']+=1
            geometry=scale(geometry,xfact=1e5,yfact=1e5,origin=(0,0))
            layer,style,minlevel=kind;stats['features']+=1
            for poly in polygons(geometry):
                stats['polygons']+=1;stats['holes']+=len(poly.interiors)
                x0,y0,x1,y1=poly.bounds
                b=[math.floor(y0),math.floor(x0),math.ceil(y1),math.ceil(x1)]
                if stats['bounds_e5'] is None: stats['bounds_e5']=b
                else:
                    prev=stats['bounds_e5'];stats['bounds_e5']=[min(b[0],prev[0]),min(b[1],prev[1]),max(b[2],prev[2]),max(b[3],prev[3])]
                for z,tx,ty,points in tile_polygon(poly,minlevel):
                    batch.append((z,tx,ty,layer,style,area_blob(layer,style,points)))
                    stats['records']+=1
                    if len(batch)>=10000:
                        conn.executemany('insert into areas values(?,?,?,?,?,?)',batch);batch.clear()
            if time.monotonic()-last>10:
                log(f"  polygons={stats['polygons']:,} holes={stats['holes']:,} records={stats['records']:,}")
                last=time.monotonic()
        if batch: conn.executemany('insert into areas values(?,?,?,?,?,?)',batch)
        conn.commit();log('Indexing polygon tiles...')
        conn.execute('create index tilekey on areas(z,tx,ty,layer desc)');conn.commit()
    finally: conn.close()
    return stats


def combine_tile(original,areas,stats):
    if original:
        ng,nl,nm,res=struct.unpack_from('<4H',original);pos=8;retained=[]
        replaced={(a[0],a[1]) for a in areas}
        for _ in range(ng):
            layer,style,np,mi=original[pos:pos+4];end=pos+4+np*4
            if end>len(original): raise ValueError('truncated base geometry')
            if (layer,style&127) in replaced:
                stats['replaced_boundaries']+=1
            else: retained.append(original[pos:end])
            pos=end
        tail=original[pos:]
    else: nl=nm=res=0;retained=[];tail=b''
    used=8+sum(map(len,retained))+len(tail);kept=[]
    for layer,style,blob in areas:
        if used+len(blob)<=MAX_TILE_BYTES:
            kept.append(blob);used+=len(blob)
        else: stats['dropped_area_records']+=1
    geoms=kept+retained
    stats['kept_area_records']+=len(kept)
    return struct.pack('<4H',len(geoms),nl,nm,res)+b''.join(geoms)+tail,len(geoms)+nl+nm


def merge_map(base,database,output,area_bounds):
    stats={'tiles':0,'kept_area_records':0,'dropped_area_records':0,'replaced_boundaries':0,
           'new_tiles':0,'max_tile_bytes':0}
    conn=sqlite3.connect(database)
    cursor=conn.execute('select z,tx,ty,layer,style,blob from areas order by z,tx,ty,layer desc')
    groups=itertools.groupby(cursor,key=lambda row:row[:3])
    def next_area():
        try:
            key,rows=next(groups)
            return key,[(r[3],r[4],r[5]) for r in rows]
        except StopIteration: return None,[]
    index_tmp=output.with_suffix('.index.tmp');data_tmp=output.with_suffix('.data.tmp')
    final_tmp=output.with_suffix('.new');offset=0;last=time.monotonic()
    try:
        with base.open('rb') as ix,base.open('rb') as source,index_tmp.open('wb') as index,data_tmp.open('wb') as data:
            h=HDR.unpack(ix.read(HDR.size))
            if h[:2]!=(b'CPM5',5) or h[4]&1: raise ValueError('base must be a production CPM5 map')
            if h[4]&2: raise ValueError('base already contains the new polygon format; use the original map')
            ix.seek(h[6]);remaining=h[5]
            def next_base():
                nonlocal remaining
                if not remaining:return None,None
                remaining-=1;r=IDX.unpack(ix.read(IDX.size));return (r[0],r[2],r[3]),r
            bk,record=next_base();ak,areas=next_area()
            while bk is not None or ak is not None:
                key=bk if ak is None or (bk is not None and bk<=ak) else ak
                original=b''
                if key==bk:
                    source.seek(h[7]+record[4]);original=source.read(record[5])
                    if len(original)!=record[5]:raise ValueError('truncated base tile')
                else:stats['new_tiles']+=1
                tile,count=combine_tile(original,areas if key==ak else [],stats)
                if len(tile)>MAX_TILE_BYTES:raise ValueError('tile cache cap exceeded')
                z,tx,ty=key
                index.write(IDX.pack(z,0,tx,ty,offset,len(tile),count,0));data.write(tile)
                offset+=len(tile);stats['tiles']+=1;stats['max_tile_bytes']=max(stats['max_tile_bytes'],len(tile))
                if HDR.size+stats['tiles']*IDX.size+offset>0x7fffffff:
                    raise ValueError('map exceeds the runtime signed 2 GiB seek limit')
                if key==bk:bk,record=next_base()
                if key==ak:ak,areas=next_area()
                if time.monotonic()-last>10:
                    log(f"  merged tiles={stats['tiles']:,} areas={stats['kept_area_records']:,} data={offset/1024**2:.1f} MiB")
                    last=time.monotonic()
            bounds=list(h[8:12])
            if area_bounds:
                bounds=[min(bounds[0],area_bounds[0]),min(bounds[1],area_bounds[1]),max(bounds[2],area_bounds[2]),max(bounds[3],area_bounds[3])]
        with final_tmp.open('wb') as out:
            out.write(HDR.pack(b'CPM5',5,h[2],h[3],h[4]|2,stats['tiles'],HDR.size,HDR.size+stats['tiles']*IDX.size,*bounds))
            for path in (index_tmp,data_tmp):
                with path.open('rb') as src:shutil.copyfileobj(src,out,4*1024*1024)
            out.flush();os.fsync(out.fileno())
        os.replace(final_tmp,output)
    finally:conn.close()
    index_tmp.unlink();data_tmp.unlink()
    stats['bytes']=output.stat().st_size
    return stats


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base',type=Path,required=True);ap.add_argument('--pbf',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True);ap.add_argument('--work',type=Path,required=True)
    ap.add_argument('--filtered',type=Path)
    a=ap.parse_args()
    base=a.base.expanduser().resolve();pbf=a.pbf.expanduser().resolve();out=a.out.expanduser().resolve()
    work=a.work.expanduser().resolve();work.mkdir(parents=True,exist_ok=True);out.parent.mkdir(parents=True,exist_ok=True)
    if out in (base,pbf):raise SystemExit('output must differ from both inputs')
    manifest={'builder':'complete-cpm4-areas-v2','source':fingerprint(pbf),'base':fingerprint(base),'filters':FILTERS}
    manifest_path=work/'areas.manifest.json';db=work/'areas.sqlite'
    previous=json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
    if previous.get('inputs')==manifest and db.exists():
        area_stats=previous['area_stats'];log('Reusing complete polygon index.')
    else:
        filtered=a.filtered.resolve() if a.filtered else work/'areas.osm.pbf'
        if not a.filtered:
            subprocess.run(['osmium','tags-filter',str(pbf),*FILTERS,'-o',str(filtered),'--overwrite','--progress'],check=True)
        config=work/'export-areas.json'
        config.write_text(json.dumps({'attributes':{'type':True,'id':True},'linear_tags':False,
                                     'area_tags':True,'include_tags':['natural','leisure','landuse','waterway']}))
        node_index=work/'area-node-index.dat'
        if node_index.exists():node_index.unlink()
        command=['osmium','export',str(filtered),'-f','geojsonseq','--geometry-types=polygon',
                 '-c',str(config),'-i',f'sparse_file_array,{node_index}','-x','print_record_separator=false']
        log('Exporting and clipping complete OSM water/green areas...')
        suppressed=relation_outer_members(filtered)
        temporary=work/'areas.new.sqlite'
        if temporary.exists():temporary.unlink()
        proc=subprocess.Popen(command,stdout=subprocess.PIPE,text=True,encoding='utf-8')
        try:area_stats=build_index(proc.stdout,temporary,suppressed)
        except BaseException:
            proc.terminate();proc.wait();raise
        finally:proc.stdout.close()
        if proc.wait()!=0:raise RuntimeError('osmium polygon export failed')
        os.replace(temporary,db)
        manifest_path.write_text(json.dumps({'inputs':manifest,'area_stats':area_stats},indent=2)+'\n')
        if node_index.exists():node_index.unlink()
    log('Merging polygon tiles into existing map data...')
    merge_stats=merge_map(base,db,out,area_stats['bounds_e5'])
    if fingerprint(base)!=manifest['base'] or fingerprint(pbf)!=manifest['source']:
        raise RuntimeError('an input changed during the rebuild')
    report={'inputs':manifest,'area_stats':area_stats,'merge_stats':merge_stats,'sha256':sha(out),
            'runtime':'requires clio040 with CPM5 style-bit-7 area support',
            'preserved':'road geometry, road metadata, labels and route sidecars',
            'format':'CPM5 v4, header flag 2, closed area contours with style bit 128'}
    out.with_suffix('.cpm.json').write_text(json.dumps(report,indent=2)+'\n')
    log(json.dumps(report,indent=2))


if __name__=='__main__':main()
