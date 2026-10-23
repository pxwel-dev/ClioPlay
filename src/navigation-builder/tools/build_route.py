#!/usr/bin/env python3
from __future__ import annotations
import argparse,json,math,os,shutil,sqlite3,struct,subprocess,sys,time,urllib.parse
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
try: import build_map as bm
except Exception: bm=None

MAGIC=b'CPR2'; VERSION=5; TILE_SPAN=256
# v4: compact physical road graph + selective arrival-context states at turn-restricted junctions.
HDR=struct.Struct('<4sHH' + 'I'*21 + 'iiii')
TILE=struct.Struct('<iiII')
NODE=struct.Struct('<iiIHHIIIIHHI')         # lat,lon,first_in,in_count,flags,phys,rank,comp,upf_first,upf_count,upr_count,upr_first
EDGE=struct.Struct('<I')
BASE=struct.Struct('<IIIIHHBBH')            # cost,geom_first,src_first,to_state,geom_count,src_count,cls,flags,pad
CHARC=struct.Struct('<IIIII BBH')            # route-state CH arc; base: left=physical base arc id; shortcut: left/right child arcs
UREF=struct.Struct('<I')
POINT=struct.Struct('<ii')
RAW_HDR=struct.Struct('<4sIIII')             # CRW4,ver,state_nodes,route_arcs,geom_points
RAW_ARC=CHARC
CH_HDR=struct.Struct('<4sIIIIII')
ARC_SHORTCUT=1; ARC_GEOM_REVERSED=2; ARC_ROUNDABOUT=4

ROUTABLE={
 'motorway':1,'motorway_link':1,'trunk':2,'trunk_link':2,'primary':3,'primary_link':3,
 'secondary':4,'secondary_link':4,'tertiary':5,'tertiary_link':5,'residential':6,
 'unclassified':6,'living_street':6,'service':7,
}
DEFAULT_MPH={1:60,2:55,3:45,4:40,5:35,6:25,7:15}; BLOCK_ACCESS={'no','private'}
WAY_TAGS=['highway','oneway','junction','access','vehicle','motor_vehicle','maxspeed','maxspeed:forward','maxspeed:backward','maxspeed:conditional','maxspeed:type','source:maxspeed','zone:maxspeed','name','ref']
from routing_policy import ACCESS_TAGS,ONEWAY_TAGS,access_allowed,direction,split_at_barriers,barrier_blocked
WAY_TAGS=list(dict.fromkeys(WAY_TAGS+ACCESS_TAGS+ONEWAY_TAGS+['barrier','opening_hours','impassable']))
BLOCKED_NODES=set()
CAR_EXCEPT={'motorcar','motor_vehicle','vehicle','car'}


def eprint(*a): print(*a,file=sys.stderr,flush=True)
def floor_div(a,b): return a//b if a>=0 else -((-a+b-1)//b)
def tile_of(lat,lon): return floor_div(lon,TILE_SPAN),floor_div(lat,TILE_SPAN)
def norm(v): return str(v or '').strip().lower()
def access_blocked(p):return not(access_allowed(p,'forward') or access_allowed(p,'backward'))
def oneway_dir(p):return direction(p)
def explicit_speed(props):
 from global_speeds import directions, mph_value
 f,b=directions(props)
 candidates=[mph_value(x) for x in (f,b) if x[0]>0 and not x[1]&12]
 return min(candidates) if candidates else 0

def segment_m(a,b):
 lon1,lat1=a;lon2,lat2=b;dy=(lat2-lat1)*111320.;dx=(lon2-lon1)*111320.*math.cos(math.radians((lat1+lat2)*.5));return math.hypot(dx,dy)
def cost_ds(a,b,mph): return max(1,int(round(segment_m(a,b)/max(1.,mph*.44704)*10.)))
def parse_way(obj,speed_lookup=None):
 p=obj.get('properties') or {};h=norm(p.get('highway'));cls=ROUTABLE.get(h)
 if not cls or norm(p.get("impassable"))=="yes":return None
 d=oneway_dir(p)
 if d is None:return None
 g=obj.get('geometry') or {};coords=g.get('coordinates') or [];nodes=p.get('@way_nodes') or []
 if g.get('type')!='LineString' or len(coords)<2 or len(nodes)!=len(coords):return None
 try:nodes=[int(x) for x in nodes];pts=[(float(q[0]),float(q[1])) for q in coords];wid=int(p.get('@id') or 0)
 except Exception:return None
 mph=0
 if speed_lookup is not None:
  try:mph=int(speed_lookup(wid) or 0)
  except Exception:mph=0
 if not mph:mph=explicit_speed(p)
 if not mph:mph=DEFAULT_MPH[cls]
 return wid,nodes,pts,cls,mph,(ARC_ROUNDABOUT if norm(p.get('junction'))=='roundabout' else 0),d


def open_db(path):
 if path.exists():path.unlink()
 db=sqlite3.connect(path)
 db.execute('pragma journal_mode=off');db.execute('pragma synchronous=off');db.execute('pragma temp_store=file');db.execute('pragma cache_size=-196608')
 db.execute('create table nodes(osm_id integer primary key,lat integer not null,lon integer not null,tx integer not null,ty integer not null,uses integer not null default 0,endpoint integer not null default 0)')
 return db

def speed_lookup_from_db(path):
 if not path or not path.exists():return None,None
 db=sqlite3.connect(path);cur=db.cursor()
 def get(wid):
  row=cur.execute('select source from speed_way where way_id=?',(wid,)).fetchone()
  from global_speeds import decode_source,mph_value
  speeds=[mph_value(v) for v in decode_source(row[0]) if v[0]>0 and not v[1]&12] if row else []
  return min(speeds) if speeds else 0
 return get,db

def export_ways(osmium,pbf,work,pass_no):
 cfg=work/'route-export.json'
 cfg.write_text(json.dumps({'attributes':{'id':True,'way_nodes':True},'format_options':{},'linear_tags':True,'area_tags':False,'include_tags':WAY_TAGS},separators=(',',':')))
 idx=work/f'route-node-index-p{pass_no}.dat';idx.unlink(missing_ok=True)
 cmd=[osmium,'export',str(pbf),'-f','geojsonseq','--geometry-types=linestring','-c',str(cfg),'-x','print_record_separator=false','-i',f'sparse_file_array,{idx}','--progress']
 eprint('+',' '.join(map(str,cmd)))
 return subprocess.Popen(cmd,stdout=subprocess.PIPE,text=True,encoding='utf-8',errors='replace',bufsize=1024*1024)

def iter_json_lines(lines):
 for line in lines:
  line=line.strip('\x1e\r\n ')
  if not line:continue
  try:
   obj=json.loads(line)
   yield from split_at_barriers(obj,BLOCKED_NODES)
  except Exception:continue

