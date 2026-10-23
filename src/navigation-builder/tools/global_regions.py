"""Offline country lookup and CPG1 region sidecar (Natural Earth, public domain).

This is generalized cartography, not a legal border survey. Auto units can be
manually overridden. Camera exports exclude restricted and unresolved territories
and a conservative exclusion buffer around explicitly restricted countries.
"""
import gzip,hashlib,json,struct
from pathlib import Path
from shapely.geometry import shape,Point,box
from shapely.prepared import prep
DATA=Path(__file__).resolve().parents[1]/'data/countries.geojson'
HDR=struct.Struct('<4sHHII')
REC=struct.Struct('<2sBBiiiiIII') # ISO, flags, reserved, bbox E5, ring offset, ring count
# Road units. Native mph tags also inform Auto on a matched road.
PROFILES=json.loads(DATA.with_name('country-profiles.json').read_text())['countries']
MPH={iso for iso,p in PROFILES.items() if p['speed_unit']=='mph'}
LEFT={iso for iso,p in PROFILES.items() if p['driving_side']=='left'}
POLICY=json.loads(DATA.with_name('camera-policy.json').read_text())
RESTRICTED=set(POLICY['blocked_exact_locations'])
CAMERA_ALLOWED=set(POLICY['allowed_exact_locations'])
if CAMERA_ALLOWED & RESTRICTED:raise ValueError('Camera country cannot be both enabled and blocked')
if set(POLICY['policy_states'])!=set(PROFILES):raise ValueError('Every supported territory needs an explicit camera policy state')
if {iso for iso,state in POLICY['policy_states'].items() if state=='reviewed-information-only'}!=CAMERA_ALLOWED:raise ValueError('Camera allowlist/state mismatch')
if {iso for iso,state in POLICY['policy_states'].items() if state=='blocked'}!=RESTRICTED:raise ValueError('Camera blocklist/state mismatch')
if set(POLICY['policy_states'].values())-{'reviewed-information-only','blocked','disabled-pending-review'}:raise ValueError('Unknown camera policy state')
class Regions:
 def __init__(self):
  self.rows=[]
  for f in json.loads(DATA.read_text())['features']:
   p=f['properties'];iso=p.get('ISO_A2_EH') or p.get('ISO_A2');g=shape(f['geometry'])
   if not iso or iso=='-99':continue
   self.rows.append((iso,p['ADMIN'],g,prep(g)))
  refinements=DATA.with_name('country-refinements.geojson.gz')
  if refinements.exists():
   extra=[];replaced=set()
   for f in json.loads(gzip.decompress(refinements.read_bytes()))['features']:
    iso=f['properties']['iso'];g=shape(f['geometry']);extra.append((iso,f['properties']['name'],g,prep(g)));replaced.add(iso)
   self.rows=extra+[row for row in self.rows if row[0] not in replaced]
  self.blocked=[prep(g.buffer(POLICY["border_buffer_degrees"])) for iso,n,g,p in self.rows if iso in RESTRICTED]
 def country(self,lat,lon):
  point=Point(lon,lat)
  for iso,name,g,p in self.rows:
   a,b,c,d=g.bounds
   if a<=lon<=c and b<=lat<=d and p.covers(point):return iso,name
  return '', 'Unresolved / offshore'
 def camera_allowed(self,lat,lon):
  iso,_=self.country(lat,lon)
  return iso in CAMERA_ALLOWED and not any(g.covers(Point(lon,lat)) for g in self.blocked)
 def intersecting(self,bounds):
  west,south,east,north=bounds
  clip=box(west,south,east,north)
  return [(iso,name) for iso,name,g,p in self.rows if g.intersects(clip) and g.intersection(clip).area>1e-10]
 def write(self,out,bounds=None):
  # Include an extra .25-degree margin so unit/policy resolution survives a
  # short trip past the map edge; clip avoids loading a huge world polygon.
  clip=box(*(bounds or (-180,-90,180,90))).buffer(.25)
  records=[];data=bytearray()
  for iso,name,geom,prepared in self.rows:
   g=geom.intersection(clip)
   parts=[g] if g.geom_type=='Polygon' else list(g.geoms) if g.geom_type in ('MultiPolygon','GeometryCollection') else []
   for poly in parts:
    if poly.geom_type!='Polygon' or poly.is_empty:continue
    # Retain dataset geometry: no extra border simplification in the pack.
    rings=[poly.exterior,*poly.interiors];offset=len(data)
    for ring in rings:
     pts=[(round(y*1e5),round(x*1e5)) for x,y,*_ in ring.coords]
     data.extend(struct.pack('<I',len(pts)))
     for point in pts:data.extend(struct.pack('<ii',*point))
    minx,miny,maxx,maxy=poly.bounds
    flags=int(iso in MPH)|(2 if iso in CAMERA_ALLOWED else 0)|(4 if iso in LEFT else 0)
    records.append((iso.encode(),flags,0,round(miny*1e5),round(minx*1e5),round(maxy*1e5),round(maxx*1e5),offset,len(rings),0))
  offset=HDR.size+REC.size*len(records)
  out=Path(out)
  with out.open('wb') as f:
   f.write(HDR.pack(b'CPG1',1,REC.size,len(records),offset))
   for row in records:f.write(REC.pack(*row))
   f.write(data)
  return {'format':'CPG1','polygons':len(records),'boundary_source_sha256':hashlib.sha256(DATA.read_bytes()).hexdigest(),'refinements':json.loads(DATA.with_name('country-refinements.json').read_text())['bundled_sha256']}
