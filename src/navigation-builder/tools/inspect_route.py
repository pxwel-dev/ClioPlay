#!/usr/bin/env python3
import argparse,heapq,random,struct
from pathlib import Path
HDR=struct.Struct('<4sHH'+'I'*21+'iiii');TILE=struct.Struct('<iiII');NODE=struct.Struct('<iiIHHIIIIHHI');EDGE=struct.Struct('<I');BASE=struct.Struct('<IIIIHHBBH');CHARC=struct.Struct('<IIIII BBH');UREF=struct.Struct('<I');POINT=struct.Struct('<ii');NO=0xffffffff;ARC_SHORTCUT=1;ARC_GEOM_REVERSED=2
class CPR2:
 def __init__(self,p):
  self.p=Path(p);self.f=self.p.open('rb');h=HDR.unpack(self.f.read(HDR.size))
  (self.magic,self.ver,self.flags,self.n,self.nphys,self.nb,self.rm,self.nt,self.ntr,self.na,self.nuf,self.nur,self.ng,self.nsrc,self.to,self.tro,self.no,self.eo,self.bo,self.so,self.ao,self.ufo,self.uro,self.go,*self.bounds)=h
  if self.magic!=b'CPR2' or self.ver not in (4,5):raise ValueError('not CPR2 v4')
 def node(self,i):self.f.seek(self.no+i*NODE.size);return NODE.unpack(self.f.read(NODE.size))
 def base(self,i):self.f.seek(self.bo+i*BASE.size);return BASE.unpack(self.f.read(BASE.size))
 def arc(self,i):self.f.seek(self.ao+i*CHARC.size);return CHARC.unpack(self.f.read(CHARC.size))
 def incoming(self,i):
  n=self.node(i);first,cnt=n[2],n[3];self.f.seek(self.eo+first*4);return [EDGE.unpack(self.f.read(4))[0] for _ in range(cnt)]
 def upf(self,i):
  n=self.node(i);first,cnt=n[8],n[9];self.f.seek(self.ufo+first*4);return [UREF.unpack(self.f.read(4))[0] for _ in range(cnt)]
 def upr(self,i):
  n=self.node(i);first,cnt=n[11],n[10];self.f.seek(self.uro+first*4);return [UREF.unpack(self.f.read(4))[0] for _ in range(cnt)]
 def geom_base(self,bid):
  b=self.base(bid);gf,gc=b[1],b[4];self.f.seek(self.go+gf*POINT.size);pts=[POINT.unpack(self.f.read(POINT.size)) for _ in range(gc)]
  if b[7]&ARC_GEOM_REVERSED:pts.reverse()
  return pts
 def base_route(self,s,t):
  d={t:0};q=[(0,t)];seen=set()
  while q:
   du,u=heapq.heappop(q)
   if u in seen:continue
   seen.add(u)
   if u==s:return du
   for aid in self.incoming(u):
    a=self.arc(aid);v=a[0];nd=du+a[2]
    if nd<d.get(v,1<<62):d[v]=nd;heapq.heappush(q,(nd,v))
  return None
 def ch_route(self,s,t,with_path=False):
  df={s:0};db={t:0};pf={};pb={};qf=[(0,s)];qb=[(0,t)];sf=set();sb=set();best=1<<62;meet=None
  while qf or qb:
   mf=qf[0][0] if qf else 1<<62;mb=qb[0][0] if qb else 1<<62
   if mf>=best and mb>=best:break
   if mf<=mb:
    du,u=heapq.heappop(qf)
    if u in sf or du!=df.get(u):continue
    sf.add(u)
    if u in db and du+db[u]<best:best=du+db[u];meet=u
    for aid in self.upf(u):
     a=self.arc(aid);v=a[1];nd=du+a[2]
     if nd<df.get(v,1<<62):df[v]=nd;pf[v]=(u,aid);heapq.heappush(qf,(nd,v))
   else:
    du,u=heapq.heappop(qb)
    if u in sb or du!=db.get(u):continue
    sb.add(u)
    if u in df and du+df[u]<best:best=du+df[u];meet=u
    for aid in self.upr(u):
     a=self.arc(aid);v=a[0];nd=du+a[2]
     if nd<db.get(v,1<<62):db[v]=nd;pb[v]=(u,aid);heapq.heappush(qb,(nd,v))
  if meet is None:return (None,[]) if with_path else None
  if not with_path:return best
  fas=[];u=meet
  while u!=s:u0,aid=pf[u];fas.append(aid);u=u0
  fas.reverse();bas=[];u=meet
  while u!=t:u1,aid=pb[u];bas.append(aid);u=u1
  return best,fas+bas
 def unpack(self,aid,out):
  a=self.arc(aid)
  if a[6]&ARC_SHORTCUT:self.unpack(a[3],out);self.unpack(a[4],out)
  else:out.append(aid)
 def route_arc_path(self,arcs):
  out=[]
  for a in arcs:self.unpack(a,out)
  return out
 def physical_base_path(self,arcs):return [self.arc(a)[3] for a in self.route_arc_path(arcs)]
 def geometry_path(self,arcs):
  pts=[]
  for bid in self.physical_base_path(arcs):
   g=self.geom_base(bid)
   if pts and g and pts[-1]==g[0]:pts.extend(g[1:])
   else:pts.extend(g)
  return pts

def main():
 ap=argparse.ArgumentParser();ap.add_argument('file',type=Path);ap.add_argument('--route',nargs=2,type=int);ap.add_argument('--validate',type=int,default=0);ap.add_argument('--seed',type=int,default=1);a=ap.parse_args();c=CPR2(a.file)
 print(f'CPR2 v{c.ver}: route_states={c.n:,} physical_nodes={c.nphys:,} physical_base_arcs={c.nb:,} route_base_arcs={c.rm:,} ch_arcs={c.na:,} shortcuts={c.na-c.rm:,} shortcut/route={(c.na-c.rm)/max(1,c.rm):.3f} geom_points={c.ng:,} source_refs={c.nsrc:,} tile_refs={c.ntr:,} upf={c.nuf:,} upr={c.nur:,} tiles={c.nt:,} size={c.p.stat().st_size:,} bounds={c.bounds}')
 if a.route:
  s,t=a.route;b=c.base_route(s,t);h,arcs=c.ch_route(s,t,True);print('base_cost',b,'ch_cost',h,'hierarchy_arcs',len(arcs));print('physical_base_arcs',c.physical_base_path(arcs)[:100] if h is not None else 'NO ROUTE');print('geometry_points',len(c.geometry_path(arcs)) if h is not None else 0)
 if a.validate:
  r=random.Random(a.seed);ok=0;fail=0
  for _ in range(a.validate):
   s=r.randrange(c.n);t=r.randrange(c.n);b=c.base_route(s,t);h=c.ch_route(s,t)
   if b!=h:print('MISMATCH',s,t,b,h);fail+=1;break
   ok+=1
  print(f'validated={ok} fail={fail}')
if __name__=='__main__':main()