# ---------- turn restrictions ----------
def opl_decode(s): return urllib.parse.unquote(s)
def parse_opl_kv(field):
 out={}
 if not field:return out
 for ent in field.split(','):
  if not ent:continue
  if '=' in ent:k,v=ent.split('=',1)
  else:k,v=ent,''
  out[opl_decode(k)]=opl_decode(v)
 return out

def parse_opl_members(field):
 out=[]
 for ent in field.split(',') if field else ():
  if len(ent)<2:continue
  typ=ent[0]
  try:
   core,role=(ent[1:].split('@',1)+[''])[:2]
   out.append((typ,int(core),opl_decode(role)))
  except Exception:continue
 return out

def restriction_kind(tags):
 # Prefer mode-specific restrictions for cars when present.
 for k in ('restriction:motorcar','restriction:motor_vehicle','restriction:vehicle','restriction'):
  v=norm(tags.get(k))
  if v:return v
 return ''

def relation_to_restriction(rel_id,tags,members):
 if norm(tags.get('type'))!='restriction':return None,'not_restriction'
 exc={norm(x) for x in str(tags.get('except') or '').replace(',',';').split(';') if norm(x)}
 if exc & CAR_EXCEPT:return None,'car_excepted'
 kind=restriction_kind(tags)
 if not(kind.startswith('no_') or kind.startswith('only_')):return None,'unsupported_kind'
 froms=[mid for typ,mid,role in members if typ=='w' and role=='from']
 tos=[mid for typ,mid,role in members if typ=='w' and role=='to']
 vnodes=[mid for typ,mid,role in members if typ=='n' and role=='via']
 # Preserve relation-member order for via ways: it defines the restricted corridor.
 vways=[mid for typ,mid,role in members if typ=='w' and role=='via']
 if not froms or not tos:return None,'bad_members'
 if vways and vnodes:return None,'bad_members'
 if vways:
  return {'id':int(rel_id),'kind':kind,'from_ways':tuple(sorted(set(froms))),'to_ways':tuple(sorted(set(tos))),'via_ways':tuple(vways)},'ok_via_way'
 if len(vnodes)!=1:return None,'bad_members'
 return {'id':int(rel_id),'kind':kind,'from_ways':tuple(sorted(set(froms))),'to_ways':tuple(sorted(set(tos))),'via_node':int(vnodes[0]),'via_ways':()},'ok'

def collect_barriers(pbf):
 import osmium
 class Handler(osmium.SimpleHandler):
  def node(self,n):
   if len(n.tags) and barrier_blocked(dict(n.tags)):BLOCKED_NODES.add(n.id)
 BLOCKED_NODES.clear();Handler().apply_file(str(pbf));return len(BLOCKED_NODES)

def extract_restrictions(osmium,pbf,work):
 rpbf=work/'turn-restrictions.osm.pbf'
 cmd=[osmium,'tags-filter',str(pbf),'r/type=restriction','-o',str(rpbf),'--overwrite']
 eprint('+',' '.join(map(str,cmd)));subprocess.check_call(cmd)
 cmd=[osmium,'cat',str(rpbf),'-f','opl'];eprint('+',' '.join(map(str,cmd)))
 p=subprocess.Popen(cmd,stdout=subprocess.PIPE,text=True,encoding='utf-8',errors='replace',bufsize=1024*1024)
 restrictions=[];stats={'relations':0,'accepted':0,'node_via':0,'via_way':0,'car_excepted':0,'unsupported_kind':0,'bad_members':0,'not_restriction':0}
 assert p.stdout is not None
 for line in p.stdout:
  if not line.startswith('r'):continue
  toks=line.rstrip('\n').split(' ');rid=0;tags={};members=[]
  try:rid=int(toks[0][1:])
  except Exception:continue
  for t in toks[1:]:
   if t.startswith('T'):tags=parse_opl_kv(t[1:])
   elif t.startswith('M'):members=parse_opl_members(t[1:])
  stats['relations']+=1;r,why=relation_to_restriction(rid,tags,members)
  if r:
   restrictions.append(r);stats['accepted']+=1
   if why=='ok_via_way':stats['via_way']+=1
   else:stats['node_via']+=1
  else:stats[why]=stats.get(why,0)+1
 p.stdout.close();rc=p.wait()
 if rc:raise SystemExit(f'osmium restriction export failed: {rc}')
 eprint('  turn restrictions:',json.dumps(stats,sort_keys=True))
 return restrictions,stats


def observe_pass(objs,db,speed_lookup=None):
 ways=points=bad=0;last=time.monotonic();sql='''insert into nodes(osm_id,lat,lon,tx,ty,uses,endpoint) values(?,?,?,?,?,?,?)
 on conflict(osm_id) do update set uses=nodes.uses+1,endpoint=max(nodes.endpoint,excluded.endpoint)'''
 for obj in objs:
  w=parse_way(obj,speed_lookup)
  if not w:continue
  wid,nodes,pts,cls,mph,wayflags,d=w;rows=[]
  for i,(nid,(lon,lat)) in enumerate(zip(nodes,pts)):
   la=int(round(lat*1e5));lo=int(round(lon*1e5));tx,ty=tile_of(la,lo);rows.append((nid,la,lo,tx,ty,1,1 if i==0 or i==len(nodes)-1 else 0))
  db.executemany(sql,rows);ways+=1;points+=len(rows)
  if ways%100000==0 and time.monotonic()-last>2:
   eprint(f'  pass1 routable ways={ways:,} node occurrences={points:,}');db.commit();last=time.monotonic()
 db.commit();return {'ways':ways,'node_occurrences':points,'bad':bad}

def mark_restriction_vias(db,restrictions):
 ids=sorted({r['via_node'] for r in restrictions if r.get('via_node') is not None})
 hit=0
 for s in range(0,len(ids),800):
  ch=ids[s:s+800];qm=','.join('?'*len(ch));cur=db.execute(f'update nodes set endpoint=1 where osm_id in ({qm})',ch);hit+=max(0,cur.rowcount)
 db.commit();eprint(f'  restriction via nodes retained as topology vertices: requested={len(ids):,} matched={hit:,}')
 return hit

def build_topology_map(db):
 eprint('  selecting routing vertices (shared nodes + way endpoints + restriction vias)...')
 db.execute('create table topo(osm_id integer primary key,node_id integer unique,lat integer,lon integer,tx integer,ty integer)')
 batch=[];nid=0
 q='select osm_id,lat,lon,tx,ty from nodes where uses>1 or endpoint=1 order by tx,ty,lat,lon,osm_id'
 for osm,lat,lon,tx,ty in db.execute(q):
  batch.append((osm,nid,lat,lon,tx,ty));nid+=1
  if len(batch)>=50000:db.executemany('insert into topo values(?,?,?,?,?,?)',batch);batch.clear()
 if batch:db.executemany('insert into topo values(?,?,?,?,?,?)',batch)
 db.execute('create index topo_id on topo(node_id)')
 db.execute('''create table base_arcs(
   id integer primary key,fr_id integer,to_id integer,cost integer,cls integer,
   geom_first integer,geom_count integer,flags integer,way_id integer,piece_id integer,to_state integer)''')
 db.execute('create index base_to on base_arcs(to_id)');db.execute('create index base_fr on base_arcs(fr_id)')
 db.execute('create table tile_refs(tx integer,ty integer,arc_id integer)')
 db.commit();total=int(db.execute('select count(*) from nodes').fetchone()[0]);eprint(f'  topology vertices={nid:,} from unique OSM nodes={total:,} ({(100.0*nid/max(1,total)):.1f}%)')
 return nid,total

