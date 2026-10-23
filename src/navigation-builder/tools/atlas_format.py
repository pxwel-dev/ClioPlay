"""CPA1 region/portal overlay. All offsets and per-file seeks fit signed 31 bits."""
import array,math,struct
from collections import Counter
from pathlib import Path
from inspect_route import CPR2,CHARC,ARC_SHORTCUT
HDR=struct.Struct('<4sHH14I');REG=struct.Struct('<24s24s4i4I');NODE=struct.Struct('<ii4I');EDGE=struct.Struct('<6I');MEM=struct.Struct('<4I');CPN=struct.Struct('<QIiiI')
def candidates(path):
 with Path(path).open('rb') as f:
  magic,v,size,n,off=struct.unpack('<4sHHII',f.read(16))
  if (magic,v,size,off)!=(b'CPN1',1,24,16):raise ValueError('Invalid CPN1')
  for _ in range(n):yield CPN.unpack(f.read(24))
def metres(pts):
 total=0
 for (a,b),(c,d) in zip(pts,pts[1:]):
  x=abs(d-b)*math.cos(math.radians((a+c)/2e5));y=abs(c-a);hi,lo=max(x,y),min(x,y);total+=round((hi+lo*.375)*1.1132)
 return total

def write(output,regions):
 # Nodes are keyed by original OSM identity, never proximity or street name.
 nodes={};members=[];selected=[]
 for region,meta in enumerate(regions):
  local={};r=CPR2(output/meta['path']/'route.cpr');items=list(candidates(output/meta['path']/'route.cpn'))
  if items:
   floor=r.n-len(items)
   if min(r.node(n)[6] for osm,n,la,lo,comp in items)<floor:raise ValueError('Portal nodes were not contracted last')
  for osm,n,la,lo,comp in items:
   if osm not in nodes:nodes[osm]=(len(nodes),la,lo)
   g,x,y=nodes[osm]
   if (x,y)!=(la,lo):raise ValueError('Inconsistent coordinates for shared OSM node')
   local[n]=g;members.append((region,g,n,0))
  selected.append(local);r.f.close()
 if len(nodes)>65534:raise ValueError('Atlas exceeds the 65,534 portal-state memory limit; use smaller installed coverage')
 adjacency=[[] for _ in nodes]
 for region,meta in enumerate(regions):
  r=CPR2(output/meta['path']/'route.cpr');local=selected[region];lengths=array.array('I');base_lengths={}
  # CH children precede parents; compute a metric length once per arc.
  r.f.seek(r.ao)
  for aid in range(r.na):
   # Geometry/base reads change the file position: explicitly seek each record.
   a=r.arc(aid)
   if a[6]&ARC_SHORTCUT:
    if a[3]>=aid or a[4]>=aid:raise ValueError('Invalid CH child order')
    m=min(0xffffffff,lengths[a[3]]+lengths[a[4]])
   else:
    if a[3] not in base_lengths:base_lengths[a[3]]=metres(r.geom_base(a[3]))
    m=base_lengths[a[3]]
   lengths.append(m)
   if a[0] in local and a[1] in local and a[0]!=a[1]:adjacency[local[a[0]]].append((local[a[1]],a[2],m,region,aid,0))
  r.f.close()
 member_node=sorted(members,key=lambda m:(m[1],m[0]));members.sort();edges=[];nr=[];rn=[];nm=[]
 for r,meta in enumerate(regions):
  group=[m for m in members if m[0]==r];first=len(nm);nm.extend(group)
  rn.append((meta['path'].encode(),meta['name'].encode()[:23],*[round(v*1e5) for v in meta['core']],first,len(group),0,0))
 coords={g:(la,lo) for g,la,lo in nodes.values()};mf=0;counts=Counter(m[1] for m in member_node)
 for g in range(len(nodes)):
  # Parallel alternatives within a region retain the cheapest hierarchy arc.
  best={}
  for e in adjacency[g]:
   k=(e[0],e[3])
   if k not in best or e[1]<best[k][1]:best[k]=e
  group=sorted(best.values());first=len(edges);edges.extend(group)
  count=counts[g];nr.append((*coords[g],first,len(group),mf,count));mf+=count
 ro=HDR.size;no=ro+REG.size*len(rn);eo=no+NODE.size*len(nr);mo=eo+EDGE.size*len(edges);size=mo+2*MEM.size*len(members)
 if size>0x7fffffff:raise ValueError('Atlas catalog too large')
 path=output/'atlas.cpa'
 with path.open('wb') as f:
  f.write(HDR.pack(b'CPA1',1,REG.size,len(rn),len(nr),len(edges),len(members),ro,no,eo,mo,size,0,max((r[7] for r in rn),default=0),0,0,0))
  for rows,fmt in ((rn,REG),(nr,NODE),(edges,EDGE),(members,MEM),(member_node,MEM)):
   for row in rows:f.write(fmt.pack(*row))
 return {'format':'CPA1','regions':len(rn),'portals':len(nr),'edges':len(edges),'memberships':len(members),'bytes':size,'transfer':'shared OSM physical default states only; all boundary portal states retained'}
