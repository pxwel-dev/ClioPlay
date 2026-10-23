#!/usr/bin/env python3
"""Build directed synthetic graphs and verify ordered destination fallback.

Run with the navigation builder's Python environment. All generated files stay
temporary. No coastline cache, installed maps or head unit are required.
"""
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILDER = ROOT/'navigation-builder/tools'
sys.path.insert(0, str(BUILDER))
from atlas_format import write
from build_route import repack_for_atlas, write_portal_candidates
from inspect_route import CPR2


def main():
    with tempfile.TemporaryDirectory(prefix='clioplay-atlas-destination-') as folder:
        for spread in (False, True):
            work = Path(folder)/str(int(spread))
            pack = work/'clio843/maps/region'
            pack.mkdir(parents=True)
            nodes = {1:(55,-1.65), 2:(55,-1.64), 3:(55.0001,-1.65), 4:(55.0001,-1.64)}
            ways = [(2,1), (1,3), (3,4)]
            if spread:
                nodes[5] = (55.01,-1.65)
                ways += [(1,5), (5,2)]
            parts = ['<osm version="0.6">']
            parts += [f'<node id="{i}" lat="{lat}" lon="{lon}"/>' for i,(lat,lon) in nodes.items()]
            for i,(a,b) in enumerate(ways, 10):
                parts.append(f'<way id="{i}"><nd ref="{a}"/><nd ref="{b}"/>'
                             '<tag k="highway" v="residential"/><tag k="oneway" v="yes"/>'
                             '<tag k="maxspeed" v="30 mph"/></way>')
            source = work/'case.osm'
            source.write_text(''.join(parts)+'</osm>')
            route = pack/'route.cpr'
            with (work/'build.log').open('w') as log:
                subprocess.run([sys.executable, str(BUILDER/'build_route.py'), '--pbf', str(source),
                                '--out', str(route), '--work', str(work/'route-work')], check=True,
                               stdout=log, stderr=subprocess.STDOUT)
                graph = CPR2(route)
                coordinates = {(5500000,-165000)}
                if spread:
                    coordinates.add((5501000,-165000))
                portals = [i for i in range(graph.nphys) if graph.node(i)[:2] in coordinates]
                start = next(i for i in portals if graph.node(i)[:2] == (5500000,-165000))
                graph.f.close()
                repack_for_atlas(route, work/'route-work', portals)
                with sqlite3.connect(work/'route-work/route4.sqlite') as db:
                    write_portal_candidates(db, route, route.with_suffix('.cpn'), portals)
                subprocess.run([sys.executable, str(BUILDER/'build_qadj.py'), str(route),
                                str(pack/'route.qadj')], check=True, stdout=log, stderr=subprocess.STDOUT)
            write(pack.parent, [{'path':'region', 'name':'synthetic', 'core':[-2,54,0,56]}])
            exe = work/'test'
            subprocess.run(['cc', '-O1', '-g', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            '-DCLIO_HOST_ROOT="'+str(work)+'/"', str(ROOT/'tests/atlas-destination.c'),
                            '-o', str(exe)], check=True)
            subprocess.run([str(exe), str(start), str(0 if spread else 1)], check=True)


if __name__ == '__main__':
    main()