def topo_lookup(db,ids):
 out={};ids=list(dict.fromkeys(ids))
 for s in range(0,len(ids),800):
  ch=ids[s:s+800];qm=','.join('?'*len(ch))
  for osm,nid in db.execute(f'select osm_id,node_id from topo where osm_id in ({qm})',ch):out[int(osm)]=int(nid)
 return out

def tiles_for_geom(e5pts):
 out=set()
 for i,(la,lo) in enumerate(e5pts):
  x,y=tile_of(la,lo);out.add((x,y))
  if i==0:continue
  pla,plo=e5pts[i-1];x0,y0=tile_of(pla,plo);x1,y1=x,y;dx=abs(x1-x0);sx=1 if x0<x1 else -1;dy=-abs(y1-y0);sy=1 if y0<y1 else -1;err=dx+dy
  while True:
   out.add((x0,y0))
   if x0==x1 and y0==y1:break
   e2=2*err
   if e2>=dy:err+=dy;x0+=sx
   if e2<=dx:err+=dx;y0+=sy
 return out

def compress_batch(batch,db,geomf,geom_point_count,arc_id,piece_id,speed_lookup=None):
 parsed=[];allids=[]
 for obj in batch:
  w=parse_way(obj,speed_lookup)
  if w:parsed.append(w);allids.extend(w[1])
 topo=topo_lookup(db,allids);arcs=[];trefs=[];pieces=0;shape_points=0
 for wid,nodes,pts,cls,mph,wayflags,d in parsed:
  cuts=[i for i,n in enumerate(nodes) if n in topo]
  if not cuts or cuts[0]!=0 or cuts[-1]!=len(nodes)-1:raise RuntimeError('topology endpoint invariant failed')
  for ci in range(1,len(cuts)):
   i0,i1=cuts[ci-1],cuts[ci]
   if i1<=i0:continue
   fr,to=topo[nodes[i0]],topo[nodes[i1]]
   if fr==to:continue
   g=[];cost=0
   for j in range(i0,i1+1):
    lon,lat=pts[j];g.append((int(round(lat*1e5)),int(round(lon*1e5))))
    if j>i0:cost+=cost_ds(pts[j-1],pts[j],mph)
   if len(g)<2 or len(g)>65535:raise RuntimeError('geometry point-count overflow')
   gf=geom_point_count
   for la,lo in g:geomf.write(POINT.pack(la,lo))
   geom_point_count+=len(g);shape_points+=len(g);pieces+=1;pid=piece_id;piece_id+=1;tids=tiles_for_geom(g)
   if d>=0:
    arcs.append((arc_id,fr,to,cost,cls,gf,len(g),wayflags,wid,pid,to));trefs.extend((tx,ty,arc_id) for tx,ty in tids);arc_id+=1
   if d<=0:
    arcs.append((arc_id,to,fr,cost,cls,gf,len(g),wayflags|ARC_GEOM_REVERSED,wid,pid,fr));trefs.extend((tx,ty,arc_id) for tx,ty in tids);arc_id+=1
 if arcs:db.executemany('insert into base_arcs values(?,?,?,?,?,?,?,?,?,?,?)',arcs)
 if trefs:db.executemany('insert into tile_refs values(?,?,?)',trefs)
 return geom_point_count,arc_id,piece_id,pieces,shape_points,len(parsed)

def compression_pass(objs,db,geom_path,speed_lookup=None,batch_ways=800):
 geom_path.unlink(missing_ok=True);geom_point_count=0;arc_id=0;piece_id=0;pieces=shape_points=ways=0;batch=[];last=time.monotonic()
 with geom_path.open('wb') as gf:
  for obj in objs:
   batch.append(obj)
   if len(batch)>=batch_ways:
    geom_point_count,arc_id,piece_id,p,s,w=compress_batch(batch,db,gf,geom_point_count,arc_id,piece_id,speed_lookup);pieces+=p;shape_points+=s;ways+=w;batch.clear()
    if ways and ways%100000<batch_ways and time.monotonic()-last>2:
     eprint(f'  pass2 ways={ways:,} compressed pieces={pieces:,} directed base arcs={arc_id:,} stored geom pts={geom_point_count:,}');db.commit();last=time.monotonic()
  if batch:
   geom_point_count,arc_id,piece_id,p,s,w=compress_batch(batch,db,gf,geom_point_count,arc_id,piece_id,speed_lookup);pieces+=p;shape_points+=s;ways+=w
  gf.flush();os.fsync(gf.fileno())
 db.execute('create index tile_refs_xy on tile_refs(tx,ty,arc_id)');db.execute('create index base_to_way on base_arcs(to_id,way_id)');db.execute('create index base_fr_way on base_arcs(fr_id,way_id)');db.execute('create index base_piece on base_arcs(piece_id)');db.execute('create index base_way on base_arcs(way_id,fr_id,to_id,id)');db.commit()
 return {'ways':ways,'compressed_pieces':pieces,'base_arcs':arc_id,'geometry_points':geom_point_count}


def _way_arcs(db,wid,cache):
 wid=int(wid)
 if wid not in cache:
  cache[wid]=[tuple(map(int,r)) for r in db.execute('select id,fr_id,to_id,piece_id,cost from base_arcs where way_id=? order by id',(wid,))]
 return cache[wid]

def _directed_via_path(db,via_ways,start,end,way_cache,max_settled=8192):
 """Shortest legal physical-base-arc path from start to end following via ways in relation order.
 Each via way must contribute at least one directed arc. Returns tuple(base_arc_ids) or None."""
 import heapq
 via=tuple(map(int,via_ways))
 if not via:return () if int(start)==int(end) else None
 outs={}
 for i,wid in enumerate(via):
  d={}
  for aid,fr,to,piece,cost in _way_arcs(db,wid,way_cache):d.setdefault(fr,[]).append((aid,to,cost))
  outs[i]=d
 # state=(node,way_index,used_current_way)
 st=(int(start),0,0);dist={st:0};parent={};q=[(0,st)];settled=0;goal=None
 while q and settled<max_settled:
  du,s=heapq.heappop(q)
  if du!=dist.get(s):continue
  settled+=1;node,i,used=s
  if i==len(via)-1 and used and node==int(end):goal=s;break
  # Follow another directed arc on this via way.
  for aid,to,cost in outs[i].get(node,()):
   ns=(to,i,1);nd=du+cost
   if nd<dist.get(ns,1<<62):dist[ns]=nd;parent[ns]=(s,aid);heapq.heappush(q,(nd,ns))
  # Move to the next via-way member only after traversing at least one arc of this member.
  if used and i+1<len(via) and outs[i+1].get(node):
   ns=(node,i+1,0);nd=du
   if nd<dist.get(ns,1<<62):dist[ns]=nd;parent[ns]=(s,None);heapq.heappush(q,(nd,ns))
 if goal is None:return None
 path=[];s=goal
 while s!=st:
  ps,aid=parent[s]
  if aid is not None:path.append(int(aid))
  s=ps
 path.reverse();return tuple(path)

