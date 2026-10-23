#!/usr/bin/env python3
"""Bounded-memory validator for ClioPlay CPM1/CPM2/CPM3/CPM4 map packs."""
import argparse, struct, sys
from pathlib import Path
HDR=struct.Struct('<4sHBBIIIIiiii')
IDX=struct.Struct('<BBxxiiIIHH')
MAX_TILE=32768
SUPPORTED_MAX_LEVEL=6
MF_SPEED_WEAK=0x01


def read_exact(f,n):
    b=f.read(n)
    if len(b)!=n: raise SystemExit(f'unexpected EOF (wanted {n}, got {len(b)})')
    return b


def validate_cpm1(data):
    if len(data)<2: raise ValueError('tile too short')
    nf=struct.unpack_from('<H',data,0)[0]; p=2
    for j in range(nf):
        if p+2>len(data): raise ValueError(f'truncated road {j}')
        cls,np=data[p],data[p+1]; p+=2
        if np<2: raise ValueError(f'road {j} np={np}')
        p+=np*4
        if p>len(data): raise ValueError(f'road {j} coordinate overflow')
    if p!=len(data): raise ValueError(f'{len(data)-p} trailing bytes')
    return nf,0,0,0,0,0


def validate_labels(data,p,nl):
    for j in range(nl):
        if p+8>len(data): raise ValueError(f'truncated label {j}')
        kind,prio,x,y,tlen,res=struct.unpack_from('<BBHHBB',data,p); p+=8
        if not(1<=kind<=8): raise ValueError(f'label {j} kind={kind}')
        if tlen<1 or tlen>112: raise ValueError(f'label {j} text length={tlen}')
        if p+tlen>len(data): raise ValueError(f'label {j} text overflow')
        raw=data[p:p+tlen]; p+=tlen
        raw.decode('utf-8')
    return p


def validate_cpm2(data):
    if len(data)<4: raise ValueError('tile too short')
    ng,nl=struct.unpack_from('<HH',data,0); p=4
    for j in range(ng):
        if p+4>len(data): raise ValueError(f'truncated geometry {j}')
        layer,style,np,res=data[p:p+4]; p+=4
        if layer>3: raise ValueError(f'geometry {j} layer={layer}')
        if np<2: raise ValueError(f'geometry {j} np={np}')
        p+=np*4
        if p>len(data): raise ValueError(f'geometry {j} coordinate overflow')
    p=validate_labels(data,p,nl)
    if p!=len(data): raise ValueError(f'{len(data)-p} trailing bytes')
    return ng,nl,0,0,0,0


def _validate_meta_container(data,version):
    if len(data)<8: raise ValueError('tile too short')
    ng,nl,nm,res=struct.unpack_from('<HHHH',data,0); p=8; refs=[]
    for j in range(ng):
        if p+4>len(data): raise ValueError(f'truncated geometry {j}')
        layer,style,np,mi=data[p:p+4]; p+=4
        if layer>3: raise ValueError(f'geometry {j} layer={layer}')
        if np<2: raise ValueError(f'geometry {j} np={np}')
        if mi!=255: refs.append((j,mi,layer))
        if version>=4 and style&128:
            if layer not in (1,2) or (layer==1 and style&127 not in (4,5)) or np<4 or mi!=255:
                raise ValueError(f'geometry {j} invalid area record')
            if p+np*4>len(data): raise ValueError(f'geometry {j} area coordinate overflow')
            if data[p:p+4]!=data[p+(np-1)*4:p+np*4]:
                raise ValueError(f'geometry {j} open area contour')
            if any(q>4095 for q in struct.unpack_from(f'<{np*2}H',data,p)):
                raise ValueError(f'geometry {j} area coordinate outside tile')
        p+=np*4
        if p>len(data): raise ValueError(f'geometry {j} coordinate overflow')
    p=validate_labels(data,p,nl)
    metas=[]; weak=0
    for j in range(nm):
        need=5 if version>=5 else 3 if version>=4 else 2
        if p+need>len(data): raise ValueError(f'truncated road metadata {j}')
        if version>=5:
            mph,flags,back,bflags,tlen=data[p:p+5]; p+=5
            if bflags & ~31: raise ValueError("bad reverse speed flags")
            if flags & ~0x1F: raise ValueError(f'road metadata {j} unsupported flags=0x{flags:02x}')
            if flags & MF_SPEED_WEAK: weak+=1
        elif version==4:
            mph,flags,tlen=data[p:p+3];p+=3
        else:
            mph,tlen=data[p],data[p+1]; flags=0; p+=2
        if mph>255: raise ValueError(f'road metadata {j} mph={mph}')
        if tlen>112: raise ValueError(f'road metadata {j} text length={tlen}')
        if p+tlen>len(data): raise ValueError(f'road metadata {j} text overflow')
        raw=data[p:p+tlen]; p+=tlen
        raw.decode('utf-8')
        metas.append((mph,flags,raw))
    for j,mi,layer in refs:
        if layer!=0: raise ValueError(f'non-road geometry {j} references road metadata')
        if mi>=nm: raise ValueError(f'geometry {j} metadata index {mi} >= {nm}')
    if p!=len(data): raise ValueError(f'{len(data)-p} trailing bytes')
    known=sum(1 for mph,_,_ in metas if mph>0); unknown=nm-known
    weak_known=sum(1 for mph,flags,_ in metas if mph>0 and flags&MF_SPEED_WEAK)
    return ng,nl,nm,known,unknown,weak_known


