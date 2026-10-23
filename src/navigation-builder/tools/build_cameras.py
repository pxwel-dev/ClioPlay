#!/usr/bin/env python3
"""Build CPC1 from OSM nodes/enforcement relations and optional UTF-8 CSV imports.

CSV: id,latitude,longitude,type,maxspeed,heading,end_latitude,end_longitude
Types: fixed,red_light,average. Heading is affected TRAFFIC direction, degrees
clockwise from north (blank=unknown). maxspeed follows OSM km/h or '30 mph'.
Average records require entry/exit coordinates. Imports are data, never code.
"""
import argparse,csv,hashlib,json,math,struct
from pathlib import Path
import osmium
from global_regions import Regions
from global_speeds import scalar
HDR=struct.Struct('<4sHHII');REC=struct.Struct('<iiiiIHHBB2sIQ')
KINDS={'fixed':1,'red_light':2,'average':3}
COMPASS={'N':0,'NE':45,'E':90,'SE':135,'S':180,'SW':225,'W':270,'NW':315}
def bearing(a,b):
 dy=b[0]-a[0];dx=(b[1]-a[1])*math.cos(math.radians((a[0]+b[0])/2))
 return round(math.degrees(math.atan2(dx,dy)))%360 if dx or dy else 65535

def heading(raw):
 text=str(raw or '').strip().upper()
 if not text:return 65535
 if text in COMPASS:return COMPASS[text]
 try:
  n=float(text)
  if not math.isfinite(n) or not 0<=n<360:raise ValueError()
  return round(n)%360
 except ValueError:raise ValueError('invalid traffic heading: '+text)

class Collect(osmium.SimpleHandler):
 def __init__(self):
  super().__init__();self.cameras={};self.relations=[];self.wanted=set();self.neighbours={};self.skipped=0
 def node(self,n):
  if n.tags.get('highway')=='speed_camera':
   self.cameras[n.id]=dict(n.tags);self.wanted.add(n.id)
 def way(self,w):
  # Nodes precede ways in a normal OSM extract; retain only camera neighbours.
  ids=[n.ref for n in w.nodes]
  for i,n in enumerate(ids):
   if n in self.cameras:
    a=ids[max(0,i-1)];b=ids[min(len(ids)-1,i+1)]
    self.neighbours[n]=(a,b,w.id);self.wanted.update((a,b))
 def relation(self,r):
  if r.tags.get('type')!='enforcement' or r.tags.get('enforcement') not in ('maxspeed','traffic_signals','average_speed'):return
  roles={k:[m.ref for m in r.members if m.type=='n' and m.role==k] for k in ('device','from','to')}
  if len(roles['from'])>1 or len(roles['to'])>1:self.skipped+=1;return
  roles['section']=[m.ref for m in r.members if m.type=='w' and m.role=='section']
  self.wanted.update(n for k,v in roles.items() if k!='section' for n in v);self.relations.append((r.id,dict(r.tags),roles))
class Locations(osmium.SimpleHandler):
 def __init__(self,wanted):super().__init__();self.wanted=wanted;self.points={}
 def node(self,n):
  if n.id in self.wanted and n.location.valid():self.points[n.id]=(n.location.lat,n.location.lon)

class Sections(osmium.SimpleHandler):
 def __init__(self,wanted,points):super().__init__();self.wanted=wanted;self.points=points;self.ways={};self.roads={}
 def way(self,w):
  ids=[n.ref for n in w.nodes]
  if w.id in self.wanted:self.ways[w.id]=(ids,dict(w.tags))
  if w.tags.get('highway'):
   for n in ids:
    if n in self.points:self.roads.setdefault(n,set()).add(w.id)

def section_chain(way_ids,ways):
 # Recover a single continuous chain irrespective of relation member ordering.
 # Branches, closed loops, missing ways and conflicting one-way directions fail.
 from routing_policy import direction
 if not way_ids or any(w not in ways for w in way_ids):return None
 adj={}
 for w in way_ids:
  ids,p=ways[w]
  if len(ids)<2:return None
  adj.setdefault(ids[0],[]).append(w);adj.setdefault(ids[-1],[]).append(w)
 ends=[n for n,v in adj.items() if len(v)==1]
 if len(ends)!=2 or any(len(v)>2 for v in adj.values()):return None
 at=min(ends);unused=set(way_ids);chain=[];legal={1,-1};first_way=last_way=0
 while unused:
  options=[w for w in adj.get(at,[]) if w in unused]
  if len(options)!=1:return None
  w=options[0];unused.remove(w);ids,p=ways[w];forward=ids[0]==at
  if not forward:ids=ids[::-1]
  d=direction(p)
  if d is None:return None
  if d:legal.intersection_update({d if forward else -d})
  if not first_way:first_way=w
  last_way=w;chain.extend(ids if not chain else ids[1:]);at=ids[-1]
 if not legal:return None
 return chain,legal,first_way,last_way