def resolve_restriction_patterns(db,restrictions):
 """Resolve OSM node-via and via-way restrictions to selective physical-arc patterns.
 Pattern semantics: after in_arc, traverse via[]; at its end targets[] are forbidden (no_*)
 or the only legal exits (only_*)."""
 way_cache={};patterns=[];stats={'relations_input':len(restrictions),'node_via_resolved':0,'via_way_resolved':0,'relations_unresolved':0,'patterns':0,'via_path_searches':0}
 for r in restrictions:
  kind=r['kind'];made=[]
  if r.get('via_ways'):
   via=tuple(map(int,r['via_ways']))
   first,last=via[0],via[-1]
   # Incoming from-way arcs must enter a node from which the first via way can be traversed.
   first_out_nodes={fr for aid,fr,to,piece,cost in _way_arcs(db,first,way_cache)}
   incoming=[]
   for wid in r['from_ways']:
    incoming.extend(a for a in _way_arcs(db,wid,way_cache) if a[2] in first_out_nodes)
   # Candidate final exits are directed to-way arcs beginning on the last via way.
   last_nodes=set()
   for aid,fr,to,piece,cost in _way_arcs(db,last,way_cache):last_nodes.add(fr);last_nodes.add(to)
   by_end={}
   for wid in r['to_ways']:
    for a in _way_arcs(db,wid,way_cache):
     if a[1] in last_nodes:by_end.setdefault(a[1],[]).append(a)
   for inc in incoming:
    iid,ifr,istart,ipiece,icost=inc
    for end,tos in by_end.items():
     stats['via_path_searches']+=1
     path=_directed_via_path(db,via,istart,end,way_cache)
     if path is None:continue
     targets=tuple(sorted({int(a[0]) for a in tos}))
     if targets:made.append((iid,path,targets))
   # Deduplicate identical resolved corridors/targets.
   made=list(dict.fromkeys(made))
   if made:stats['via_way_resolved']+=1
  else:
   row=db.execute('select node_id from topo where osm_id=?',(r.get('via_node'),)).fetchone()
   if row:
    via=int(row[0]);incoming=[];outgoing=[]
    for wid in r['from_ways']:
     incoming.extend(db.execute('select id,fr_id,way_id,piece_id,cost from base_arcs where to_id=? and way_id=?',(via,int(wid))).fetchall())
    for wid in r['to_ways']:
     outgoing.extend(db.execute('select id,to_id,way_id,piece_id,cost from base_arcs where fr_id=? and way_id=?',(via,int(wid))).fetchall())
    for inc in incoming:
     iid,ifr,iw,ip,icost=map(int,inc);targets=[]
     if 'u_turn' in kind:
      targets=[int(o[0]) for o in outgoing if int(o[3])==ip]
      if not targets:targets=[int(o[0]) for o in outgoing if int(o[1])==ifr]
     elif 'straight_on' in kind and set(r['from_ways']) & set(r['to_ways']):
      targets=[int(o[0]) for o in outgoing if int(o[3])!=ip]
     else:targets=[int(o[0]) for o in outgoing]
     if targets:made.append((iid,(),tuple(sorted(set(targets)))))
   if made:stats['node_via_resolved']+=1
  if not made:
   stats['relations_unresolved']+=1;continue
  for iid,path,targets in made:
   patterns.append({'rel_id':int(r['id']),'kind':kind,'in_arc':int(iid),'via':tuple(path),'targets':tuple(targets)})
 # Exact duplicates can occur where a relation has repeated members or bidirectional aliases.
 uniq={}
 for p in patterns:
  k=(p['kind'],p['in_arc'],p['via'],p['targets']);uniq[k]=p
 patterns=list(uniq.values());stats['patterns']=len(patterns)
 return patterns,stats

def _add_restriction_exit_cooldowns(db,patterns):
 """Add one-step no-immediate-reverse contexts after a restriction is legally escaped/completed.
 This prevents a route from satisfying a turn restriction by taking a tiny side road, U-turning,
 and immediately returning to the prohibited manoeuvre. Only roads adjacent to restriction
 contexts receive these states; this is not a global edge-state expansion."""
 originals=list(patterns);synthetic=[];seen=set()
 arcinfo={}
 def info(aid):
  aid=int(aid)
  if aid not in arcinfo:
   r=db.execute('select fr_id,to_id,piece_id from base_arcs where id=?',(aid,)).fetchone();arcinfo[aid]=tuple(map(int,r)) if r else (-1,-1,-1)
  return arcinfo[aid]
 def reverse_targets(aid):
  fr,to,piece=info(aid)
  return tuple(sorted(int(r[0]) for r in db.execute('select id from base_arcs where fr_id=? and piece_id=?',(to,piece))))
 for p in originals:
  via=p['via'];kind=p['kind'];cur=info(p['in_arc'])[1]
  stages=[]
  for pos in range(len(via)+1):
   outs=[int(r[0]) for r in db.execute('select id from base_arcs where fr_id=? order by id',(cur,))]
   prev=via[pos-1] if pos>0 else None
   if pos<len(via):
    expected=via[pos]
    endings=[] if kind.startswith('only_') else [a for a in outs if a!=expected and not(prev is not None and info(a)[2]==info(prev)[2])]
    stages.extend(endings);cur=info(expected)[1]
   else:
    if kind.startswith('only_'):endings=[a for a in outs if a in p['targets']]
    else:endings=[a for a in outs if a not in p['targets'] and not(prev is not None and info(a)[2]==info(prev)[2])]
    stages.extend(endings)
  for aid in stages:
   rt=reverse_targets(aid)
   if not rt:continue
   k=(aid,rt)
   if k in seen:continue
   seen.add(k);synthetic.append({'rel_id':-1,'kind':'no_u_turn_cooldown','in_arc':aid,'via':(),'targets':rt,'synthetic':True})
 start=len(patterns);patterns.extend(synthetic);return {int(p['in_arc']):start+i for i,p in enumerate(synthetic)},len(synthetic)

