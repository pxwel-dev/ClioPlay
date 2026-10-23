"""CPM5 native speed evidence; runtime fills eligible urban gaps with ? estimates.

Flags: 1 context estimate; 2 mph (otherwise km/h); 4 unrestricted;
8 conditional/variable, requiring information unavailable to this device;
16 suppress automatic estimates (unsupported evidence, special road or wrong way).
Directions are relative to the stored OSM way geometry.
"""
import json, math, re
from pathlib import Path
UNKNOWN=(0,0)
BLOCKED=(0,16)
VARIABLE=(0,8)
# Explicit OSM context tags, not automatic defaults. Unknown namespaces stay unknown.
CONTEXT={
 'FR:urban':(50,1),'FR:rural':(80,1),'FR:trunk':(110,1),'FR:motorway':(130,1),
 'ES:rural':(90,1),'ES:motorway':(120,1),
 'GB:nsl_restricted':(30,3),'GB-WLS:nsl_restricted':(20,3),
 'GB:nsl_single':(60,3),'GB:nsl_dual':(70,3),'GB:motorway':(70,3),
 'DE:urban':(50,1),'DE:rural':(100,1),'DE:motorway':(0,4),
}

# Broader traceable contexts; the reviewed values above override upstream snapshots.
_REVIEWED=CONTEXT.copy()
for _key,_value in json.loads((Path(__file__).resolve().parents[1]/'data/speed-contexts.json').read_text())['contexts'].items():
 if _value=='none':CONTEXT.setdefault(_key,(0,4))
 else:
  _parts=_value.split();_v=int(_parts[0])
  if 0<_v<=255:CONTEXT.setdefault(_key,(_v,3 if len(_parts)>1 else 1))
# Spanish urban defaults depend on street configuration (20/30/50), not one value.
CONTEXT.pop('ES:urban',None)
CONTEXT.update(_REVIEWED)

def scalar(raw):
 text=str(raw or '').strip()
 if not text:return UNKNOWN
 if text.lower() in ('signals','variable'):return VARIABLE
 if text.lower()=='none':return (0,4)
 if text in CONTEXT:return CONTEXT[text]
 if text.startswith('UK:'):return scalar('GB:'+text[3:])
 m=re.fullmatch(r'(\d+(?:\.\d+)?)\s*(mph|mi/h|km/h|kmh|kph)?',text,re.I)
 if not m:return UNKNOWN
 value=float(m[1]); unit=(m[2] or 'km/h').lower()
 # Limits are whole integers on device; never silently round unusual signs.
 if not value.is_integer() or not 1<=value<=255:return UNKNOWN
 return (int(value),2 if unit in ('mph','mi/h') else 0)

def equal_lanes(text):
 parts=[scalar(x) for x in str(text).split('|')]
 return parts[0] if parts and all(x==parts[0] and x!=UNKNOWN for x in parts) else UNKNOWN

def directions(p):
 base=scalar(p.get('maxspeed'))
 if not p.get('maxspeed') or p.get('maxspeed') in ('implicit','national'):
  for key in ('maxspeed:type','source:maxspeed','zone:maxspeed'):
   if p.get(key):
    v=scalar(p[key])
    if v!=UNKNOWN:base=v;break
 if p.get('maxspeed:lanes'):base=equal_lanes(p['maxspeed:lanes'])
 out=[]
 for side in ('forward','backward'):
  v=scalar(p['maxspeed:'+side]) if 'maxspeed:'+side in p else base
  if 'maxspeed:lanes:'+side in p:v=equal_lanes(p['maxspeed:lanes:'+side])
  # We cannot evaluate weather, variable signs, vehicle or local-time schedules.
  if any(p.get(k) for k in ('maxspeed:conditional','maxspeed:'+side+':conditional','maxspeed:lanes:conditional','maxspeed:lanes:'+side+':conditional')) or str(p.get('maxspeed:variable') or '').lower() not in ('','no','false','0'):
   v=VARIABLE
  if v==UNKNOWN and (any(str(p.get(k) or '').strip() for k in p if k.startswith('maxspeed') or k in ('source:maxspeed','zone:maxspeed')) or p.get('highway') in ('living_street','service') or str(p.get('highway') or '').endswith('_link')):v=BLOCKED
  out.append(v)
 ow=str(p.get('oneway') or '').lower()
 if ow in ('yes','1','true'):out[1]=BLOCKED
 if ow=='-1':out[0]=BLOCKED
 return tuple(out)

def global_speed_detail(p):
 f,b=directions(p)
 return f[0],json.dumps([f,b],separators=(',',':'))

def decode_source(source):
 try:
  f,b=json.loads(source)
  return tuple(f),tuple(b)
 except (ValueError,TypeError):return UNKNOWN,UNKNOWN

def global_flags(source,weak=False):return decode_source(source)
def global_meta(name,ref,speed,meta,txt):
 f,b=meta
 return bytes((*f,*b,len(txt)))+txt

def mph_value(v):return v[0] if v[1]&2 else v[0]/1.609344

def global_speed_scan(lines,db,corrections=None):
 # Use the common disk-backed way schema; deliberately skip UK sign/density/graph inference.
 from build_map import ROAD,iter_lines,polyline_length_m
 columns=[x[1] for x in db.execute('pragma table_info(speed_way)')]
 batch=[];seq=0;counts={}
 for line in lines:
  line=line.strip('\x1e\r\n ')
  if not line:continue
  obj=json.loads(line);p=obj.get('properties') or {};h=p.get('highway')
  if h not in ROAD:continue
  coords=next(iter_lines(obj),None)
  if not coords or len(coords)<2:continue
  if '@id' not in p:raise ValueError('missing OSM way id')
  if corrections is not None:p=corrections.apply(p)
  f,b=directions(p);seq+=1;source=json.dumps([f,b],separators=(',',':'))
  row={c:0 for c in columns}
  for c in ('ref_key','name_key','hclass','junction','lit','jurisdiction','source','reason'):row[c]=''
  row.update(seq=seq,way_id=int(p['@id']),speed=f[0],source=source,weak=int(bool(f[1]&1)),hclass=h,length_m=polyline_length_m(coords),reason='not-representable-or-unmapped' if not f[0] else '')
  batch.append(tuple(row[c] for c in columns));counts[source]=counts.get(source,0)+1
  if len(batch)>=10000:
   db.executemany('insert into speed_way values('+','.join('?' for _ in columns)+')',batch);batch=[]
 if batch:db.executemany('insert into speed_way values('+','.join('?' for _ in columns)+')',batch)
 db.commit()
 return {'road_ways':seq,'directional_records':counts,'policy':'explicit OSM values and reviewed context tags only; no inferred national limits'}

def global_self_test():
 assert directions({'maxspeed':'130'})==((130,0),(130,0))
 assert directions({'maxspeed':'50 mph'})==((50,2),(50,2))
 assert directions({'highway':'motorway'})==(UNKNOWN,UNKNOWN)
 assert directions({'maxspeed':'FR:urban'})==((50,1),(50,1))
 assert directions({'maxspeed':'ES:urban'})==(BLOCKED,BLOCKED)
 assert directions({'maxspeed':'50','maxspeed:conditional':'30 @ (wet)'})==(VARIABLE,VARIABLE)
 assert directions({'maxspeed:forward':'80','maxspeed:backward':'60'})==((80,0),(60,0))
 assert directions({'maxspeed':'none'})==((0,4),(0,4))
