#!/usr/bin/env python3
import argparse, struct, math
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('qadj',type=Path);ap.add_argument('route',type=Path);a=ap.parse_args()
qb=a.qadj.read_bytes(); rb=a.route.read_bytes()
if len(rb)<108 or rb[:4]!=b'CPR2' or struct.unpack_from('<H',rb,4)[0] !=5: raise SystemExit('route is not CPR2 v5')
nodes,phys_nodes,base_edges,route_edges=struct.unpack_from('<IIII',rb,8);ch=struct.unpack_from('<I',rb,32)[0];uf,ur=struct.unpack_from('<II',rb,36)
magic=qb[:4]
if magic==b'QAD7':
    if len(qb)<80: raise SystemExit('sidecar too small')
    ver,entry=struct.unpack_from('<HH',qb,4); vals=struct.unpack_from('<18I',qb,8)
    n,ca,fc,rc,re,rbytes,index_off,index_bytes,data_off,data_bytes,rmeta_off,rmeta_bytes,flags,reserved,packet_off,packet_bytes,packet_points,packet_entry=vals
    if ver!=7 or entry!=16: raise SystemExit('invalid QAD7 header')
    if (n,ca,fc,rc,re,rbytes)!=(nodes,ch,uf,ur,route_edges,len(rb)): raise SystemExit('QAD7 does not match route.cpr')
    if index_off!=80 or index_bytes!=nodes*4 or data_off!=index_off+index_bytes or rmeta_off!=data_off+data_bytes or rmeta_bytes!=ch*32 or packet_off!=rmeta_off+rmeta_bytes or packet_entry!=20 or packet_bytes!=packet_points*20 or len(qb)!=packet_off+packet_bytes or (flags&94)!=94: raise SystemExit('QAD7 layout/size mismatch')
    idx=struct.unpack_from(f'<{nodes}I',qb,index_off)
    if any(x>=data_bytes for x in idx): raise SystemExit('QAD7 node offset outside query data')
    seen=0
    for node,off in enumerate(idx):
        pp=data_off+off
        if pp+4>rmeta_off: raise SystemExit(f'QAD7 block header out of range for node {node}')
        fcnt,rcnt=struct.unpack_from('<HH',qb,pp); size=4+(fcnt+rcnt)*16
        if off+size>data_bytes: raise SystemExit(f'QAD7 block out of range for node {node}')
        pp+=4
        for _ in range(fcnt+rcnt):
            other,cost,arc,nextoff=struct.unpack_from('<IIII',qb,pp);pp+=16
            if other>=nodes or arc>=ch or nextoff!=idx[other]: raise SystemExit(f'QAD7 adjacency mismatch at node {node}')
            seen+=1
    if seen!=uf+ur: raise SystemExit('QAD7 adjacency total does not match route.cpr')
    BASE=struct.Struct('<IIIIHHBBH'); PT=struct.Struct('<ii')
    base_off=struct.unpack_from('<I',rb,68)[0]; geom_off=struct.unpack_from('<I',rb,88)[0]
    def cq(lat):
        d=abs(lat)//100
        if d>=90000:return 0
        i,f=divmod(d,1000);a=round(math.cos(math.radians(i))*32767);b=round(math.cos(math.radians(i+1))*32767)
        return a+int((b-a)*f/1000) # C signed division truncates towards zero
    def units(a,b):
        dx=abs(((b[1]-a[1])*cq((a[0]+b[0])//2))>>15);dy=abs(b[0]-a[0]);hi=max(dx,dy);lo=min(dx,dy);return hi+((lo*3)>>3)
    def ratio(num,den):
        if not den or not num:return 0
        if num>=den:return 65536
        while den>65535:
            den=(den+1)>>1;num=(num+1)>>1
            if num>=den:return 65536
        return (num<<16)//den
    checked={}
    for aid in range(ch):
        r=rmeta_off+aid*32
        fr,cost,left,right,tf,gf,gi,poff=struct.unpack_from('<8I',qb,r)
        fl=tf>>24
        if fl&1: continue
        if left>=base_edges: raise SystemExit(f'QAD7 base id out of range arc={aid}')
        _,bgf,_,_,gc,_,cls,bflags,_=BASE.unpack_from(rb,base_off+left*BASE.size)
        if gf!=bgf or (gi&0xffff)!=gc or ((gi>>16)&255)!=bflags or (gi>>24)!=cls: raise SystemExit(f'QAD7 base metadata mismatch arc={aid}')
        if poff==0xffffffff or poff+gc*20>packet_bytes: raise SystemExit(f'QAD7 packet offset out of range arc={aid}')
        key=(gf,gc,poff)
        if key in checked: continue
        checked[key]=1
        pts=[PT.unpack_from(rb,geom_off+(gf+j)*8) for j in range(gc)]
        cum=[0];total=0;mf=[0]*gc;mr=[0]*gc
        for j in range(1,gc):
            u=units(pts[j-1],pts[j]);total+=u;cum.append(total);mf[j]=(u*111+50)//100
            urv=units(pts[j],pts[j-1]);mr[j-1]=(urv*111+50)//100
        for j in range(gc):
            at=packet_off+poff+j*20
            la,lo,qf16,qr16,mfv,mrv,pfl,_=struct.unpack_from('<iiHHHHHH',qb,at)
            qf=65536 if pfl&1 else qf16;qr=65536 if pfl&2 else qr16
            if (la,lo)!=(pts[j][0],pts[j][1]) or qf!=ratio(cum[j],total) or qr!=ratio(total-cum[j],total) or mfv!=mf[j] or mrv!=mr[j]: raise SystemExit(f'QAD7 packet mismatch arc={aid} point={j}')
    print(f'QAD7 v7 OK: nodes={nodes:,} CH arcs={ch:,} forward={uf:,} reverse={ur:,} mmap-index={index_bytes/1048576:.1f} MiB query-data={data_bytes/1048576:.1f} MiB recon={rmeta_bytes/1048576:.1f} MiB packets={packet_bytes/1048576:.1f} MiB unique-polylines={len(checked):,} total={len(qb)/1048576:.1f} MiB')
else: raise SystemExit('QAD7 required')