def resolve_turn_restrictions(db,restrictions,nphys):
 """Build a selective turn-restriction automaton over the compact physical road graph.
 Ordinary junctions stay as one route state. Extra states exist only while a no/only restriction
 is active; via-way restrictions propagate that context along the declared via corridor."""
 from collections import defaultdict,deque
 patterns,pstats=resolve_restriction_patterns(db,restrictions)
 cooldown_by_arc,anti=_add_restriction_exit_cooldowns(db,patterns);pstats['anti_bypass_patterns']=anti;pstats['patterns_total']=len(patterns)
 triggers=defaultdict(list)
 for pid,p in enumerate(patterns):
  if not p.get('synthetic'):triggers[p['in_arc']].append(pid)
 piece_cache={}
 def piece_of(aid):
  aid=int(aid)
  if aid not in piece_cache:
   row=db.execute('select piece_id from base_arcs where id=?',(aid,)).fetchone();piece_cache[aid]=int(row[0]) if row else -1
  return piece_cache[aid]

 # Generic context tables. A duplicate route arc may target a context state that differs from
 # the physical base arc's default target; this is what permits via-way history to propagate.
 db.execute('create table extra_states(state_id integer primary key,phys_id integer,ctx text)')
 db.execute('create table extra_sources(base_arc integer,state_id integer,to_state integer,route_arc_id integer,primary key(base_arc,state_id))')
 sid_next=nphys;state_by_key={};queue=deque()
 def active_norm(active):return tuple(sorted(set((int(pid),int(pos)) for pid,pos in active)))
 def get_state(phys,active):
  nonlocal sid_next
  active=active_norm(active);key=(int(phys),active)
  sid=state_by_key.get(key)
  if sid is not None:return sid
  sid=sid_next;sid_next+=1;state_by_key[key]=sid
  ctx=';'.join(f'{pid}:{pos}' for pid,pos in active)
  db.execute('insert into extra_states values(?,?,?)',(sid,int(phys),ctx));queue.append((sid,int(phys),active));return sid

 # Traversing a triggering physical arc from an ordinary state activates all restrictions tied
 # to that arrival. Update the physical arc's default CH target accordingly.
 for aid,pids in sorted(triggers.items()):
  row=db.execute('select to_id from base_arcs where id=?',(int(aid),)).fetchone()
  if not row:continue
  target=get_state(int(row[0]),[(pid,0) for pid in pids])
  db.execute('update base_arcs set to_state=? where id=?',(target,int(aid)))

 generated=0;max_active=0
 while queue:
  sid,phys,active=queue.popleft();max_active=max(max_active,len(active))
  outs=[tuple(map(int,r)) for r in db.execute('select id,to_id from base_arcs where fr_id=? order by id',(phys,))]
  for aid,to_phys in outs:
   legal=True;next_active=[];ended_restriction=False
   only_entries=[]
   # no_* restrictions independently forbid their terminal manoeuvre; deviating before the
   # end of a no_* via corridor drops that restriction but carries one transient anti-reverse
   # state so an artificial out-and-back cannot be used to bypass it.
   for pid,pos in active:
    p=patterns[pid];via=p['via']
    if p.get('synthetic'):
     if aid in p['targets']:legal=False;break
     continue
    is_only=p['kind'].startswith('only_')
    if is_only:
     only_entries.append((pid,pos));continue
    if pos<len(via):
     if aid==via[pos]:next_active.append((pid,pos+1))
     elif pos>0 and piece_of(aid)==piece_of(via[pos-1]):
      legal=False;break
     else:ended_restriction=True
    else:
     if aid in p['targets']:legal=False;break
     if pos>0 and piece_of(aid)==piece_of(via[pos-1]):legal=False;break
     ended_restriction=True
   if not legal:continue
   # Multiple only_* records with the same arrival are alternatives: the chosen road must match
   # at least one active only pattern, and only the matching alternatives remain active.
   if only_entries:
    matched=[]
    for pid,pos in only_entries:
     p=patterns[pid];via=p['via']
     if pos<len(via):
      if aid==via[pos]:matched.append((pid,pos+1))
     elif aid in p['targets']:
      ended_restriction=True
     else:continue
    terminal_match=any(pos>=len(patterns[pid]['via']) and aid in patterns[pid]['targets'] for pid,pos in only_entries)
    if not matched and not terminal_match:continue
    next_active.extend(matched)
   # Anti-reversal context is transition-specific: the same road remains normal when the car
   # starts/reroutes there without having traversed the restriction corridor first.
   if ended_restriction and aid in cooldown_by_arc:next_active.append((cooldown_by_arc[aid],0))
   # The road just taken can itself trigger another real restriction at its destination.
   next_active.extend((pid,0) for pid in triggers.get(aid,()))
   if next_active:to_state=get_state(to_phys,next_active)
   else:to_state=to_phys
   db.execute('insert or ignore into extra_sources(base_arc,state_id,to_state,route_arc_id) values(?,?,?,null)',(aid,sid,to_state));generated+=1

 base_m=int(db.execute('select count(*) from base_arcs').fetchone()[0]);rid=base_m
 for ba,st,to_state in db.execute('select base_arc,state_id,to_state from extra_sources order by state_id,base_arc').fetchall():
  db.execute('update extra_sources set route_arc_id=? where base_arc=? and state_id=?',(rid,int(ba),int(st)));rid+=1
 db.execute('create index extra_states_phys on extra_states(phys_id,state_id)');db.execute('create index extra_sources_base on extra_sources(base_arc,state_id)');db.execute('create index extra_sources_route on extra_sources(route_arc_id)');db.execute('create index extra_sources_to on extra_sources(to_state,route_arc_id)');db.commit()
 stats={**pstats,'incoming_contexts':len(triggers),'extra_states':sid_next-nphys,'extra_route_arcs':int(db.execute('select count(*) from extra_sources').fetchone()[0]),'max_active_restrictions':max_active}
 eprint('  turn-state automaton:',json.dumps(stats,sort_keys=True))
 return sid_next,stats

def write_raw(db,nstate,geom_points,path):
 base_m=int(db.execute('select count(*) from base_arcs').fetchone()[0]);extra_m=int(db.execute('select count(*) from extra_sources').fetchone()[0]);route_m=base_m+extra_m
 with path.open('wb') as f:
  f.write(RAW_HDR.pack(b'CRW4',1,nstate,route_m,geom_points))
  # Default physical-node state can take every outgoing arc.
  for aid,fr,to_state,c,cl in db.execute('select id,fr_id,to_state,cost,cls from base_arcs order by id'):
   f.write(RAW_ARC.pack(int(fr),int(to_state),int(c),int(aid),0,int(cl)&255,0,0))
  # Restricted arrival-context states duplicate only legal outgoing arcs.
  for sid,to_state,c,aid,cl in db.execute('''select es.state_id,es.to_state,b.cost,b.id,b.cls from extra_sources es join base_arcs b on b.id=es.base_arc order by es.route_arc_id'''):
   f.write(RAW_ARC.pack(int(sid),int(to_state),int(c),int(aid),0,int(cl)&255,0,0))
 return base_m,route_m