def validate_tile(f,base,off,ln,expected,idx,fmt):
    f.seek(base+off); data=read_exact(f,ln)
    try:
        if fmt==1: vals=validate_cpm1(data)
        elif fmt==2: vals=validate_cpm2(data)
        else: vals=_validate_meta_container(data,fmt)
    except ValueError as e: raise SystemExit(f'tile {idx}: {e}')
    g,l,m,sk,su,sw=vals
    if g+l+m!=expected: raise SystemExit(f'tile {idx} count mismatch index={expected} payload={g+l+m}')
    return vals


def main():
    ap=argparse.ArgumentParser(); ap.add_argument('map',type=Path); ap.add_argument('--full',action='store_true'); a=ap.parse_args()
    size=a.map.stat().st_size
    with a.map.open('rb') as ix:
        h=HDR.unpack(read_exact(ix,HDR.size)); magic,ver,mn,mx,flags,count,ioff,doff,minlat,minlon,maxlat,maxlon=h
        if (magic,ver)==(b'CPM1',1): fmt=1
        elif (magic,ver)==(b'CPM2',2): fmt=2
        elif (magic,ver)==(b'CPM3',3): fmt=3
        elif (magic,ver)==(b'CPM4',4): fmt=4
        elif (magic,ver)==(b'CPM5',5): fmt=5
        else: raise SystemExit(f'unsupported map {magic!r} v{ver}')
        if mn>mx or mx>SUPPORTED_MAX_LEVEL: raise SystemExit(f'unsupported levels {mn}..{mx}')
        IDX=struct.Struct('<BBxxiiIIHH' if fmt>=5 else '<BBhhxxIIHH')
        if ioff!=HDR.size or doff!=ioff+count*IDX.size: raise SystemExit('bad index layout')
        if doff>size: raise SystemExit('index past EOF')
        last=None; last_end=0; maxlen=0; total=0; by=[0]*12; samples=[]; sample_indices={0,count//4,count//2,(3*count)//4,max(0,count-1)} if count else set()
        ix.seek(ioff)
        for i in range(count):
            lvl,fl,tx,ty,off,ln,nf,pad=IDX.unpack(read_exact(ix,IDX.size)); key=(lvl,tx,ty,fl)
            ocean=fmt>=4 and flags&4 and 8<=lvl<=11
            if not(mn<=lvl<=mx or ocean): raise SystemExit(f'bad level at {i}')
            if fl and not ocean: raise SystemExit(f'page on a non-ocean tile at {i}')
            if ocean and fl and (last is None or last[:3]!=key[:3] or last[3]+1!=fl):
                raise SystemExit(f'ocean page sequence broken at {i}')
            if last is not None and key<=last: raise SystemExit(f'index not sorted at {i}: {key} <= {last}')
            last=key
            if ln>MAX_TILE: raise SystemExit(f'tile {i} exceeds cache: {ln}')
            if off<last_end or doff+off+ln>size: raise SystemExit(f'bad data offset at {i}')
            last_end=off+ln; maxlen=max(maxlen,ln); total+=nf; by[lvl]+=1
            if i in sample_indices: samples.append((i,off,ln,nf))
        if doff+last_end!=size: raise SystemExit('data end mismatch')
    geom=labels=meta=speed_known=speed_unknown=speed_weak=0
    with a.map.open('rb') as ix,a.map.open('rb') as data:
        if a.full:
            ix.seek(ioff)
            for i in range(count):
                lvl,fl,tx,ty,off,ln,nf,pad=IDX.unpack(read_exact(ix,IDX.size)); g,l,m,sk,su,sw=validate_tile(data,doff,off,ln,nf,i,fmt); geom+=g; labels+=l; meta+=m; speed_known+=sk; speed_unknown+=su; speed_weak+=sw
                if i and i%50000==0: print(f'  validated {i:,}/{count:,} tiles',file=sys.stderr)
        else:
            for i,off,ln,nf in samples:
                g,l,m,sk,su,sw=validate_tile(data,doff,off,ln,nf,i,fmt); geom+=g; labels+=l; meta+=m; speed_known+=sk; speed_unknown+=su; speed_weak+=sw
    print(f'CPM{fmt} v{ver}: {a.map}')
    print(f'  size={size:,} bytes ({size/1048576:.2f} MiB)')
    print(f'  tiles={count:,} records={total:,} levels={mn}..{mx} flags=0x{flags:x}')
    print(f'  tiles_by_level={by[mn:mx+1]} max_tile={maxlen:,}/{MAX_TILE:,} bytes')
    if flags&4: print(f'  shared_ocean_pages_by_source_level={by[8:12]}')
    print(f'  bounds_e5=({minlat},{minlon})..({maxlat},{maxlon})')
    if a.full and fmt>=2: print(f'  geometry={geom:,} labels={labels:,} road_meta={meta:,}')
    if a.full and fmt>=3:
        pct=(100.0*speed_known/meta) if meta else 0.0
        print(f'  speed_meta_known={speed_known:,} speed_meta_unknown={speed_unknown:,} ({pct:.1f}% metadata records carry a speed)')
    if a.full and fmt>=4:
        wp=(100.0*speed_weak/speed_known) if speed_known else 0.0
        print(f'  speed_meta_weak={speed_weak:,} speed_meta_strong={speed_known-speed_weak:,} ({wp:.1f}% of known metadata is ?-estimated)')
    print('  pack='+('synthetic diagnostic' if flags&1 else 'OpenStreetMap/production'))
    print('  payload_check='+('FULL' if a.full else 'SAMPLED')+' OK')
if __name__=='__main__': main()