def extract(pbf):
 c=Collect();c.apply_file(str(pbf))
 wanted={w for _,_,r in c.relations for w in r['section']}
 sections=Sections(wanted,c.wanted);sections.apply_file(str(pbf))
 for ids,p in sections.ways.values():c.wanted.update(ids)
 loc=Locations(c.wanted);loc.apply_file(str(pbf));pts=loc.points;out=[];used=set();skipped=c.skipped
 def road(n):
  ids=sections.roads.get(n,set());return next(iter(ids)) if len(ids)==1 else 0
 for rid,p,roles in c.relations:
  def point(role):return pts.get(roles[role][0]) if roles[role] else None
  start=point('from');end=point('to');device=point('device');kind={'maxspeed':'fixed','traffic_signals':'red_light','average_speed':'average'}[p['enforcement']]
  wid=road(roles['from'][0]) if roles['from'] else 0;h=65535
  if kind=='average':
   chain=section_chain(roles['section'],sections.ways) if roles['section'] else None
   if chain:
    ids,legal,first,last=chain
    if roles['from'] and roles['from'][0] in (ids[0],ids[-1]):d=1 if roles['from'][0]==ids[0] else -1
    elif roles['to'] and roles['to'][0] in (ids[0],ids[-1]):d=-1 if roles['to'][0]==ids[0] else 1
    elif len(legal)==1:d=next(iter(legal))
    else:skipped+=1;continue # undirected two-way section: do not invent coverage
    if d not in legal:skipped+=1;continue
    if d<0:ids=ids[::-1]
    if not start:start=pts.get(ids[0])
    if not end:end=pts.get(ids[-1])
    wid=first if d>0 else last
    if ids[0] in pts and ids[1] in pts:h=bearing(pts[ids[0]],pts[ids[1]])
   if not start or not end:skipped+=1;continue
   pos=start
  else:
   if len(roles['device'])>1:skipped+=1;continue
   pos=device or end
   if not wid and roles['device']:wid=road(roles['device'][0])
  if not pos:skipped+=1;continue
  if h==65535:h=bearing(start,end or device) if start and (end or device) else 65535
  out.append(dict(id='osm:r'+str(rid),latitude=pos[0],longitude=pos[1],type=kind,maxspeed=p.get('maxspeed',''),heading='' if h==65535 else h,end_latitude=end[0] if kind=='average' else '',end_longitude=end[1] if kind=='average' else '',osm_way_id=wid))
  used.update(roles['device'])
 for nid,p in c.cameras.items():
  if nid in used or nid not in pts:continue
  h=65535;d=p.get('direction','');wid=road(nid)
  if nid in c.neighbours:
   a,b,w=c.neighbours[nid]
   if not wid and len(sections.roads.get(nid,set()))==1:wid=w
   if d in ('forward','backward') and a in pts and b in pts:
    h=bearing(pts[a],pts[b]);h=(h+180)%360 if d=='backward' and h!=65535 else h
  if d and d not in ('both','forward','backward'):
   try:h=(heading(d)+180)%360 # camera facing is opposite incoming traffic
   except ValueError:pass
  out.append(dict(id='osm:n'+str(nid),latitude=pts[nid][0],longitude=pts[nid][1],type='fixed',maxspeed=p.get('maxspeed',''),heading='' if h==65535 else h,end_latitude='',end_longitude='',osm_way_id=wid))
 return out,skipped

def coordinate(row,key,minimum,maximum):
 value=float(row[key])
 if not math.isfinite(value) or not minimum<=value<=maximum:raise ValueError('invalid '+key)
 return value

def build(pbf,imports,out,regions=None):
 regions=regions or Regions();rows,skipped=extract(pbf) if pbf else ([],0);source_count=len(rows);import_count=0
 for path in imports:
  with Path(path).open(encoding='utf-8-sig',newline='') as f:
   for row in csv.DictReader(f):rows.append(row);import_count+=1
 packed={};excluded=0
 for row in rows:
  lat=coordinate(row,'latitude',-90,90);lon=coordinate(row,'longitude',-180,180);kind=KINDS.get(row.get('type','fixed'))
  if not kind:raise ValueError('unsupported camera type: '+str(row.get('type')))
  endlat=endlo=0
  if kind==3:endlat=coordinate(row,'end_latitude',-90,90);endlo=coordinate(row,'end_longitude',-180,180)
  # Filter BOTH entries and exits; no French exact-location database is installed.
  if not regions.camera_allowed(lat,lon) or (kind==3 and not regions.camera_allowed(endlat,endlo)):
   excluded+=1;continue
  speed,flags=scalar(row.get('maxspeed',''));h=heading(row.get('heading',''));iso,_=regions.country(lat,lon)
  if speed and flags&12:speed=0
  ident=int.from_bytes(hashlib.sha256(str(row.get('id') or json.dumps(row,sort_keys=True)).encode()).digest()[:4],'little')
  a,b,el,eo=[round(x*1e5) for x in (lat,lon,endlat,endlo)]
  key=(a,b,kind,h,el,eo)
  wid=int(row.get('osm_way_id') or 0)
  if not 0<=wid<1<<64:raise ValueError('invalid osm_way_id')
  record=(a,b,el,eo,ident,h,speed,flags,kind,iso.encode(),0,wid)
  if key not in packed or speed:packed[key]=record # enrich co-located OSM/import points deterministically
 records=sorted(packed.values())
 with Path(out).open('wb') as f:
  f.write(HDR.pack(b'CPC1',2,REC.size,len(records),HDR.size))
  for record in records:f.write(REC.pack(*record))
 report={'format':'CPC1-v2','matched_road_records':sum(bool(r[-1]) for r in records),'osm_candidates':source_count,'import_candidates':import_count,'records':len(records),'policy_excluded':excluded,'enabled_policy_countries':sorted(__import__('global_regions').CAMERA_ALLOWED),'unsupported_or_incomplete_relations':skipped,'coverage':'OSM/import data only; no live or guaranteed comprehensive database'}
 Path(str(out)+'.json').write_text(json.dumps(report,indent=2)+'\n');return report

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--pbf',type=Path);p.add_argument('--import-csv',type=Path,action='append',default=[]);p.add_argument('--out',type=Path,required=True);a=p.parse_args();print(json.dumps(build(a.pbf,a.import_csv,a.out),indent=2))
if __name__=='__main__':main()
