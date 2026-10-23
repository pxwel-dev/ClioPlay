#!/usr/bin/env python3
"""Exercise real CPM5 matching and inference with global synthetic street networks.

Temporary fixtures only: no installed map, preference or country build is changed.
"""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'navigation-builder/tools'))
from build_map import build_streams
from global_regions import Regions
from global_speeds import directions, BLOCKED, VARIABLE
from build_speed_defaults import main as defaults


def main():
    defaults()
    assert directions({'highway':'residential','maxspeed':'walk'})==(BLOCKED,BLOCKED)
    assert directions({'maxspeed:lanes':'30|50'})==(BLOCKED,BLOCKED)
    assert directions({'maxspeed:lanes:backward:conditional':'30 @ (wet)'})[1]==VARIABLE
    cases=[
        (48.85,2.35,{},'', '50?'),
        (40.42,-3.70,{},'30','30?'),
        (51.48,-3.18,{},'20 mph','20?'),
        (37.77,-122.42,{},'25 mph','25?'),
        (-33.86,151.20,{},'', '--'), # conflicting subnational defaults
        (0.,0.,{},'30','--'),       # unresolved country
        (48.86,2.35,{'maxspeed':'20'},'50','20'),
        (48.87,2.35,{'maxspeed':'50','maxspeed:conditional':'30 @ (wet)'},'50','VAR'),
        (48.88,2.35,{'maxspeed':'walk'},'50','--'),
        (48.89,2.35,{'highway':'living_street'},'50','--'),
        (48.90,2.35,{'highway':'tertiary_link'},'50','--'),
        (48.91,2.35,{'oneway':'-1'},'50','--'),
        (48.92,2.35,{'maxspeed':'none'},'50','INF'),
        (40.43,-3.70,{},'', '--'),   # Spanish lane/streetscape context absent
    ]
    with tempfile.TemporaryDirectory(prefix='clioplay-speed-estimates-') as td:
        work=Path(td);maps=work/'clio843/maps';maps.mkdir(parents=True)
        Regions().write(maps/'regions.cpg');objects=[];expected=[];wid=0
        for lat,lon,tags,nearby,result in cases:
            expected.append(f'{round(lat*1e5)} {round(lon*1e5)} 90 {result}\n')
            for i in range(6):
                wid+=1
                props={'@id':wid,'highway':'residential' if i else 'tertiary',
                       'name':f'STREET {i}'}
                if i==0:props.update(tags)
                elif nearby and i<=3:props['maxspeed']=nearby
                y=lat+i*.0003
                objects.append(json.dumps({'type':'Feature','properties':props,
                    'geometry':{'type':'LineString','coordinates':[[lon-.001,y],[lon+.001,y]]}})+'\n')
        build_streams(iter(objects),iter(()),maps/'map.cpm',work,demo=True)
        targets=work/'targets.txt';targets.write_text(''.join(expected))
        for source,args in [('speed-estimates',[]),('speed-map',[str(targets)])]:
            exe=work/source
            subprocess.run(['cc','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all',
                '-DCLIO_HOST_ROOT="'+str(work)+'/"',str(ROOT/'tests'/f'{source}.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe),*args],check=True)


if __name__=='__main__':main()
