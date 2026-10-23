#!/usr/bin/env python3
"""CPT1 offline timezone polygons + IANA UTC transitions (2020..2060).

The device selects the polygon and binary-searches transitions using GPS UTC.
No host timezone database or internet access is needed in the car.
"""
from pathlib import Path
import argparse,gzip,json,struct,hashlib
from datetime import datetime,timedelta,timezone
from importlib.resources import files
from zoneinfo import ZoneInfo
from shapely.geometry import shape,box
HDR=struct.Struct('<4sHH8I');POLY=struct.Struct('<iiiiIIII');ZONE=struct.Struct('<48sIIii');TRANS=struct.Struct('<Ii')
DATA=Path(__file__).resolve().parents[1]/'data/timezones.geojson.gz'
START=int(datetime(2020,1,1,tzinfo=timezone.utc).timestamp());END=int(datetime(2061,1,1,tzinfo=timezone.utc).timestamp())
def zone(name):
 with files('tzdata.zoneinfo').joinpath(*name.split('/')).open('rb') as f:return ZoneInfo.from_file(f,key=name)
def transitions(name):
 z=zone(name)
 def offset(stamp):return int(datetime.fromtimestamp(stamp,timezone.utc).astimezone(z).utcoffset().total_seconds())
 prior=offset(START);initial=prior;out=[];at=START
 while at<END:
  nxt=min(at+21600,END);current=offset(nxt)
  if current!=prior:
   lo,hi=at,nxt
   while hi-lo>1:
    mid=(lo+hi)//2
    if offset(mid)==prior:lo=mid
    else:hi=mid
   out.append((hi,current));prior=current
  at=nxt
 return initial,out

def build(out,bounds=None):
 clip=box(*(bounds or (-180,-85,180,85))).buffer(.25);polys=[];geometry=bytearray();names=[];ids={}
 for f in json.loads(gzip.decompress(DATA.read_bytes()))['features']:
  g=shape(f['geometry'])
  if not g.intersects(clip):continue
  g=g.intersection(clip);parts=[g] if g.geom_type=='Polygon' else list(g.geoms) if hasattr(g,'geoms') else []
  name=f['properties']['tzid']
  if name not in ids:ids[name]=len(names);names.append(name)
  for p in parts:
   if p.geom_type!='Polygon' or p.is_empty:continue
   rings=[p.exterior,*p.interiors];off=len(geometry)
   for ring in rings:
    points=[(round(y*1e5),round(x*1e5)) for x,y,*_ in ring.coords];geometry.extend(struct.pack('<I',len(points)))
    for point in points:geometry.extend(struct.pack('<ii',*point))
   w,s,e,n=p.bounds;polys.append((round(s*1e5),round(w*1e5),round(n*1e5),round(e*1e5),off,len(rings),ids[name],0))
 records=[];events=[]
 for name in names:
  initial,ts=transitions(name);records.append((name.encode(),len(events),len(ts),initial,0));events.extend(ts)
 po=HDR.size;zo=po+len(polys)*POLY.size;go=zo+len(records)*ZONE.size;to=go+len(geometry)
 out=Path(out);out.parent.mkdir(parents=True,exist_ok=True)
 with out.open('wb') as f:
  f.write(HDR.pack(b'CPT1',1,POLY.size,len(polys),len(names),po,zo,go,to,START,END))
  for row in polys:f.write(POLY.pack(*row))
  for row in records:f.write(ZONE.pack(*row))
  f.write(geometry)
  for row in events:f.write(TRANS.pack(*row))
 report={'format':'CPT1','polygons':len(polys),'zones':names,'transitions':len(events),'valid_from_utc':START,'valid_until_utc':END,'source_sha256':hashlib.sha256(DATA.read_bytes()).hexdigest(),'tzdata':'2026.5'}
 out.with_suffix(out.suffix+'.json').write_text(json.dumps(report,indent=2)+'\n');return report
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--bounds',type=float,nargs=4);a=p.parse_args();print(json.dumps(build(a.output,a.bounds),indent=2))