def ensure_preprocessor(work):
 src=HERE/'ch_preprocess.cpp';exe=work/'ch_preprocess';cc=shutil.which('c++') or shutil.which('g++') or shutil.which('clang++')
 if not cc:raise SystemExit('C++17 compiler required for CPR2 hierarchy preprocessing')
 cmd=[cc,'-O3','-std=c++17','-DNDEBUG',str(src),'-o',str(exe)];eprint('+',' '.join(cmd));subprocess.check_call(cmd);return exe

class CHMeta:
 def __init__(self,path,n_expected,route_m_expected):
  self.path=Path(path)
  with self.path.open('rb') as f:b=f.read(CH_HDR.size)
  if len(b)!=CH_HDR.size:raise RuntimeError('short CH output')
  magic,ver,self.n,self.route_m,self.arc_n,self.upf_n,self.upr_n=CH_HDR.unpack(b)
  if magic!=b'CH04' or ver!=3 or self.n!=n_expected or self.route_m!=route_m_expected:raise RuntimeError('bad CH preprocessor output')
  self.rank_off=CH_HDR.size;self.comp_off=self.rank_off+4*self.n;self.arc_off=self.comp_off+4*self.n;self.upfi_off=self.arc_off+CHARC.size*self.arc_n;self.upfr_off=self.upfi_off+4*(self.n+1);self.upri_off=self.upfr_off+4*self.upf_n;self.uprr_off=self.upri_off+4*(self.n+1)
  want=self.uprr_off+4*self.upr_n
  if self.path.stat().st_size!=want:raise RuntimeError(f'CH size mismatch: {self.path.stat().st_size} != {want}')

def iter_u32_region(path,off,count,chunk=65536):
 with Path(path).open('rb') as f:
  f.seek(off);left=count
  while left:
   n=min(left,chunk);b=f.read(n*4)
   if len(b)!=n*4:raise RuntimeError('short CH u32 region')
   for x in struct.unpack('<%dI'%n,b):yield x
   left-=n

def copy_region(path,off,nbytes,out,chunk=4*1024*1024):
 with Path(path).open('rb') as f:
  f.seek(off);left=nbytes
  while left:
   b=f.read(min(left,chunk))
   if not b:raise RuntimeError('short CH region')
   out.write(b);left-=len(b)

def pack(db,out,work,geom_path,nphys,nstate,raw_nodes,geom_points,witness_limit,max_shortcut_ratio,protected_nodes=None):
 raw=work/'cpr2.raw';physical_base_m,route_m=write_raw(db,nstate,geom_points,raw);prep=ensure_preprocessor(work);ch=work/'cpr2.ch'
 cmd=[str(prep),str(raw),str(ch),str(int(witness_limit)),str(float(max_shortcut_ratio))]
 if protected_nodes is not None:
  portal_file=work/'portals.u32';portal_file.write_bytes(b''.join(struct.pack('<I',int(i)) for i in sorted(set(protected_nodes))));cmd.append(str(portal_file))
 eprint('+',' '.join(cmd));subprocess.check_call(cmd);cm=CHMeta(ch,nstate,route_m)
 tile_rows=[];first=0
 for tx,ty,count in db.execute('select tx,ty,count(*) from tile_refs group by tx,ty order by tx,ty'):
  tile_rows.append((int(tx),int(ty),first,int(count)));first+=int(count)
 tile_ref_count=first;tiles=len(tile_rows);src_ref_count=physical_base_m+int(db.execute('select count(*) from extra_sources').fetchone()[0]);bounds=db.execute('select min(lat),min(lon),max(lat),max(lon) from nodes').fetchone()
 if not nstate or not nphys or not bounds or any(v is None for v in bounds):raise RuntimeError('No routable motorcar roads in region')
 tile_off=HDR.size;tref_off=tile_off+tiles*TILE.size;node_off=tref_off+tile_ref_count*4;edge_off=node_off+nstate*NODE.size;base_off=edge_off+route_m*EDGE.size;src_off=base_off+physical_base_m*BASE.size;arc_off=src_off+src_ref_count*4;upf_off=arc_off+cm.arc_n*CHARC.size;upr_off=upf_off+cm.upf_n*4;geom_off=upr_off+cm.upr_n*4
 if geom_off+geom_points*POINT.size+physical_base_m*8>0x7fffffff:raise RuntimeError('Routing pack exceeds device 2 GiB seek limit; choose a smaller regional extract')
 tmp=out.with_suffix(out.suffix+'.new');out.parent.mkdir(parents=True,exist_ok=True)
 ranks=iter_u32_region(ch,cm.rank_off,nstate);comps=iter_u32_region(ch,cm.comp_off,nstate);ufidx=iter_u32_region(ch,cm.upfi_off,nstate+1);uridx=iter_u32_region(ch,cm.upri_off,nstate+1);uf0=next(ufidx);ur0=next(uridx);maxcomp=0
 with tmp.open('wb') as f:
  f.write(HDR.pack(MAGIC,VERSION,3,nstate,nphys,physical_base_m,route_m,tiles,tile_ref_count,cm.arc_n,cm.upf_n,cm.upr_n,geom_points,src_ref_count,tile_off,tref_off,node_off,edge_off,base_off,src_off,arc_off,upf_off,upr_off,geom_off,*map(int,bounds)))
  for r in tile_rows:f.write(TILE.pack(*r))
  for (aid,) in db.execute('select arc_id from tile_refs order by tx,ty,arc_id'):f.write(UREF.pack(int(aid)))
  # Route states are physical default states first, then restriction arrival-context states.
  extra_iter=iter(db.execute('''select es.state_id,es.phys_id,t.lat,t.lon from extra_states es join topo t on t.node_id=es.phys_id order by es.state_id'''));erow=next(extra_iter,None)
  phys_iter=iter(db.execute('select node_id,lat,lon from topo order by node_id'));prow=next(phys_iter,None)
  incq=iter(db.execute('''select to_state,route_id from (select to_state as to_state,id as route_id from base_arcs union all select es.to_state as to_state,es.route_arc_id as route_id from extra_sources es) order by to_state,route_id'''));incrow=next(incq,None);incpos=0
  for sid in range(nstate):
   if sid<nphys:
    if prow is None or int(prow[0])!=sid:raise RuntimeError('physical state ordering invariant failed')
    phys=sid;lat=int(prow[1]);lon=int(prow[2]);prow=next(phys_iter,None)
   else:
    if erow is None or int(erow[0])!=sid:raise RuntimeError('extra state ordering invariant failed')
    phys=int(erow[1]);lat=int(erow[2]);lon=int(erow[3]);erow=next(extra_iter,None)
   rank=next(ranks);comp=next(comps);maxcomp=max(maxcomp,comp);uf1=next(ufidx);ur1=next(uridx)
   if uf1-uf0>65535 or ur1-ur0>65535:raise RuntimeError('route-state hierarchy degree overflow')
   first_in=incpos;inc_count=0
   while incrow is not None and int(incrow[0])==sid:inc_count+=1;incpos+=1;incrow=next(incq,None)
   if inc_count>65535:raise RuntimeError('route-state reverse degree overflow')
   f.write(NODE.pack(lat,lon,first_in,inc_count,0,phys,int(rank),int(comp),int(uf0),int(uf1-uf0),int(ur1-ur0),int(ur0)));uf0,ur0=uf1,ur1
  # Reverse adjacency for validation/reference Dijkstra: route-base CH arc ids ordered by destination state.
  for to_state,route_id in db.execute('''select to_state,route_id from (select to_state as to_state,id as route_id from base_arcs union all select es.to_state as to_state,es.route_arc_id as route_id from extra_sources es) order by to_state,route_id'''):f.write(EDGE.pack(int(route_id)))
  # Physical base arcs and their legal source-state lists.
  src_first=0
  exits={int(n) for n, in db.execute('select distinct fr_id from base_arcs where flags&4=0')}
  for aid,c,gf,gc,to_state,cl,flags,fr,to_phys in db.execute('select id,cost,geom_first,geom_count,to_state,cls,flags,fr_id,to_id from base_arcs order by id'):
   extras=[int(x[0]) for x in db.execute('select state_id from extra_sources where base_arc=? order by state_id',(int(aid),))];sc=1+len(extras)
   if sc>65535:raise RuntimeError('base source-state count overflow')
   f.write(BASE.pack(int(c),int(gf),int(src_first),int(to_state),int(gc),int(sc),int(cl)&255,int(flags)&255,int(bool(flags&ARC_ROUNDABOUT and to_phys in exits))));src_first+=sc
  if src_first!=src_ref_count:raise RuntimeError('source-ref count mismatch')
  for aid,fr in db.execute('select id,fr_id from base_arcs order by id'):
   f.write(UREF.pack(int(fr)))
   for (sid,) in db.execute('select state_id from extra_sources where base_arc=? order by state_id',(int(aid),)):f.write(UREF.pack(int(sid)))
  copy_region(ch,cm.arc_off,cm.arc_n*CHARC.size,f);copy_region(ch,cm.upfr_off,cm.upf_n*4,f);copy_region(ch,cm.uprr_off,cm.upr_n*4,f)
  with geom_path.open('rb') as gf:shutil.copyfileobj(gf,f,4*1024*1024)
  # flags bit1: lossless u64 OSM way identities following geometry; old readers
  # safely ignore this extension. Used to disambiguate parallel/stacked roads.
  for (wid,) in db.execute('select way_id from base_arcs order by id'):f.write(struct.pack('<Q',int(wid)))
  f.flush();os.fsync(f.fileno())
 os.replace(tmp,out);shortcuts=cm.arc_n-route_m;comps=maxcomp+1 if nstate else 0
 return {'raw_osm_nodes':raw_nodes,'physical_topology_nodes':nphys,'routing_state_nodes':nstate,'compression_ratio_nodes':round(raw_nodes/max(1,nphys),3),'physical_base_edges':physical_base_m,'route_base_edges':route_m,'tiles':tiles,'tile_refs':tile_ref_count,'source_refs':src_ref_count,'geometry_points':geom_points,'ch_arcs':cm.arc_n,'shortcuts':shortcuts,'shortcut_route_ratio':round(shortcuts/max(1,route_m),3),'up_forward':cm.upf_n,'up_reverse':cm.upr_n,'components':comps,'bytes':out.stat().st_size,'bounds_e5':list(map(int,bounds))}

