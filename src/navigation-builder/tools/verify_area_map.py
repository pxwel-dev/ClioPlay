#!/usr/bin/env python3
"""Independently hash every road, label and metadata record before/after area merge."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import time

from build_map import HDR, IDX


def digest(path):
    hasher=hashlib.sha256();roads=labels=metadata=areas=0;last=time.monotonic()
    with path.open('rb') as ix,path.open('rb') as data:
        h=HDR.unpack(ix.read(HDR.size))
        if h[:2]!=(b'CPM5',5):raise ValueError('CPM5 required')
        ix.seek(h[6]);data.seek(h[7]);expected=0
        for i in range(h[5]):
            z,flags,tx,ty,off,length,records,pad=IDX.unpack(ix.read(IDX.size))
            if off!=expected:raise ValueError('non-contiguous tile stream')
            tile=data.read(length);expected+=length
            if len(tile)!=length or length<8:raise ValueError('truncated tile')
            ng,nl,nm,res=struct.unpack_from('<4H',tile);pos=8;road=[]
            for _ in range(ng):
                if pos+4>len(tile):raise ValueError('truncated geometry')
                layer,style,np,mi=tile[pos:pos+4];end=pos+4+np*4
                if end>len(tile):raise ValueError('geometry past tile end')
                if layer==0:road.append(tile[pos:end])
                if style&128:areas+=1
                pos=end
            if road or nl or nm:
                hasher.update(struct.pack('<BhhIII',z,tx,ty,len(road),nl,nm))
                for blob in road:hasher.update(blob)
                hasher.update(tile[pos:])
            roads+=len(road);labels+=nl;metadata+=nm
            if time.monotonic()-last>10:
                print(f'{path.name}: {i+1:,}/{h[5]:,} tiles verified',flush=True);last=time.monotonic()
    return {'sha256':hasher.hexdigest(),'roads':roads,'labels':labels,'metadata':metadata,'areas':areas,
            'tiles':h[5],'flags':h[4],'bytes':path.stat().st_size}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('base',type=Path);ap.add_argument('rebuilt',type=Path)
    ap.add_argument('--report',type=Path)
    a=ap.parse_args();base=digest(a.base);rebuilt=digest(a.rebuilt)
    for key in ('sha256','roads','labels','metadata'):
        if base[key]!=rebuilt[key]:raise SystemExit(f'preservation check failed: {key}')
    if not rebuilt['flags']&2 or not rebuilt['areas']:
        raise SystemExit('rebuilt map has no complete-area geometry')
    report={'preservation':'PASS: all road/label/metadata bytes and tile locations match',
            'base':base,'rebuilt':rebuilt}
    if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
