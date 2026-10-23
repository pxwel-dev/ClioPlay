#!/usr/bin/env python3
import argparse, struct, math
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('qadj',type=Path);ap.add_argument('route',type=Path);a=ap.parse_args()
qb=a.qadj.read_bytes(); rb=a.route.read_bytes()
if len(rb)<108 or rb[:4]!=b'CPR2' or struct.unpack_from('<H',rb,4)[0] not in (4,5): raise SystemExit('route is not CPR2 v4')
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
        if struct.unpack_from('<H',rb,4)[0]>=5:
            d=abs(lat)//100
            if d>=90000:return 0
            i,f=divmod(d,1000);a=round(math.cos(math.radians(i))*32767);b=round(math.cos(math.radians(i+1))*32767)
            return a+int((b-a)*f/1000) # C signed division truncates towards zero
        d=(abs(lat)+50000)//100000; c=(21500,21054,20603,20145,19681,19211,18736,18255,17769,17278,16782,16282,15777)
        return 21500 if d<49 else 15777 if d>61 else c[d-49]
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
elif magic==b'QAD6':
    if len(qb)<80: raise SystemExit('sidecar too small')
    ver,entry=struct.unpack_from('<HH',qb,4); vals=struct.unpack_from('<18I',qb,8)
    n,ca,fc,rc,re,rbytes,index_off,index_bytes,data_off,data_bytes,rmeta_off,rmeta_bytes=vals[:12]
    # Correct DEV5.32+ layout: flags,reserved,gmeta_off,gmeta_bytes,gpoints,gmeta_entry.
    # DEV5.31 builder layout accidentally wrote gmeta fields first; accept it for diagnosis.
    if (vals[12]&62)==62:
        flags,res0,gmeta_off,gmeta_bytes,gpoints,gmeta_entry=vals[12:18]; legacy=False
    else:
        gmeta_off,gmeta_bytes,gpoints,flags,res0,res1=vals[12:18]; gmeta_entry=12; legacy=True
    geom_points=struct.unpack_from('<I',rb,44)[0]; base_off=struct.unpack_from('<I',rb,68)[0]; geom_off=struct.unpack_from('<I',rb,88)[0]
    if ver!=6 or entry!=16: raise SystemExit('invalid QAD6 header')
    if (n,ca,fc,rc,re,rbytes)!=(nodes,ch,uf,ur,route_edges,len(rb)) or gpoints!=geom_points: raise SystemExit('QAD6 does not match route.cpr')
    if index_off!=80 or index_bytes!=nodes*4 or data_off!=index_off+index_bytes or rmeta_off!=data_off+data_bytes or rmeta_bytes!=ch*28 or gmeta_off!=rmeta_off+rmeta_bytes or gmeta_bytes!=geom_points*12 or gmeta_entry!=12 or len(qb)!=gmeta_off+gmeta_bytes or (flags&62)!=62: raise SystemExit('QAD6 layout/size mismatch')
    idx=struct.unpack_from(f'<{nodes}I',qb,index_off)
    if any(x>=data_bytes for x in idx): raise SystemExit('QAD6 node offset outside query data')
    seen=0
    for node,off in enumerate(idx):
        pp=data_off+off
        if pp+4>rmeta_off: raise SystemExit(f'QAD6 block header out of range for node {node}')
        fcnt,rcnt=struct.unpack_from('<HH',qb,pp); size=4+(fcnt+rcnt)*16
        if off+size>data_bytes: raise SystemExit(f'QAD6 block out of range for node {node}')
        pp+=4
        for _ in range(fcnt+rcnt):
            other,cost,arc,nextoff=struct.unpack_from('<IIII',qb,pp);pp+=16
            if other>=nodes or arc>=ch or nextoff!=idx[other]: raise SystemExit(f'QAD6 adjacency mismatch at node {node}')
            seen+=1
    if seen!=uf+ur: raise SystemExit('QAD6 adjacency total does not match route.cpr')
    BASE=struct.Struct('<IIIIHHBBH'); PT=struct.Struct('<ii')
    def cq(lat):
        if struct.unpack_from('<H',rb,4)[0]>=5:
            d=abs(lat)//100
            if d>=90000:return 0
            i,f=divmod(d,1000);a=round(math.cos(math.radians(i))*32767);b=round(math.cos(math.radians(i+1))*32767)
            return a+int((b-a)*f/1000) # C signed division truncates towards zero
        d=(abs(lat)+50000)//100000; c=(21500,21054,20603,20145,19681,19211,18736,18255,17769,17278,16782,16282,15777)
        return 21500 if d<49 else 15777 if d>61 else c[d-49]
    def units(a,b):
        dx=abs(((b[1]-a[1])*cq((a[0]+b[0])//2))>>15);dy=abs(b[0]-a[0]);hi=max(dx,dy);lo=min(dx,dy);return hi+((lo*3)>>3)
    def ratio(num,den):
        if not den or not num:return 0
        if num>=den:return 65536
        while den>65535:
            den=(den+1)>>1;num=(num+1)>>1
            if num>=den:return 65536
        return (num<<16)//den
    for bid in range(base_edges):
        _,gf,_,_,gc,_,_,_,_=BASE.unpack_from(rb,base_off+bid*BASE.size)
        if gc<2: continue
        pts=[PT.unpack_from(rb,geom_off+(gf+j)*8) for j in range(gc)];cum=[0];total=0
        for j in range(1,gc): total+=units(pts[j-1],pts[j]);cum.append(total)
        for j in range(gc):
            ef=min(65535,ratio(cum[j],total));er=min(65535,ratio(total-cum[j],total));em=0 if not j else (units(pts[j-1],pts[j])*111+50)//100
            af,ar,amf,amr,ag,_=struct.unpack_from('<HHHHHH',qb,gmeta_off+(gf+j)*12)
            af=65536 if ag&1 else af; ar=65536 if ag&2 else ar
            emr=0 if not j else (units(pts[j],pts[j-1])*111+50)//100
            if (af,ar,amf,amr)!=(ratio(cum[j],total),ratio(total-cum[j],total),em,emr): raise SystemExit(f'QAD6 geometry metadata mismatch base={bid} point={j}')
    print(('QAD6 v6 LEGACY-HEADER OK: ' if legacy else 'QAD6 v6 OK: ')+f'nodes={nodes:,} CH arcs={ch:,} forward={uf:,} reverse={ur:,} mmap-index={index_bytes/1048576:.1f} MiB query-data={data_bytes/1048576:.1f} MiB recon={rmeta_bytes/1048576:.1f} MiB geom-meta={gmeta_bytes/1048576:.1f} MiB total={len(qb)/1048576:.1f} MiB')
elif magic==b'QAD5':
    if len(qb)<64: raise SystemExit('sidecar too small')
    ver,entry=struct.unpack_from('<HH',qb,4); vals=struct.unpack_from('<14I',qb,8)
    n,ca,fc,rc,re,rbytes,index_off,index_bytes,data_off,data_bytes,rmeta_off,rmeta_bytes,flags,reserved=vals
    if ver!=5 or entry!=16: raise SystemExit('invalid QAD5 header')
    if (n,ca,fc,rc,re,rbytes)!=(nodes,ch,uf,ur,route_edges,len(rb)): raise SystemExit('QAD5 does not match route.cpr')
    if index_off!=64 or index_bytes!=nodes*4 or data_off!=index_off+index_bytes or rmeta_off!=data_off+data_bytes or rmeta_bytes!=ch*28 or len(qb)!=rmeta_off+rmeta_bytes or (flags&30)!=30: raise SystemExit('QAD5 layout/size mismatch')
    idx=struct.unpack_from(f'<{nodes}I',qb,index_off)
    if any(x>=data_bytes for x in idx): raise SystemExit('QAD5 node offset outside query data')
    # Sample/full block validation is cheap enough for normal builder-side use.
    seen=0
    for node,off in enumerate(idx):
        p=data_off+off
        if p+4>rmeta_off: raise SystemExit(f'QAD5 block header out of range for node {node}')
        fcnt,rcnt=struct.unpack_from('<HH',qb,p); size=4+(fcnt+rcnt)*16
        if off+size>data_bytes: raise SystemExit(f'QAD5 block out of range for node {node}')
        p+=4
        for _ in range(fcnt+rcnt):
            other,cost,arc,nextoff=struct.unpack_from('<IIII',qb,p);p+=16
            if other>=nodes or arc>=ch or nextoff!=idx[other]: raise SystemExit(f'QAD5 adjacency mismatch at node {node}')
            seen+=1
    if seen!=uf+ur: raise SystemExit('QAD5 adjacency total does not match route.cpr')
    print(f'QAD5 v5 OK: nodes={nodes:,} CH arcs={ch:,} forward={uf:,} reverse={ur:,} mmap-index={index_bytes/1048576:.1f} MiB query-data={data_bytes/1048576:.1f} MiB recon={rmeta_bytes/1048576:.1f} MiB total={len(qb)/1048576:.1f} MiB')
elif magic in (b'QAD3',b'QAD4'):
    if len(qb)<64: raise SystemExit('sidecar too small')
    ver,entry=struct.unpack_from('<HH',qb,4); vals=struct.unpack_from('<14I',qb,8)
    n,ca,fc,rc,re,rbytes,shift,gc,gidx,data,data_bytes,rmeta,rmeta_bytes,flags=vals
    expect_ver=4 if magic==b'QAD4' else 3; recsz=28 if magic==b'QAD4' else 20; needflags=7 if magic==b'QAD4' else 3
    if ver!=expect_ver or entry!=12: raise SystemExit('invalid QAD header')
    if (n,ca,fc,rc,re,rbytes)!=(nodes,ch,uf,ur,route_edges,len(rb)): raise SystemExit('QAD does not match route.cpr')
    if gidx!=64 or data!=gidx+(gc+1)*4 or rmeta!=data+data_bytes or rmeta_bytes!=ch*recsz or len(qb)!=rmeta+rmeta_bytes or (flags&needflags)!=needflags: raise SystemExit('QAD layout/size mismatch')
    print(f'{magic.decode()} v{ver} OK: nodes={nodes:,} CH arcs={ch:,} forward={uf:,} reverse={ur:,} groups={gc:,} shift={shift} index={(gc+1)*4/1048576:.2f} MiB query-data={data_bytes/1048576:.1f} MiB recon={rmeta_bytes/1048576:.1f} MiB total={len(qb)/1048576:.1f} MiB')
elif magic in (b'QAD2',b'QAD1'):
    print(magic.decode(),'legacy sidecar detected; PERF7 can fall back to it, but QAD6 is recommended.')
else: raise SystemExit('not a QAD sidecar')