# Demo: W -> X has a no-right restriction onto X -> N. A legal detour X->E->N remains.
def boundary_portals(db,geom,core):
 # Protect the endpoints of real physical arcs that cross a core edge. Protecting
 # every junction in a broad border band multiplies portals in dense cities and
 # can exceed device RAM even after recursive splitting. Endpoint identity is
 # stable across complete-way overlapping extracts; proximity is never a join.
 from shapely.geometry import box,LineString
 west,south,east,north=[round(x*1e5) for x in core];boundary=box(west,south,east,north).boundary
 selected=set();crossing_arcs=[];crossings=0;crossed_geometry={}
 db.execute('create index if not exists base_state_to on base_arcs(to_state)');db.execute('create index if not exists extra_source_state on extra_sources(state_id,to_state)');db.commit()
 with Path(geom).open('rb') as f:
  for aid,fr,to,dest,gf,gc in db.execute('select id,fr_id,to_id,to_state,geom_first,geom_count from base_arcs order by id'):
   if gf not in crossed_geometry:
    f.seek(int(gf)*8);raw=f.read(int(gc)*8)
    if len(raw)!=int(gc)*8:raise RuntimeError('short portal geometry')
    pts=[(lo,la) for la,lo in struct.iter_unpack('<ii',raw)];crossed_geometry[gf]=len(pts)>=2 and LineString(pts).intersects(boundary)
   if not crossed_geometry[gf]:continue
   crossings+=1;crossing_arcs.append((int(aid),));selected.update((int(fr),int(to)))
 # If a cut falls inside a via-way restriction, continue through the local
 # automaton to the first context-free states BEFORE and AFTER it. Protecting
 # only the physical endpoints would make legal long corridors disconnected.
 # The extra/context states themselves are never merged across packs.
 db.execute('create temp table atlas_crossing_arcs(id integer primary key)');db.executemany('insert into atlas_crossing_arcs values(?)',crossing_arcs)
 nphys=int(db.execute('select count(*) from topo').fetchone()[0]);original_count=len(selected)
 forward=[int(n) for n, in db.execute('select b.to_state from base_arcs b join atlas_crossing_arcs x on x.id=b.id')]
 variants=list(db.execute('select e.state_id,e.to_state from extra_sources e join atlas_crossing_arcs x on x.id=e.base_arc'));forward.extend(int(n) for _,n in variants);backward=[int(n) for n,_ in variants]
 seen_f=set();seen_b=set()
 while forward:
  state=forward.pop()
  if state<nphys:selected.add(state);continue
  if state in seen_f:continue
  seen_f.add(state);forward.extend(int(n) for n, in db.execute('select to_state from extra_sources where state_id=?',(state,)))
 while backward:
  state=backward.pop()
  if state<nphys:selected.add(state);continue
  if state in seen_b:continue
  seen_b.add(state);selected.update(int(n) for n, in db.execute('select fr_id from base_arcs where to_state=?',(state,)));backward.extend(int(n) for n, in db.execute('select state_id from extra_sources where to_state=?',(state,)))
 db.execute('drop table atlas_crossing_arcs')

 eprint(f'  atlas boundary crossings={crossings:,} protected default states={len(selected):,}')
 return selected,{'crossings':crossings,'context_free_endpoints_added':len(selected)-original_count,'forward_context_states':len(seen_f),'backward_context_states':len(seen_b),'arrival_context_transfers':0,'policy':'all physical default endpoints of crossing arcs; exact regional queries must prove arrival in that state'}

