"""Ordinary public motorcar access, including hierarchy and direction.

Most-specific mode wins: motorcar > motor_vehicle > vehicle > access. Explicit
mode-specific one-way exceptions override generic one-way/roundabout defaults.
Unresolved conditional access is excluded from the all-times graph, rather than
publishing a road which is only sometimes legal. Keep this distinction in reports.
Source: https://wiki.openstreetmap.org/wiki/Key:access
"""
MODES=('motorcar','motor_vehicle','vehicle','access')
PUBLIC={'yes','permissive','designated','official','discouraged'}
PURPOSE={'destination','customers','delivery','agricultural','forestry','permit','residents'}
DENIED={'no','private','use_sidepath','dismount'}|PURPOSE
SIDES=('forward','backward')
ACCESS_TAGS=[mode+suffix for mode in MODES for suffix in ('',':forward',':backward',':conditional',':forward:conditional',':backward:conditional')]
ONEWAY_TAGS=['oneway'+suffix for suffix in ('',':motor_vehicle',':motorcar',':conditional',':motor_vehicle:conditional',':motorcar:conditional')]
def norm(value):return str(value or '').strip().lower()
def access_value(tags,side=None):
 for mode in MODES:
  keys=[mode+':'+side,mode] if side else [mode]
  for key in keys:
   if norm(tags.get(key)):return norm(tags[key]),key
 return '',None

def access_allowed(tags,side=None):
 # A more-specific unconditional permission supersedes less-specific conditions;
 # conditions at the same/more specific mode cannot be assumed inactive.
 for mode in MODES:
  keys=[mode+':'+side,mode] if side else [mode]
  for key in keys:
   if norm(tags.get(key+':conditional')):return False
   v=norm(tags.get(key))
   if v:return v in PUBLIC
 return True

def oneway(tags):
 for key in ('oneway:motorcar','oneway:motor_vehicle','oneway'):
  if norm(tags.get(key+':conditional')):return None
  v=norm(tags.get(key))
  if v in ('no','false','0'):return 0
  if v in ('yes','true','1'):return 1
  if v in ('-1','reverse'):return -1
  if v:return None # reversible/alternating/unknown are not a fixed direction
 if norm(tags.get('junction')) in ('roundabout','circular'):return 1
 if norm(tags.get('highway')) in ('motorway','motorway_link'):return 1
 return 0

def direction(tags):
 d=oneway(tags)
 if d is None:return None
 forward=d>=0 and access_allowed(tags,'forward')
 backward=d<=0 and access_allowed(tags,'backward')
 if forward and backward:return 0
 if forward:return 1
 if backward:return -1
 return None

def barrier_blocked(tags):
 # Explicit restrictions apply even to nodes without a barrier tag.
 value,key=access_value(tags)
 if not access_allowed(tags):return True
 if key:return False
 b=norm(tags.get('barrier'))
 return b in {'bollard','block','bus_trap','cycle_barrier','stile','turnstile','kissing_gate','height_restrictor','jersey_barrier','chain','sump_buster','yes'}

def split_at_barriers(obj,blocked):
 p=obj.get('properties') or {};nodes=p.get('@way_nodes') or [];g=obj.get('geometry') or {};points=g.get('coordinates') or []
 if len(nodes)!=len(points) or not any(n in blocked for n in nodes):yield obj;return
 start=0
 for end in [i for i,n in enumerate(nodes) if n in blocked]+[len(nodes)]:
  if end-start>=2:yield {**obj,'properties':{**p,'@way_nodes':nodes[start:end]},'geometry':{**g,'coordinates':points[start:end]}}
  start=end+1
