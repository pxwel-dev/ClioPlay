#!/usr/bin/env python3
"""Build ClioPlay CPM5 driver-context offline maps from OSM/GeoJSON.

CPM5 retains tile geometry and uses 32-bit global tile coordinates plus native-unit directional metadata:
  * native Z0..Z6 road geometry
  * coastline/water/park/wood contextual geometry
  * zoom-aware road/place/POI labels
  * Z6 road metadata (display name/ref + native forward/backward limits and unit/state flags)
  * exact numeric OSM limits; no untagged road-class legal-limit inference

The outer header/index remain seek-friendly and bounded for the ARMv4T runtime.
All expensive filtering/tiling is performed on the desktop.
"""
import argparse
import hashlib
import json
import math
import os
import re
import shutil
import sqlite3
import struct
import subprocess
import sys
import time
import unicodedata
from pathlib import Path
from typing import Iterable, Iterator

HDR = struct.Struct("<4sHBBIIIIiiii")
IDX = struct.Struct("<BBxxiiIIHH")
MAX_TILE_BYTES = 32768
MAX_FILE_OFFSET = 0x7FFFFFFF
SPANS = [16384, 8192, 4096, 2048, 1024, 512, 256]
MIN_LEVEL = 0
MAX_LEVEL = 6
SIMPLIFY_TOL = [12.0, 10.0, 8.0, 6.0, 4.0, 2.0, 0.75]

# Geometry layers understood by ClioPlay CPM5.
L_ROAD = 0
L_WATER = 1
L_GREEN = 2
L_COAST = 3

# Label kinds understood by ClioPlay CPM5.
LB_ROAD_REF = 1
LB_ROAD_NAME = 2
LB_CITY = 3
LB_TOWN = 4
LB_LOCALITY = 5
LB_FUEL = 6
LB_PARKING = 7
LB_HOSPITAL = 8

# highway -> (style class, first geometry level, first label level)
ROAD = {
    "motorway": (1, 0, 1), "motorway_link": (1, 0, 2),
    "trunk": (2, 0, 1), "trunk_link": (2, 0, 2),
    "primary": (3, 1, 2), "primary_link": (3, 1, 3),
    "secondary": (4, 2, 3), "secondary_link": (4, 2, 4),
    "tertiary": (5, 2, 4), "tertiary_link": (5, 2, 4),
    "residential": (6, 3, 5), "unclassified": (6, 3, 5),
    "living_street": (6, 4, 5), "service": (7, 5, 6),
}

PLACE = {
    "city": (LB_CITY, 0, 2),
    "town": (LB_TOWN, 1, 8),
    "village": (LB_LOCALITY, 3, 20),
    "suburb": (LB_LOCALITY, 4, 28),
    "neighbourhood": (LB_LOCALITY, 5, 36),
}

# Selected driving POIs are emitted at every close/mid-range driving zoom.
# Z0..Z2 remain intentionally uncluttered national/regional views; Z3..Z6 all
# carry the same fuel/hospital/parking POI set so fast driving and freecam can
# identify useful destinations before they are immediately adjacent.
POI = {
    "fuel": (LB_FUEL, 3, 46),
    "hospital": (LB_HOSPITAL, 3, 34),
    "parking": (LB_PARKING, 3, 64),
}

ROAD_VALUES = list(ROAD)
FILTERS = [
    "w/highway=" + ",".join(ROAD_VALUES),
    "w/natural=coastline,water,wood",
    "w/waterway=river,canal,stream",
    "w/leisure=park,nature_reserve",
    "w/landuse=forest",
    "n/place=city,town,village,suburb,neighbourhood",
    "n/amenity=fuel,hospital,parking",
    "w/amenity=fuel,hospital,parking",
    # Physical speed-limit sign nodes are useful fallback evidence when the
    # corresponding way was not fully tagged. OSM recommends maxspeed=* on
    # these nodes, so selecting the speed keys is more precise than importing
    # every traffic_sign=* object in the country.
    "n/maxspeed", "n/maxspeed:type", "n/source:maxspeed", "n/zone:maxspeed",
    # Constituent-country admin boundaries let us apply the UK restricted-road
    # statutory default correctly (Wales differs from England/Scotland/NI).
    "r/admin_level=4",
]

WAY_TAGS = [
    "highway", "name", "ref", "oneway", "junction", "dual_carriageway", "lit",
    "access", "motor_vehicle", "vehicle", "lanes", "motorroad", "expressway",
    "surface", "tracktype", "sidewalk", "sidewalk:both", "sidewalk:left", "sidewalk:right",
    "abutters", "service",
    "maxspeed", "maxspeed:type", "source:maxspeed", "zone:maxspeed",
    "maxspeed:forward", "maxspeed:backward",
    "maxspeed:lanes", "maxspeed:lanes:forward", "maxspeed:lanes:backward",
    "maxspeed:conditional", "maxspeed:variable", "maxspeed:forward:conditional",
    "maxspeed:backward:conditional", "maxspeed:lanes:conditional",
    "maxspeed:lanes:forward:conditional", "maxspeed:lanes:backward:conditional",
    "natural", "waterway", "leisure", "landuse", "amenity"
]
POINT_TAGS = ["place", "amenity", "name"]
SPEED_NODE_TAGS = [
    "traffic_sign", "traffic_sign:direction", "traffic_sign:forward", "traffic_sign:backward",
    "direction", "maxspeed", "maxspeed:type", "source:maxspeed", "zone:maxspeed",
]
ADMIN_TAGS = ["boundary", "admin_level", "name", "name:en", "ISO3166-1", "ISO3166-2"]
ROAD_RANK = {h: ROAD[h][0] for h in ROAD}
LIT_PRESENT_VALUES = {"yes", "24/7", "automatic", "limited"}
STATIC_BLOCK_TOKENS = {"signals", "variable", "none", "walk", "unknown", "unposted"}

# CPM5 metadata: bit 0 estimate, 1 mph, 2 unrestricted, 3 variable,
# 4 block automatic estimates on unsupported/special/direction records. Detailed
# provenance remains in the JSON sidecar, which keeps tile-local metadata
# highly deduplicable in dense areas.
MF_SPEED_WEAK = 0x01

# GraphHopper's current defaults use a 400 m residential radius and sensitivity
# 6000. Its optimized graph density threshold is therefore ~26.7 qualifying
# graph edges in the neighbourhood. We approximate that on OSM ways using a
# 200 m spatial grid and a deliberately conservative 24-way threshold.
URBAN_RADIUS_M = 400.0
URBAN_WAY_THRESHOLD = 24
SPEED_EVIDENCE_REFERENCE = {"OSM":"https://wiki.openstreetmap.org/wiki/Key:maxspeed", "policy":"Native values and reviewed tagged contexts; no untagged road-class inference."}


