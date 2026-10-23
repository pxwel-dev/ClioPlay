#!/usr/bin/env python3
"""Add sea/estuary polygons automatically; cache the upstream OSM coastline dataset."""
import argparse
import fcntl
import json
import itertools
import os
from pathlib import Path
import shutil
import sqlite3
import struct
import time
import urllib.request
import zipfile

import shapefile
from shapely import make_valid
from shapely.affinity import scale
from shapely.geometry import box, shape

from rebuild_areas import HDR, IDX, MAX_TILE_BYTES, area_blob, fingerprint, log, polygons, sha, tile_polygon

URL = 'https://osmdata.openstreetmap.de/download/water-polygons-split-4326.zip'


def coast_source(cache):
    cache.mkdir(parents=True, exist_ok=True)
    with (cache/'coastline.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX)
        return _coast_source(cache)


def _coast_source(cache):
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache/'water-polygons-split-4326.zip'
    if not archive.exists():
        temporary = archive.with_suffix('.download')
        log('Downloading OSM coastal water polygons (about 865 MB; cached for later builds)...')
        request = urllib.request.Request(URL, headers={'User-Agent': 'CLIOPLAY-navigation-builder/1.0'})
        try:
            with urllib.request.urlopen(request, timeout=120) as source, temporary.open('wb') as out:
                total = 0; last = time.monotonic()
                for chunk in iter(lambda: source.read(4*1024*1024), b''):
                    out.write(chunk); total += len(chunk)
                    if time.monotonic()-last > 10:
                        log(f'  downloaded {total/1024**2:.0f} MiB'); last = time.monotonic()
            with zipfile.ZipFile(temporary) as check:
                if not any(n.endswith('.shp') for n in check.namelist()):
                    raise ValueError('coastline archive has no shapefile')
            os.replace(temporary, archive)
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise
    # Extract only the geometry/index, not attributes or arbitrary archive paths.
    manifest = cache/'extracted.json'
    identity = fingerprint(archive)
    paths = {ext: cache/('water'+ext) for ext in ('.shp', '.shx')}
    if not manifest.exists() or json.loads(manifest.read_text()) != identity or not all(p.exists() for p in paths.values()):
        log('Extracting cached coastline geometry...')
        with zipfile.ZipFile(archive) as data:
            for ext, path in paths.items():
                matches = [n for n in data.namelist() if n.endswith(ext)]
                if len(matches) != 1: raise ValueError(f'expected exactly one {ext} in coastline archive')
                temporary = path.with_suffix(ext+'.new')
                with data.open(matches[0]) as source, temporary.open('wb') as out:
                    shutil.copyfileobj(source, out, 4*1024*1024)
                os.replace(temporary, path)
        manifest.write_text(json.dumps(identity)+'\n')
    return archive, paths


def build_index(paths, database, bounds):
    # Bounds are [minlat,minlon,maxlat,maxlon] in E5; SHP coordinates are lon/lat.
    south, west, north, east = bounds
    bbox = (west/1e5, south/1e5, east/1e5, north/1e5)
    envelope = box(*bbox)
    stats = {'polygons': 0, 'records': 0, 'holes': 0, 'bounds_e5': bounds}
    with sqlite3.connect(database) as db:
        db.execute('pragma journal_mode=off'); db.execute('pragma synchronous=off')
        db.execute('create table areas(z int,tx int,ty int,layer int,style int,blob blob)')
        batch = []; last = time.monotonic()
        with shapefile.Reader(shp=str(paths['.shp']), shx=str(paths['.shx'])) as data:
            for feature in data.iterShapes(bbox=bbox):
                geometry = shape(feature.__geo_interface__)
                if not geometry.is_valid: geometry = make_valid(geometry)
                geometry = scale(geometry.intersection(envelope), xfact=1e5, yfact=1e5, origin=(0,0))
                for poly in polygons(geometry):
                    stats['polygons'] += 1; stats['holes'] += len(poly.interiors)
                    for z, tx, ty, points in tile_polygon(poly, 0, maxlevel=3, fine=True):
                        batch.append((z,tx,ty,1,5,area_blob(1,5,points))); stats['records'] += 1
                        if len(batch) >= 10000:
                            db.executemany('insert into areas values(?,?,?,?,?,?)', batch); batch.clear()
                if time.monotonic()-last > 10:
                    log(f'  coastal polygons={stats["polygons"]:,} records={stats["records"]:,}')
                    last = time.monotonic()
        if batch: db.executemany('insert into areas values(?,?,?,?,?,?)', batch)
        db.execute('create index tilekey on areas(z,tx,ty,layer desc)')
    return stats


def pack_ocean(base,database,output,work):
    """Append separately paged ocean tiles; retain the entire original tile stream."""
    with base.open('rb') as source:
        h=HDR.unpack(source.read(HDR.size))
    if h[:2]!=(b'CPM5',5) or h[4]&5: raise ValueError('production CPM5 without ocean required')
    base_data=base.stat().st_size-h[7]
    index_path=work/'ocean.index.tmp';data_path=work/'ocean.data.tmp'
    stats={'tiles':h[5],'ocean_pages':0,'kept_area_records':0,'dropped_area_records':0,'max_tile_bytes':0}
    offset=base_data
    with sqlite3.connect(database) as db,index_path.open('wb') as index,data_path.open('wb') as data:
        rows=db.execute('select z,tx,ty,blob from areas order by z,tx,ty')
        for (z,tx,ty),group in itertools.groupby(rows,key=lambda r:r[:3]):
            page=0;blobs=[];used=8
            def emit():
                nonlocal offset,page
                if page>255: raise ValueError('ocean tile exceeds 256 pages')
                tile=struct.pack('<4H',len(blobs),0,0,0)+b''.join(blobs)
                index.write(IDX.pack(8+z,page,tx,ty,offset,len(tile),len(blobs),0));data.write(tile)
                offset+=len(tile);page+=1;stats['ocean_pages']+=1
                stats['kept_area_records']+=len(blobs);stats['max_tile_bytes']=max(stats['max_tile_bytes'],len(tile))
            for _,_,_,blob in group:
                if len(blob)+8>MAX_TILE_BYTES:raise ValueError('oversized ocean contour')
                if used+len(blob)>MAX_TILE_BYTES:emit();blobs=[];used=8
                blobs.append(blob);used+=len(blob)
            if blobs:emit()
    stats['tiles']+=stats['ocean_pages'];data_off=HDR.size+stats['tiles']*IDX.size
    if data_off+offset>0x7fffffff:raise ValueError('map exceeds the runtime signed 2 GiB seek limit')
    temporary=output.with_suffix('.new')
    with base.open('rb') as source,temporary.open('wb') as out:
        out.write(HDR.pack(b'CPM5',5,h[2],h[3],h[4]|6,stats['tiles'],HDR.size,data_off,*h[8:12]))
        source.seek(h[6]);remaining=h[5]*IDX.size
        while remaining:
            chunk=source.read(min(4*1024*1024,remaining))
            if not chunk:raise ValueError('truncated original index')
            out.write(chunk);remaining-=len(chunk)
        with index_path.open('rb') as index:shutil.copyfileobj(index,out,4*1024*1024)
        source.seek(h[7]);shutil.copyfileobj(source,out,4*1024*1024)
        with data_path.open('rb') as data:shutil.copyfileobj(data,out,4*1024*1024)
        out.flush();os.fsync(out.fileno())
    os.replace(temporary,output);index_path.unlink();data_path.unlink()
    stats['bytes']=output.stat().st_size
    return stats


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--cache', type=Path, required=True)
    args = parser.parse_args()
    base, out, work, cache = [p.expanduser().resolve() for p in (args.base,args.out,args.work,args.cache)]
    if base == out: raise SystemExit('output must differ from base map')
    work.mkdir(parents=True, exist_ok=True); out.parent.mkdir(parents=True, exist_ok=True)
    with base.open('rb') as source:
        header = HDR.unpack(source.read(HDR.size))
    if header[4]&4: raise SystemExit('map already contains shared ocean polygons')
    archive, paths = coast_source(cache)
    identity = {'builder': 'shared-ocean-v2', 'base': fingerprint(base),
                'coastline': fingerprint(archive), 'bounds_e5': list(header[8:12]), 'url': URL}
    manifest = work/'ocean.manifest.json'; database = work/'ocean.sqlite'
    previous = json.loads(manifest.read_text()) if manifest.exists() else {}
    if previous.get('inputs') == identity and database.exists():
        stats = previous['area_stats']; log('Reusing coastal polygon index.')
    else:
        temporary = database.with_suffix('.new.sqlite'); temporary.unlink(missing_ok=True)
        stats = build_index(paths, temporary, identity['bounds_e5'])
        os.replace(temporary, database)
        manifest.write_text(json.dumps({'inputs':identity,'area_stats':stats},indent=2)+'\n')
    log('Packing shared sea and estuary tiles...')
    merged = pack_ocean(base, database, out, work)
    if fingerprint(base) != identity['base'] or fingerprint(archive) != identity['coastline']:
        raise RuntimeError('input changed during coastal build')
    if merged['dropped_area_records']:
        raise RuntimeError('coastal geometry exceeds tile capacity; output is incomplete')
    report = {'format':'CPM5 v4; flags 2+4; ocean style 128+5; reserved index levels 8..11 alias Z0..Z3; index flags = page',
              'inputs':identity,'area_stats':stats,'merge_stats':merged,'sha256':sha(out),
              'coastline_sha256':sha(archive),
              'runtime':'requires shared-ocean clio040; roads draw after all fills'}
    out.with_suffix('.cpm.json').write_text(json.dumps(report,indent=2)+'\n')
    log(json.dumps(report,indent=2))


if __name__ == '__main__': main()