def write_portal_candidates(db,route,out,selected=None):
 # Export physical DEFAULT states only, never arrival-context extra states.
 # A receiving pack may begin at a source-only node and a sending pack may end
 # at a sink: exact destination queries prove the sending state is reachable.
 # Requiring both incoming/outgoing locally would disconnect long one-way roads.
 from inspect_route import CPR2
 db.execute('create index if not exists base_state_to on base_arcs(to_state)');db.commit()
 r=CPR2(route);rows=[]
 for osm,node,lat,lon in db.execute('select osm_id,node_id,lat,lon from topo order by osm_id'):
  if selected is None or int(node) in selected:rows.append((int(osm),int(node),int(lat),int(lon),int(r.node(node)[7])))
 r.f.close()
 with Path(out).open('wb') as f:
  f.write(struct.pack('<4sHHII',b'CPN1',1,24,len(rows),16))
  for row in rows:f.write(struct.pack('<QIiiI',*row))
 return len(rows)

def repack_for_atlas(route,work,portals):
 route=Path(route);work=Path(work);info=json.loads(route.with_suffix(route.suffix+'.json').read_text());db=sqlite3.connect(work/'route4.sqlite')
 try:
  result=pack(db,route,work,work/'cpr2.geom',info['physical_topology_nodes'],info['routing_state_nodes'],info['raw_osm_nodes'],info['geometry_points'],info['witness_settle_limit'],info['max_shortcut_ratio'],portals)
  info.update(result);info['atlas_portals']=len(set(portals));route.with_suffix(route.suffix+'.json').write_text(json.dumps(info,indent=2)+'\n');return info
 finally:db.close()

def demo_objects():
 def way(wid,nodes,pts,**tags):return {'type':'Feature','properties':{'@id':wid,'@way_nodes':nodes,'highway':'residential',**tags},'geometry':{'type':'LineString','coordinates':pts}}
 x,y=-1.65,54.98
 W=(x-.01,y);X=(x,y);N=(x,y+.01);E=(x+.01,y);D=(x+.01,y+.01)
 yield way(10,[1,2],[W,X],name='WEST',maxspeed='30 mph')
 yield way(20,[2,3],[X,N],name='NORTH',maxspeed='20 mph')
 yield way(30,[2,4],[X,E],name='EAST',maxspeed='30 mph')
 yield way(40,[4,5,3],[E,D,N],name='DETOUR',maxspeed='30 mph')

def demo_restrictions():
 return [{'id':9001,'kind':'no_right_turn','from_ways':(10,),'to_ways':(20,),'via_node':2}]

def run_osmium_pass(osmium,pbf,work,pass_no,func):
 proc=export_ways(osmium,pbf,work,pass_no)
 try:return func(iter_json_lines(proc.stdout))
 finally:
  proc.stdout.close();rc=proc.wait()
  if rc:raise SystemExit(f'osmium export pass {pass_no} failed: {rc}')

def main():
 ap=argparse.ArgumentParser(description='Build CPR2 v5 compact contraction hierarchy with selective OSM turn-restriction states')
 ap.add_argument('--pbf',type=Path);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--work',type=Path,default=Path('routework4'));ap.add_argument('--speed-db',type=Path);ap.add_argument('--demo',action='store_true')
 ap.add_argument('--portal-core',nargs=4,type=float,metavar=('WEST','SOUTH','EAST','NORTH'));ap.add_argument('--portal-margin',type=float,default=.03)
 ap.add_argument('--witness-settle-limit',type=int,default=4000);ap.add_argument('--max-shortcut-ratio',type=float,default=6.0)
 a=ap.parse_args();a.out=a.out.expanduser().resolve();a.work=a.work.expanduser().resolve();a.work.mkdir(parents=True,exist_ok=True);db=open_db(a.work/'route4.sqlite');sdb=None
 try:
  lookup,sdb=speed_lookup_from_db(a.speed_db.expanduser().resolve() if a.speed_db else None)
  if a.demo:
   restrictions=demo_restrictions();rstats={'relations':1,'accepted':1,'via_way':0};demo=list(demo_objects());stats1=observe_pass(iter(demo),db,lookup)
  else:
   if not a.pbf:raise SystemExit('--pbf required unless --demo')
   osmium=shutil.which('osmium')
   if not osmium:raise SystemExit('osmium-tool required')
   pbf=a.pbf.expanduser().resolve();barrier_count=collect_barriers(pbf)
   restrictions,rstats=extract_restrictions(osmium,pbf,a.work);stats1=run_osmium_pass(osmium,pbf,a.work,1,lambda it:observe_pass(it,db,lookup))
  mark_restriction_vias(db,restrictions);nphys,raw_nodes=build_topology_map(db);geom=a.work/'cpr2.geom'
  if not nphys:raise RuntimeError('No routable motorcar roads in region')
  if a.demo:stats2=compression_pass(iter(demo),db,geom,lookup)
  else:stats2=run_osmium_pass(osmium,pbf,a.work,2,lambda it:compression_pass(it,db,geom,lookup))
  nstate,trstats=resolve_turn_restrictions(db,restrictions,nphys)
  protected=None;portal_report=None
  if a.portal_core:
   protected,portal_report=boundary_portals(db,geom,a.portal_core)
   if len(protected)>8192:raise RuntimeError('Region has too many transfer states; use a smaller core')
  packed=pack(db,a.out,a.work,geom,nphys,nstate,raw_nodes,stats2['geometry_points'],a.witness_settle_limit,a.max_shortcut_ratio,protected)
  info={'atlas_boundary':portal_report,'atlas_portals':len(protected) if protected is not None else 0,'access_policy':'motorcar hierarchy and directional access; conditional access conservatively excluded from all-times graph; node barriers split; purpose-only roads excluded','blocked_barrier_nodes':0 if a.demo else barrier_count,'format':'CPR2','version':5,'hierarchy':'contraction','topology':'compressed-junction-graph+selective-turn-states','geometry':'base-arc-polylines','spatial_snap':'tile-to-physical-base-arc','turn_restrictions':True,'turn_restriction_scope':'unconditional OSM no_*/only_* restrictions with node-via and ordered via-way corridors; car/motor_vehicle exceptions honored','restriction_extract':rstats,'turn_state_expansion':trstats,'witness_settle_limit':a.witness_settle_limit,'max_shortcut_ratio':a.max_shortcut_ratio,'pass1':stats1,'pass2':stats2,**packed}
  write_portal_candidates(db,a.out,a.out.with_suffix('.cpn'),protected)
  a.out.with_suffix(a.out.suffix+'.json').write_text(json.dumps(info,indent=2));eprint('DONE',json.dumps(info,indent=2))
 finally:
  db.close()
  if sdb is not None:sdb.close()
if __name__=='__main__':main()