def eprint(*a, **k): print(*a, file=sys.stderr, flush=True, **k)
def mib(n): return f"{n / 1048576:.2f} MiB"
def gib(n): return f"{n / 1073741824:.2f} GiB"

def file_fingerprint(path: Path) -> dict:
    st = path.stat()
    return {"path": str(path.resolve()), "size": st.st_size, "mtime_ns": st.st_mtime_ns}

def run(cmd, quiet=False):
    eprint("+", " ".join(str(x) for x in cmd))
    kwargs = {}
    if quiet: kwargs.update(stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(cmd, check=True, **kwargs)

def find_osmium():
    p = shutil.which("osmium")
    if not p:
        raise SystemExit("osmium-tool is required.\nmacOS: brew install osmium-tool\nUbuntu/WSL: sudo apt install osmium-tool")
    try: eprint(subprocess.check_output([p, "--version"], text=True, stderr=subprocess.STDOUT).splitlines()[0])
    except Exception: pass
    return p

def check_disk(work: Path, source_size: int):
    work.mkdir(parents=True, exist_ok=True)
    free = shutil.disk_usage(work).free
    eprint(f"Disk free at {work}: {gib(free)}")
    recommended = max(40 * 1024**3, source_size * 16)
    if free < recommended:
        eprint(f"WARNING: graph-section Z0..Z6 build recommends about {gib(recommended)} free; build may run out of disk.")

def floor_div(a, b): return math.floor(a / b)

def clip_seg(x0, y0, x1, y1, xmin, ymin, xmax, ymax):
    dx, dy = x1-x0, y1-y0
    u1, u2 = 0.0, 1.0
    for p, q in [(-dx,x0-xmin),(dx,xmax-x0),(-dy,y0-ymin),(dy,ymax-y0)]:
        if p == 0:
            if q < 0: return None
        else:
            t = q / p
            if p < 0:
                if t > u2: return None
                if t > u1: u1 = t
            else:
                if t < u1: return None
                if t < u2: u2 = t
    return x0+u1*dx, y0+u1*dy, x0+u2*dx, y0+u2*dy

def qlocal(v, base, span):
    q = round((v-base)*4095/span)
    return max(0, min(4095, int(q)))

def point_seg_dist_sq(p,a,b):
    px,py=p; ax,ay=a; bx,by=b; dx=bx-ax; dy=by-ay
    if dx==0 and dy==0: return float((px-ax)**2+(py-ay)**2)
    t=((px-ax)*dx+(py-ay)*dy)/float(dx*dx+dy*dy)
    if t<=0: qx,qy=ax,ay
    elif t>=1: qx,qy=bx,by
    else: qx,qy=ax+t*dx,ay+t*dy
    ex,ey=px-qx,py-qy
    return ex*ex+ey*ey

def simplify_path(points, tol):
    if len(points)<=2 or tol<=0: return points
    keep=[False]*len(points); keep[0]=keep[-1]=True; stack=[(0,len(points)-1)]; tol2=tol*tol
    while stack:
        s,e=stack.pop(); a,b=points[s],points[e]; bi=-1; bd=-1.0
        for i in range(s+1,e):
            d=point_seg_dist_sq(points[i],a,b)
            if d>bd: bi,bd=i,d
        if bi>=0 and bd>tol2:
            keep[bi]=True; stack.append((s,bi)); stack.append((bi,e))
    return [p for i,p in enumerate(points) if keep[i]]

def dedupe_points(points):
    out=[]; last=None
    for p in points:
        if p!=last: out.append(p); last=p
    return out

def split_path(points, max_points=255):
    i=0
    while i < len(points)-1:
        j=min(len(points),i+max_points); c=points[i:j]
        if len(c)>=2: yield c
        if j==len(points): break
        i=j-1

def sanitize_text(s, cap=28):
    if not s:return ""
    s=unicodedata.normalize("NFC",str(s))
    s=" ".join("".join(c for c in s if unicodedata.category(c) not in ("Cc","Cs") and c not in "\u202a\u202b\u202c\u202d\u202e\u2066\u2067\u2068\u2069").split())
    return s[:cap]


def path_len_q(points):
    return sum(abs(b[0]-a[0])+abs(b[1]-a[1]) for a,b in zip(points,points[1:]))

def midpoint_q(points):
    if not points: return (0,0)
    lengths=[abs(b[0]-a[0])+abs(b[1]-a[1]) for a,b in zip(points,points[1:])]
    total=sum(lengths)
    if total<=0: return points[len(points)//2]
    target=total//2; acc=0
    for (a,b),ln in zip(zip(points,points[1:]),lengths):
        if acc+ln>=target and ln:
            t=(target-acc)/ln
            return (round(a[0]+(b[0]-a[0])*t),round(a[1]+(b[1]-a[1])*t))
        acc+=ln
    return points[-1]

def tile_paths(coords, minlvl, simplify=True):
    parsed=[]
    for p in coords:
        try: parsed.append((float(p[0])*100000.0,float(p[1])*100000.0))
        except Exception: pass
    if len(parsed)<2: return
    for lvl,span in enumerate(SPANS):
        if lvl<minlvl: continue
        tile_map={}
        for (x0,y0),(x1,y1) in zip(parsed,parsed[1:]):
            tx0=floor_div(int(math.floor(min(x0,x1))),span); tx1=floor_div(int(math.floor(max(x0,x1))),span)
            ty0=floor_div(int(math.floor(min(y0,y1))),span); ty1=floor_div(int(math.floor(max(y0,y1))),span)
            for ty in range(ty0,ty1+1):
                basey=ty*span
                for tx in range(tx0,tx1+1):
                    basex=tx*span
                    c=clip_seg(x0,y0,x1,y1,basex,basey,basex+span,basey+span)
                    if not c: continue
                    ax,ay,bx,by=c
                    q0=(qlocal(ax,basex,span),qlocal(ay,basey,span)); q1=(qlocal(bx,basex,span),qlocal(by,basey,span))
                    if q0==q1: continue
                    paths=tile_map.setdefault((tx,ty),[])
                    if paths and paths[-1][-1]==q0: paths[-1].append(q1)
                    else: paths.append([q0,q1])
        tol=SIMPLIFY_TOL[lvl] if simplify else 0.0
        for (tx,ty),paths in tile_map.items():
            if not(-100000<=tx<=100000 and -100000<=ty<=100000):
                raise RuntimeError(f"tile coordinate outside global range: Z{lvl} {tx},{ty}")
            for path in paths:
                path=simplify_path(dedupe_points(path),tol)
                if len(path)<2: continue
                for chunk in split_path(path): yield lvl,tx,ty,chunk

def _norm_speed_text(raw):
    return unicodedata.normalize("NFKC", str(raw or "")).strip().lower().replace("\u00a0", " ")










def _oneway_dir(props):
    v=_norm_speed_text(props.get("oneway"))
    if v in ("yes","true","1"):
        return 1
    if v=="-1":
        return -1
    if v in ("no","false","0"):
        return 0
    if _norm_speed_text(props.get("junction")) in ("roundabout","circular"):
        return 1
    return 0













def polyline_length_m(coords):
    total=0.0
    last=None
    for p in coords:
        try: lon,lat=float(p[0]),float(p[1])
        except Exception: continue
        if last is not None:
            lon0,lat0=last
            rlat=math.radians((lat0+lat)*0.5)
            dx=math.radians(lon-lon0)*math.cos(rlat)*6371008.8
            dy=math.radians(lat-lat0)*6371008.8
            total+=math.hypot(dx,dy)
        last=(lon,lat)
    return total








































def open_speed_db(path):
    if path.exists():path.unlink()
    db=sqlite3.connect(path)
    db.execute("pragma journal_mode=off");db.execute("pragma synchronous=off");db.execute("pragma cache_size=-196608")
    db.execute("create table speed_way(seq integer primary key,way_id integer unique not null,speed integer not null,source text not null,weak integer not null,hclass text not null,length_m real not null,reason text not null)")
    return db






def road_meta_flags(source,weak=False):
    # Only the weak bit is persisted in the map. Keeping source/provenance out
    # of the tile-local metadata key maximizes deduplication in dense tiles;
    # full provenance remains in the build sidecar statistics.
    return global_flags(source,weak)




















def speed_coverage_stats(db):
    total,known,total_m,known_m=db.execute("""
      select count(*),sum(case when speed>0 then 1 else 0 end),
             coalesce(sum(length_m),0),coalesce(sum(case when speed>0 then length_m else 0 end),0)
      from speed_way
    """).fetchone()
    by_class={}
    for h,total_n,known_n,strong_n,weak_n,meters,known_meters,strong_meters,weak_meters in db.execute("""
      select hclass,count(*),sum(case when speed>0 then 1 else 0 end),
             sum(case when speed>0 and weak=0 then 1 else 0 end),sum(case when speed>0 and weak=1 then 1 else 0 end),
             sum(length_m),sum(case when speed>0 then length_m else 0 end),
             sum(case when speed>0 and weak=0 then length_m else 0 end),sum(case when speed>0 and weak=1 then length_m else 0 end)
      from speed_way group by hclass order by hclass
    """):
        by_class[h]={"ways":int(total_n),"known_ways":int(known_n or 0),"strong_ways":int(strong_n or 0),"weak_ways":int(weak_n or 0),
                     "km":round(float(meters or 0)/1000.0,3),"known_km":round(float(known_meters or 0)/1000.0,3),
                     "strong_km":round(float(strong_meters or 0)/1000.0,3),"weak_km":round(float(weak_meters or 0)/1000.0,3),
                     "way_pct":round(100.0*float(known_n or 0)/total_n,2) if total_n else 0.0,
                     "km_pct":round(100.0*float(known_meters or 0)/meters,2) if meters else 0.0}
    by_source={}
    for source,n,weak_n,meters in db.execute("select source,count(*),sum(case when weak=1 then 1 else 0 end),sum(length_m) from speed_way where speed>0 group by source order by count(*) desc"):
        by_source[str(source)]={"ways":int(n),"weak_ways":int(weak_n or 0),"km":round(float(meters or 0)/1000.0,3)}
    unknown_reasons={}
    for reason,n,meters in db.execute("select reason,count(*),sum(length_m) from speed_way where speed=0 group by reason order by count(*) desc"):
        unknown_reasons[str(reason or "unknown")]={"ways":int(n),"km":round(float(meters or 0)/1000.0,3)}
    unknown_by_highway={}
    for h,n,meters in db.execute("select hclass,count(*),sum(length_m) from speed_way where speed=0 group by hclass order by count(*) desc"):
        unknown_by_highway[str(h)]={"ways":int(n),"km":round(float(meters or 0)/1000.0,3)}
    strong_ways,weak_ways,strong_m,weak_m=db.execute("""
      select sum(case when speed>0 and weak=0 then 1 else 0 end),sum(case when speed>0 and weak=1 then 1 else 0 end),
             coalesce(sum(case when speed>0 and weak=0 then length_m else 0 end),0),
             coalesce(sum(case when speed>0 and weak=1 then length_m else 0 end),0) from speed_way
    """).fetchone()
    return {
        "total_ways":int(total),"known_ways":int(known or 0),"way_pct":round(100.0*float(known or 0)/total,2) if total else 0.0,
        "strong_ways":int(strong_ways or 0),"weak_ways":int(weak_ways or 0),
        "strong_km":round(float(strong_m or 0)/1000.0,3),"weak_km":round(float(weak_m or 0)/1000.0,3),
        "total_km":round(float(total_m or 0)/1000.0,3),"known_km":round(float(known_m or 0)/1000.0,3),
        "km_pct":round(100.0*float(known_m or 0)/float(total_m or 1),2),
        "by_highway":by_class,"by_source":by_source,"unknown_reasons":unknown_reasons,"unknown_by_highway":unknown_by_highway,
    }




class SpeedResolver:
    """Sequential lookup for the second deterministic osmium way export."""
    def __init__(self,db):
        self.db=db; self.cur=iter(db.execute("select way_id,speed,source,weak from speed_way order by seq")); self.next=None
    def get(self,way_id):
        if self.next is None:
            self.next=next(self.cur,None)
        if self.next is None:
            raise RuntimeError(f"speed preprocessing ended before way {way_id}")
        wid,speed,source,weak=self.next
        if int(wid)!=int(way_id):
            raise RuntimeError(f"osmium way order changed between speed scan and tile pass: expected {wid}, got {way_id}")
        self.next=None
        return int(speed),str(source),int(weak)


def road_context_text(name, ref):
    name=sanitize_text(name,28); ref=sanitize_text(ref,12)
    if name and ref and ref not in name:
        # Give the driver the route number and the human road name where both fit.
        return sanitize_text(ref+" "+name,28)
    return name or ref

def road_meta_blob(name, ref, speed_mph, meta_flags=0):
    txt=road_context_text(name,ref).encode("utf-8")[:112]
    # CPM5: forward value/flags, backward value/flags, text_length, text.
    # Bit 0 means display the limit with
    # a question mark because it is estimated/weak rather than direct evidence.
    return global_meta(name,ref,speed_mph,meta_flags,txt)

def geom_blob(layer, style, points):
    b=bytearray((layer&255,style&255,len(points)&255,0))
    for x,y in points: b.extend(struct.pack("<HH",x,y))
    return bytes(b)

def label_blob(kind, priority, x, y, text):
    raw=sanitize_text(text).encode("utf-8")[:112]
    if not raw: return None
    return bytes((kind&255,priority&255))+struct.pack("<HH",x,y)+bytes((len(raw),0))+raw

def road_geom_priority(cls): return {1:5,2:7,3:12,4:20,5:28,6:42,7:58}.get(cls,70)
def road_draworder(cls): return 200-cls  # lower/local roads first after reverse sort at pack time

def context_spec(props):
    n=props.get("natural")
    w=props.get("waterway")
    l=props.get("leisure")
    lu=props.get("landuse")
    if n=="coastline": return (L_COAST,1,0,15,10)
    if w=="river": return (L_WATER,1,2,30,20)
    if w=="canal": return (L_WATER,2,3,38,20)
    if w=="stream": return (L_WATER,3,5,60,20)
    if n=="water": return (L_WATER,4,3,44,18)
    if l=="nature_reserve": return (L_GREEN,3,3,62,12)
    if l=="park": return (L_GREEN,1,4,68,12)
    if n=="wood" or lu=="forest": return (L_GREEN,2,4,70,12)
    return None

def road_label_for_level(name, ref, cls, lvl, label_min):
    if lvl < label_min: return None
    # Wide views favour compact route refs. Close views favour street names.
    if lvl <= 4 and ref: return LB_ROAD_REF, sanitize_text(ref,16), 10+cls*3
    if lvl >= 5 and name: return LB_ROAD_NAME, sanitize_text(name,28), 24+cls*4
    if ref: return LB_ROAD_REF, sanitize_text(ref,16), 12+cls*3
    if name and cls<=5: return LB_ROAD_NAME, sanitize_text(name,28), 28+cls*4
    return None


def open_db(path):
    if path.exists(): path.unlink()
    db=sqlite3.connect(path)
    db.execute("pragma journal_mode=off"); db.execute("pragma synchronous=off"); db.execute("pragma temp_store=file")
    db.execute("pragma locking_mode=exclusive"); db.execute("pragma cache_size=-131072")
    try: db.execute("pragma mmap_size=268435456")
    except sqlite3.DatabaseError: pass
    db.execute("create table rec(level integer,tx integer,ty integer,rectype integer,priority integer,draworder integer,blob blob,meta blob)")
    return db

def insert_batch(db,batch):
    if batch:
        db.executemany("insert into rec(level,tx,ty,rectype,priority,draworder,blob,meta) values(?,?,?,?,?,?,?,?)",batch)
        batch.clear()

def iter_lines(obj):
    g=obj.get("geometry") or {}; typ=g.get("type"); c=g.get("coordinates") or []
    if typ=="LineString": yield c
    elif typ=="MultiLineString": yield from c

def update_bounds(bounds, coords):
    for p in coords:
        try: lon,lat=float(p[0]),float(p[1])
        except Exception: continue
        la,lo=int(round(lat*1e5)),int(round(lon*1e5))
        bounds[0]=min(bounds[0],la); bounds[1]=min(bounds[1],lo); bounds[2]=max(bounds[2],la); bounds[3]=max(bounds[3],lo)

def poi_point_records(batch, lat, lon, amenity, name, stats):
    spec=POI.get(amenity)
    if not spec: return
    kind,minlvl,prio=spec; txt=sanitize_text(name or "")
    if amenity=="parking" and not txt: return
    if not txt: txt={"fuel":"FUEL","hospital":"HOSPITAL","parking":"PARKING"}[amenity]
    for lvl in range(minlvl,MAX_LEVEL+1):
        tx,ty,x,y=point_tile(lat,lon,lvl)
        lb=label_blob(kind,prio,x,y,txt)
        if lb:
            batch.append((lvl,tx,ty,1,prio,0,lb,None)); stats["label_rows"]+=1

def parse_way_stream(lines,db,simplify=True,speed_resolver=None):
    batch=[]; stats={"ways":0,"road_ways":0,"context_ways":0,"poi_ways":0,"geom_rows":0,"label_rows":0,"road_meta_rows":0,"speed_known_ways":0,"speed_strong_ways":0,"speed_weak_ways":0,"speed_unknown_ways":0,"speed_sources":{},"bad":0}; bounds=[10**9,10**9,-10**9,-10**9]
    started=time.monotonic(); last=started
    for line in lines:
        line=line.strip("\x1e\r\n ")
        if not line: continue
        try: obj=json.loads(line)
        except json.JSONDecodeError: stats["bad"]+=1; continue
        props=obj.get("properties") or {}; h=str(props.get("highway") or ""); am=str(props.get("amenity") or "")
        rs=ROAD.get(h); cs=context_spec(props); poispec=POI.get(am)
        if not rs and not cs and not poispec: continue
        stats["ways"]+=1
        speed=0; speed_source="unknown"; speed_weak=0
        if rs:
            stats["road_ways"]+=1
            if speed_resolver is not None:
                try: wid=int(props.get("@id"))
                except Exception: raise RuntimeError("tile pass is missing @id on a road way")
                speed,speed_source,speed_weak=speed_resolver.get(wid)
            else:
                speed,speed_source=global_speed_detail(props); speed_weak=0
            if speed:
                stats["speed_known_ways"]+=1
                if speed_weak: stats["speed_weak_ways"]+=1
                else: stats["speed_strong_ways"]+=1
                stats["speed_sources"][speed_source]=stats["speed_sources"].get(speed_source,0)+1
            else:
                stats["speed_unknown_ways"]+=1
        elif cs: stats["context_ways"]+=1
        else: stats["poi_ways"]+=1
        for coords in iter_lines(obj):
            if len(coords)<2: continue
            update_bounds(bounds,coords)
            if rs:
                cls,minlvl,labelmin=rs; name=props.get("name") or ""; ref=props.get("ref") or ""
                meta=road_meta_blob(name,ref,speed,road_meta_flags(speed_source,speed_weak))
                for lvl,tx,ty,path in tile_paths(coords,minlvl,simplify):
                    gb=geom_blob(L_ROAD,cls,path)
                    m=meta if lvl==6 else None
                    batch.append((lvl,tx,ty,0,road_geom_priority(cls),road_draworder(cls),gb,m)); stats["geom_rows"]+=1
                    if m: stats["road_meta_rows"]+=1
                    lab=road_label_for_level(name,ref,cls,lvl,labelmin)
                    if lab:
                        kind,txt,prio=lab
                        # Only label segments that are visibly long enough for the text.
                        need=(len(txt)*8+20)*16
                        if txt and path_len_q(path)>=need:
                            x,y=midpoint_q(path); lb=label_blob(kind,prio,x,y,txt)
                            if lb:
                                batch.append((lvl,tx,ty,1,prio,0,lb,None)); stats["label_rows"]+=1
            elif cs:
                layer,style,minlvl,prio,draw=cs
                for lvl,tx,ty,path in tile_paths(coords,minlvl,simplify):
                    batch.append((lvl,tx,ty,0,prio,draw,geom_blob(layer,style,path),None)); stats["geom_rows"]+=1
            elif poispec:
                # Fuel/hospital/parking may be mapped as closed ways. Use their
                # geometry centroid as a lightweight POI anchor.
                pts=[]
                for q in coords:
                    try: pts.append((float(q[0]),float(q[1])))
                    except Exception: pass
                if pts:
                    lon=sum(q[0] for q in pts)/len(pts); lat=sum(q[1] for q in pts)/len(pts)
                    before=stats["label_rows"]; poi_point_records(batch,lat,lon,am,props.get("name") or "",stats)
                    if stats["label_rows"]>before: stats["poi_ways"]+=0
            if len(batch)>=25000: insert_batch(db,batch)
        now=time.monotonic()
        if now-last>=10:
            eprint(f"  ways={stats['ways']:,} road={stats['road_ways']:,} context={stats['context_ways']:,} poiways={stats['poi_ways']:,} geom={stats['geom_rows']:,} labels={stats['label_rows']:,} meta={stats['road_meta_rows']:,} speed-known={stats['speed_known_ways']:,} speed-unknown={stats['speed_unknown_ways']:,}")
            last=now
    insert_batch(db,batch); db.commit()
    if bounds[0]==10**9: bounds=[0,0,0,0]
    return stats,bounds

def point_tile(lat,lon,lvl):
    span=SPANS[lvl]; x=float(lon)*1e5; y=float(lat)*1e5
    tx=floor_div(int(math.floor(x)),span); ty=floor_div(int(math.floor(y)),span)
    return tx,ty,qlocal(x,tx*span,span),qlocal(y,ty*span,span)

def parse_point_stream(lines,db,bounds):
    batch=[]; stats={"points":0,"places":0,"pois":0,"labels":0,"bad":0}
    for line in lines:
        line=line.strip("\x1e\r\n ")
        if not line: continue
        try: obj=json.loads(line)
        except json.JSONDecodeError: stats["bad"]+=1; continue
        g=obj.get("geometry") or {}
        if g.get("type")!="Point": continue
        c=g.get("coordinates") or []
        if len(c)<2: continue
        try: lon,lat=float(c[0]),float(c[1])
        except Exception: continue
        props=obj.get("properties") or {}; name=sanitize_text(props.get("name") or "")
        spec=None; txt=name
        pl=str(props.get("place") or "")
        am=str(props.get("amenity") or "")
        if pl in PLACE and name:
            kind,minlvl,prio=PLACE[pl]; spec=(kind,minlvl,prio); stats["places"]+=1
        elif am in POI:
            kind,minlvl,prio=POI[am]
            if am=="parking" and not name: continue
            if not txt: txt={"fuel":"FUEL","hospital":"HOSPITAL","parking":"PARKING"}[am]
            spec=(kind,minlvl,prio); stats["pois"]+=1
        if not spec: continue
        stats["points"]+=1
        la,lo=int(round(lat*1e5)),int(round(lon*1e5))
        bounds[0]=min(bounds[0],la); bounds[1]=min(bounds[1],lo); bounds[2]=max(bounds[2],la); bounds[3]=max(bounds[3],lo)
        kind,minlvl,prio=spec
        for lvl in range(minlvl,MAX_LEVEL+1):
            tx,ty,x,y=point_tile(lat,lon,lvl)
            lb=label_blob(kind,prio,x,y,txt)
            if lb:
                batch.append((lvl,tx,ty,1,prio,0,lb,None)); stats["labels"]+=1
                if len(batch)>=25000: insert_batch(db,batch)
    insert_batch(db,batch); db.commit(); return stats

def emit_final_map(db,out,bounds,flags=0):
    eprint("Creating disk-backed tile index...")
    db.execute("create index reckey on rec(level,tx,ty,priority,rectype)"); db.commit()
    data_tmp=out.with_suffix(out.suffix+".data.tmp"); index_tmp=out.with_suffix(out.suffix+".index.tmp"); final_tmp=out.with_suffix(out.suffix+".tmp")
    for p in (data_tmp,index_tmp,final_tmp): p.unlink(missing_ok=True)
    count=0; datarel=0; max_tile=0; kept_geom=kept_labels=kept_meta=dropped=compact_speed_meta=0; by=[0]*7
    key0=None; rows=[]
    def write_tile(df,ix,key,rows):
        nonlocal count,datarel,max_tile,kept_geom,kept_labels,kept_meta,dropped,compact_speed_meta
        if key is None: return
        lvl,tx,ty=key
        # Deduplicate text labels by kind+text inside the tile. POI icons remain
        # independently positioned at runtime; this only suppresses duplicate names.
        seen=set(); cleaned=[]
        for rectype,prio,draw,blob,meta in rows:
            if rectype==1 and blob[0]<6:
                k=(blob[0],blob[8:])
                if k in seen: continue
                seen.add(k)
            cleaned.append((rectype,prio,draw,blob,meta))
        # CPM5 tile: ng,nl,nmeta,reserved followed by geometry, labels, metadata.
        # Geometry keeps a one-byte tile-local metadata index. Metadata is optional:
        # if a dense tile cannot fit metadata we preserve the road geometry first.
        kept=[]; used=8; metas=[]; meta_index={}
        for r in sorted(cleaned,key=lambda x:(x[1],x[0])):
            rectype,prio,draw,blob,meta=r; extra=len(blob)
            # Geometry/label always has priority over metadata. Do this check
            # first so we never allocate an orphan metadata entry for a record
            # that is later dropped.
            if used+extra>MAX_TILE_BYTES:
                dropped+=1; continue
            mi=255
            if rectype==0 and meta:
                keym=bytes(meta)
                if keym in meta_index:
                    mi=meta_index[keym]
                elif len(metas)<255 and used+extra+len(keym)<=MAX_TILE_BYTES:
                    mi=len(metas); meta_index[keym]=mi; metas.append(keym); used+=len(keym)
                else:
                    # CPM5 speed-preservation fallback: if the full road text
                    # metadata will not fit, try a compact mph+flags+tlen=0
                    # record before dropping metadata altogether. This keeps the
                    # speed limit / '?' available in dense city tiles.
                    compact=None
                    if len(keym)>=5 and (keym[0]>0 or keym[2]>0 or keym[1]&28 or keym[3]&28):
                        compact=keym[:4]+bytes((0,))
                    if compact is not None:
                        if compact in meta_index:
                            mi=meta_index[compact]
                        elif len(metas)<255 and used+extra+len(compact)<=MAX_TILE_BYTES:
                            mi=len(metas); meta_index[compact]=mi; metas.append(compact); used+=len(compact); compact_speed_meta+=1
            kept.append((rectype,prio,draw,blob,mi)); used+=extra
        geoms=[r for r in kept if r[0]==0]; labels=[r for r in kept if r[0]==1]
        geoms.sort(key=lambda r:(r[2],r[1])); labels.sort(key=lambda r:r[1])
        tile=bytearray(struct.pack("<HHHH",len(geoms),len(labels),len(metas),0))
        for r in geoms:
            b=bytearray(r[3]); b[3]=r[4] if r[4] is not None else 255; tile.extend(b)
        for r in labels: tile.extend(r[3])
        for m in metas: tile.extend(m)
        if len(tile)>MAX_TILE_BYTES: raise RuntimeError(f"internal CPM5 cap failure Z{lvl} {tx},{ty}: {len(tile)}")
        if datarel+len(tile)>MAX_FILE_OFFSET: raise RuntimeError("CPM5 data exceeded device 2 GiB seek limit; choose a smaller regional extract")
        df.write(tile); ix.write(IDX.pack(lvl,0,tx,ty,datarel,len(tile),len(geoms)+len(labels)+len(metas),0))
        datarel+=len(tile); max_tile=max(max_tile,len(tile)); kept_geom+=len(geoms); kept_labels+=len(labels); kept_meta+=len(metas); by[lvl]+=1; count+=1
        if count%25000==0: eprint(f"  packed {count:,} tiles / {mib(datarel)} data")
    with data_tmp.open("wb") as df,index_tmp.open("wb") as ix:
        cur=db.execute("select level,tx,ty,rectype,priority,draworder,blob,meta from rec order by level,tx,ty,priority,rectype")
        for lvl,tx,ty,rectype,prio,draw,blob,meta in cur:
            key=(lvl,tx,ty)
            if key0 is None: key0=key
            if key!=key0:
                write_tile(df,ix,key0,rows); key0=key; rows=[]
            rows.append((rectype,prio,draw,blob,meta))
        write_tile(df,ix,key0,rows)
    index_off=HDR.size; data_off=index_off+count*IDX.size
    if data_off+datarel>MAX_FILE_OFFSET: raise RuntimeError("CPM5 file exceeded device 2 GiB seek limit; choose a smaller regional extract")
    out.parent.mkdir(parents=True,exist_ok=True)
    with final_tmp.open("wb") as f:
        f.write(HDR.pack(b"CPM5",5,MIN_LEVEL,MAX_LEVEL,flags,count,index_off,data_off,*bounds))
        with index_tmp.open("rb") as sf: shutil.copyfileobj(sf,f,4*1024*1024)
        with data_tmp.open("rb") as sf: shutil.copyfileobj(sf,f,4*1024*1024)
        f.flush(); os.fsync(f.fileno())
    os.replace(final_tmp,out); data_tmp.unlink(missing_ok=True); index_tmp.unlink(missing_ok=True)
    return {"tiles":count,"kept_geometry":kept_geom,"kept_labels":kept_labels,"kept_road_meta":kept_meta,"compact_speed_meta":compact_speed_meta,"dropped_records":dropped,"max_tile":max_tile,"by_level":by,"size":out.stat().st_size}

def sha256_file(path):
    h=hashlib.sha256()
    with path.open("rb") as f:
        for b in iter(lambda:f.read(4*1024*1024),b""): h.update(b)
    return h.hexdigest()

def manifest_matches(path,wanted):
    try: return json.loads(path.read_text("utf-8"))==wanted
    except Exception: return False

def prepare_filtered_pbf(osmium,pbf,work,force=False):
    out=work/"clio-map.osm.pbf"; manifest=work/"clio-map.manifest.json"
    wanted={"source":file_fingerprint(pbf),"filters":FILTERS,"builder":"driver-context-v9-legal-defaults"}
    if out.exists() and manifest_matches(manifest,wanted) and not force:
        eprint(f"Reusing filtered PBF: {out} ({mib(out.stat().st_size)})"); return out
    eprint("Stage 1/7: filtering roads + speed evidence + jurisdiction boundaries + places + selected POIs...")
    tmp=work/"clio-map.new.osm.pbf"; tmp.unlink(missing_ok=True)
    run([osmium,"tags-filter",str(pbf),*FILTERS,"-t","-o",str(tmp),"--overwrite","--progress"])
    os.replace(tmp,out); manifest.write_text(json.dumps(wanted,indent=2),encoding="utf-8")
    eprint(f"Filtered OSM: {mib(out.stat().st_size)}"); return out

def make_export_config(path,tags,attributes=None,linear_tags=True,area_tags=False):
    attrs=attributes or {}
    path.write_text(json.dumps({"attributes":attrs,"format_options":{},"linear_tags":linear_tags,"area_tags":area_tags,"include_tags":tags},separators=(",",":")),encoding="utf-8")

def export_stream(osmium,pbf,work,kind):
    linear_tags=True; area_tags=False
    if kind=="ways_scan":
        tags=WAY_TAGS; geom="linestring"; attrs={"id":True,"way_nodes":True}
    elif kind=="ways":
        # The second pass only needs the stable way id; omitting @way_nodes here
        # saves a large amount of JSON parsing and pipe traffic on a large regional build.
        tags=WAY_TAGS; geom="linestring"; attrs={"id":True}
    elif kind=="speednodes":
        tags=SPEED_NODE_TAGS; geom="point"; attrs={"id":True}
    elif kind=="jurisdictions":
        tags=ADMIN_TAGS; geom="polygon"; attrs={"id":True}; linear_tags=False; area_tags=["boundary=administrative"]
    else:
        tags=POINT_TAGS; geom="point"; attrs={"id":True}
    cfg=work/f"export-{kind}.json"; make_export_config(cfg,tags,attrs,linear_tags,area_tags)
    cmd=[osmium,"export",str(pbf),"-f","geojsonseq","--geometry-types="+geom,"-c",str(cfg),"-x","print_record_separator=false","--progress"]
    node_index=None
    if kind in ("ways","ways_scan","jurisdictions"):
        node_index=work/("jurisdiction-node-index.dat" if kind=="jurisdictions" else "node-index.dat")
        node_index.unlink(missing_ok=True); cmd.extend(["-i",f"sparse_file_array,{node_index}"])
    eprint("+"," ".join(cmd),"> [ClioPlay converter]")
    proc=subprocess.Popen(cmd,stdout=subprocess.PIPE,text=True,encoding="utf-8",errors="replace",bufsize=1024*1024)
    if proc.stdout is None: raise RuntimeError("failed to open osmium stdout")
    return proc,proc.stdout,node_index

def wait_proc(proc,stream):
    stream.close(); rc=proc.wait()
    if rc: raise SystemExit(f"osmium export failed with exit code {rc}")

def make_demo_way_lines():
    lat,lon=54.982,-1.65638
    f=[]
    def add(props,pts): f.append({"type":"Feature","properties":props,"geometry":{"type":"LineString","coordinates":pts}})
    add({"highway":"primary","ref":"A1","name":"GREAT NORTH ROAD","maxspeed":"40 mph"},[(lon-.04,lat-.01),(lon,lat),(lon+.04,lat+.01)])
    add({"highway":"secondary","name":"PONTELAND ROAD","maxspeed":"30 mph"},[(lon-.02,lat+.025),(lon,lat),(lon+.02,lat-.025)])
    for k in range(-4,5):
        o=k*.0035; add({"highway":"residential","name":f"TEST STREET {k+5}","maxspeed":"20 mph"},[(lon-.016,lat+o),(lon+.016,lat+o)])
    add({"waterway":"river","name":"RIVER TYNE"},[(lon-.05,lat-.025),(lon-.01,lat-.02),(lon+.05,lat-.018)])
    add({"leisure":"park","name":"TEST PARK"},[(lon+.006,lat+.006),(lon+.014,lat+.006),(lon+.014,lat+.014),(lon+.006,lat+.014),(lon+.006,lat+.006)])
    for o in f: yield json.dumps(o,separators=(",",":"))+"\n"

def make_demo_point_lines():
    lat,lon=54.982,-1.65638
    f=[
      {"type":"Feature","properties":{"place":"city","name":"NEWCASTLE"},"geometry":{"type":"Point","coordinates":[lon+.01,lat+.01]}},
      {"type":"Feature","properties":{"amenity":"fuel","name":"FUEL"},"geometry":{"type":"Point","coordinates":[lon-.002,lat+.002]}},
      {"type":"Feature","properties":{"amenity":"hospital","name":"RVI"},"geometry":{"type":"Point","coordinates":[lon+.004,lat-.003]}},
    ]
    for o in f: yield json.dumps(o,separators=(",",":"))+"\n"

def build_streams(way_lines,point_lines,out,work,source=None,demo=False,simplify=True):
    dbp=work/"map.sqlite"; db=open_db(dbp)
    try:
        eprint("Stage 2/4: tiling road/context geometry + road labels...")
        ws,bounds=parse_way_stream(way_lines,db,simplify)
        eprint("Stage 3/4: tiling place/POI labels...")
        ps=parse_point_stream(point_lines,db,bounds)
        eprint("Stage 4/4: packing CPM5...")
        stats=emit_final_map(db,out,bounds,flags=1 if demo else 0)
    finally: db.close()
    info={"format":"CPM5","version":5,"builder":"global-1","source":file_fingerprint(source) if source else None,"way_stats":ws,"point_stats":ps,"bounds_e5":bounds,**stats,"sha256":sha256_file(out)}
    out.with_suffix(out.suffix+".json").write_text(json.dumps(info,indent=2),encoding="utf-8")
    eprint("DONE")
    eprint(f"  {out}: {mib(out.stat().st_size)}")
    eprint(f"  tiles={stats['tiles']:,} geometry={stats['kept_geometry']:,} labels={stats['kept_labels']:,} road_meta={stats['kept_road_meta']:,} dropped={stats['dropped_records']:,}")
    total_speed=ws.get("speed_known_ways",0)+ws.get("speed_unknown_ways",0)
    if total_speed:
        pct=100.0*ws.get("speed_known_ways",0)/total_speed
        eprint(f"  speed coverage: {ws.get('speed_known_ways',0):,}/{total_speed:,} road ways ({pct:.1f}%) have trustworthy limits")
        if ws.get("speed_sources"):
            eprint("  speed sources: "+", ".join(f"{k}={v:,}" for k,v in sorted(ws["speed_sources"].items())))
    eprint(f"  max tile={stats['max_tile']:,}/{MAX_TILE_BYTES:,} bytes")
    eprint("  tiles by level: "+", ".join(f"Z{i}={n:,}" for i,n in enumerate(stats['by_level'])))
    eprint(f"  SHA-256={info['sha256']}")
    return info

def main():
    ap=argparse.ArgumentParser(description='Build complete CPM5 maps, including water, greenery, sea and estuary fills.')
    ap.add_argument("--pbf",type=Path); ap.add_argument("--demo",action="store_true"); ap.add_argument("--out",type=Path,required=True); ap.add_argument("--work",type=Path,default=Path("mapwork-global")); ap.add_argument("--force-filter",action="store_true"); ap.add_argument("--no-simplify",action="store_true"); ap.add_argument("--cleanup",action="store_true")
    ap.add_argument('--cache',type=Path,default=Path(__file__).resolve().parents[1]/'.cache',help='automatic coastline download cache')
    ap.add_argument('--speed-import',type=Path,help='source-attributed missing-limit corrections keyed to OSM way IDs')
    ap.add_argument('--base-only',action='store_true',help=argparse.SUPPRESS)
    a=ap.parse_args(); global_self_test(); a.out=a.out.expanduser().resolve(); a.work=a.work.expanduser().resolve(); a.work.mkdir(parents=True,exist_ok=True)
    if a.demo:
        build_streams(make_demo_way_lines(),make_demo_point_lines(),a.out,a.work,demo=True,simplify=not a.no_simplify); return
    if not a.pbf: raise SystemExit("--pbf /path/to/region-latest.osm.pbf is required")
    pbf=a.pbf.expanduser().resolve()
    if not pbf.is_file(): raise SystemExit(f"PBF not found: {pbf}")
    target=a.out
    if not a.base_only:
        try:
            import shapely, shapefile
        except ImportError:
            raise SystemExit('Run build_navigation.command to install the Python dependencies automatically.')
        a.out=a.work/'base-map.cpm'
        if target in (a.out,a.work/'inland-map.cpm'):
            raise SystemExit('output must differ from internal work maps')
    osmium=find_osmium(); check_disk(a.work,pbf.stat().st_size); eprint(f"Source: {pbf} ({gib(pbf.stat().st_size)})")
    run([osmium,"fileinfo","-e",str(pbf)],quiet=True)
    filtered=prepare_filtered_pbf(osmium,pbf,a.work,a.force_filter)

    from speed_import import SpeedImport
    corrections=SpeedImport(a.speed_import) if a.speed_import else None
    speeddbp=a.work/"speed.sqlite"; sdb=open_speed_db(speeddbp)
    dbp=a.work/"map.sqlite"; db=None; ok=False; nodeidx=None; jnodeidx=None
    try:
        jurisdiction_stats={};sign_stats={};topology_stats={};legal_estimate_stats={}
        eprint("Resolving native-unit, directional and conditional speed evidence...")
        wa,was,nodeidx=export_stream(osmium,filtered,a.work,"ways_scan")
        speed_scan=global_speed_scan(was,sdb,corrections); wait_proc(wa,was)
        cov=speed_coverage_stats(sdb)
        total=cov["total_ways"]; known=cov["known_ways"]; km_pct=cov.get("km_pct",0.0)
        eprint(f"  preprocessed speed coverage={known:,}/{total:,} ({(100.0*known/total if total else 0.0):.1f}% ways; {km_pct:.1f}% road-km)")
        eprint(f"    strong={cov.get('strong_ways',0):,} ways / {cov.get('strong_km',0.0):,.1f} km; weak(?)={cov.get('weak_ways',0):,} ways / {cov.get('weak_km',0.0):,.1f} km")
        for h in ("motorway","trunk","primary","secondary","tertiary","residential"):
            c=cov.get("by_highway",{}).get(h)
            if c: eprint(f"    {h}: {c['way_pct']:.1f}% ways / {c['km_pct']:.1f}% km")

        db=open_db(dbp)
        eprint("Stage 5/7: streaming geometry with resolved speed metadata...")
        wp,ws,nodeidx=export_stream(osmium,filtered,a.work,"ways")
        resolver=SpeedResolver(sdb)
        wstats,bounds=parse_way_stream(ws,db,not a.no_simplify,resolver); wait_proc(wp,ws)

        pp,ps,_=export_stream(osmium,filtered,a.work,"points")
        eprint("Stage 6/7: streaming place/POI nodes...")
        pstats=parse_point_stream(ps,db,bounds); wait_proc(pp,ps)

        eprint("Stage 7/7: packing CPM5...")
        stats=emit_final_map(db,a.out,bounds,flags=0); ok=True
    except BaseException:
        for proc in (locals().get("jp"),locals().get("sp"),locals().get("wa"),locals().get("wp"),locals().get("pp")):
            if proc and proc.poll() is None:
                try: proc.terminate()
                except Exception: pass
        raise
    finally:
        if db is not None: db.close()
        sdb.close()

    speed_preprocess={"speed_import":corrections.report() if corrections else None,"legal_defaults_reference":SPEED_EVIDENCE_REFERENCE,"jurisdictions":jurisdiction_stats,"sign_nodes":sign_stats,"scan":speed_scan,"topology":topology_stats,"legal_defaults":legal_estimate_stats,"final_coverage":cov}
    info={"format":"CPM5","version":5,"builder":"global-1","source":file_fingerprint(pbf),"speed_preprocess":speed_preprocess,"way_stats":wstats,"point_stats":pstats,"bounds_e5":bounds,**stats,"sha256":sha256_file(a.out)}
    a.out.with_suffix(a.out.suffix+".json").write_text(json.dumps(info,indent=2),encoding="utf-8")
    eprint("DONE"); eprint(f"  map={mib(a.out.stat().st_size)} tiles={stats['tiles']:,} geometry={stats['kept_geometry']:,} labels={stats['kept_labels']:,} road_meta={stats['kept_road_meta']:,} dropped={stats['dropped_records']:,}")
    total_speed=wstats.get("speed_known_ways",0)+wstats.get("speed_unknown_ways",0)
    if total_speed:
        pct=100.0*wstats.get("speed_known_ways",0)/total_speed
        eprint(f"  speed coverage: {wstats.get('speed_known_ways',0):,}/{total_speed:,} road ways ({pct:.1f}%)")
        if wstats.get("speed_sources"):
            eprint("  speed sources: "+", ".join(f"{k}={v:,}" for k,v in sorted(wstats["speed_sources"].items())))
    eprint(f"  max tile={stats['max_tile']:,}/{MAX_TILE_BYTES:,}"); eprint("  "+", ".join(f"Z{i}={n:,}" for i,n in enumerate(stats['by_level']))); eprint(f"  SHA-256={info['sha256']}")
    if ok and a.cleanup:
        for p in (dbp,speeddbp,nodeidx,jnodeidx):
            if p: Path(p).unlink(missing_ok=True)
        eprint("Cleaned SQLite/node-index scratch. Filtered OSM retained for fast rebuilds.")
    if not a.base_only:
        inland=a.work/'inland-map.cpm'
        subprocess.run([sys.executable,str(Path(__file__).with_name('rebuild_areas.py')),
                        '--base',str(a.out),'--pbf',str(pbf),'--out',str(inland),
                        '--work',str(a.work/'areas')],check=True)
        subprocess.run([sys.executable,str(Path(__file__).with_name('build_ocean.py')),
                        '--base',str(inland),'--out',str(target),'--work',str(a.work/'ocean'),
                        '--cache',str(a.cache.expanduser().resolve())],check=True)
        eprint(f'Complete map ready: {target}')

from global_speeds import (global_flags,global_meta,global_self_test,global_speed_scan,global_speed_detail)

if __name__=="__main__": main()
