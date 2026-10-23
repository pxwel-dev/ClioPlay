#!/usr/bin/env python3
"""Validate a completed country atlas and execute a real multi-pack journey.

Coverage is the supplied PBF's coverage, not a certification of country completeness.
"""
import argparse
import json
from pathlib import Path
import random
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'navigation-builder/tools'))
from install_nav import validate_bundle
from inspect_route import CPR2
from atlas_format import HDR, REG
from build_cameras import HDR as CAMERA_HDR, REC as CAMERA_REC
from global_regions import Regions


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('bundle', type=Path)
    p.add_argument('--journey', type=float, nargs=4, required=True,
                   metavar=('START_LAT', 'START_LON', 'END_LAT', 'END_LON'))
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--clock-city', nargs=3, metavar=('LAT', 'LON', 'IANA_ZONE'), help='Check an actual city with central-European DST rules')
    p.add_argument('--queries-per-region', type=int, default=3)
    args = p.parse_args()
    bundle = args.bundle.resolve()
    validate_bundle(bundle)
    manifest = json.loads((bundle/'build.json').read_text())
    catalog = (bundle/'atlas.cpa').read_bytes()
    h = HDR.unpack_from(catalog)
    assert h[3] <= 256 and h[4] <= 65534 and h[5] <= 1000000 and len(catalog) <= 32*1024*1024
    camera_raw = (bundle/'cameras.cpc').read_bytes()
    camera_header = CAMERA_HDR.unpack_from(camera_raw)
    assert camera_header[:3] == (b'CPC1',2,CAMERA_REC.size)
    policy = Regions(); camera_types = {}; camera_countries = {}; matched = 0
    for i in range(camera_header[3]):
        record = CAMERA_REC.unpack_from(camera_raw,camera_header[4]+i*CAMERA_REC.size)
        lat,lon,endlat,endlon = record[:4]; kind = record[8]
        assert policy.camera_allowed(lat/1e5,lon/1e5), record
        assert record[5] == 65535 or record[5] < 360, record
        if kind == 3: assert policy.camera_allowed(endlat/1e5,endlon/1e5), record
        iso = record[9].decode(); assert iso in policy.CAMERA_ALLOWED, record
        camera_types[str(kind)] = camera_types.get(str(kind),0)+1
        camera_countries[iso] = camera_countries.get(iso,0)+1
        matched += bool(record[-1])
    print('All exported camera entries/exits satisfy policy; records:',camera_header[3],flush=True)
    clock = None
    queries = []
    rng = random.Random(20261020)
    for i in range(h[3]):
        row = REG.unpack_from(catalog, h[7]+i*REG.size)
        name = row[0].split(b'\0')[0].decode()
        route = CPR2(bundle/name/'route.cpr')
        for _ in range(args.queries_per_region):
            a,b = rng.randrange(route.n),rng.randrange(route.n)
            base,ch = route.base_route(a,b),route.ch_route(a,b)
            assert base == ch, (name,a,b,base,ch)
            queries.append({'region':name,'from':a,'to':b,'cost_ds':ch})
        route.f.close()
        print(name, 'CH/reference queries passed', flush=True)
    with tempfile.TemporaryDirectory(prefix='clioplay-country-test-') as td:
        td = Path(td);(td/'clio843').mkdir()
        (td/'clio843/maps').symlink_to(bundle,target_is_directory=True)
        shutil.copyfile(ROOT/'clio843/font.bin',td/'clio843/font.bin')
        exe = td/'test'
        subprocess.run(['cc','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all',
                        '-DCLIO_HOST_ROOT="'+str(td)+'/"',str(ROOT/'tests/atlas-country.c'),'-o',str(exe)],check=True)
        result = subprocess.check_output([str(exe),*[str(round(v*1e5)) for v in args.journey]],text=True,timeout=1800)
        if args.clock_city:
            clock_exe = td/'clock'
            subprocess.run(['cc','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all',str(ROOT/'tests/clock.c'),'-o',str(clock_exe)],check=True)
            lat,lon,zone = args.clock_city
            clock = subprocess.check_output([str(clock_exe),str(bundle/'timezones.cpt'),str(round(float(lat)*1e5)),str(round(float(lon)*1e5)),zone],text=True).strip()
        journey = json.loads(result)
        assert journey['regions'] >= 2, journey
    report = {'bundle':bundle.name,'source':manifest['source'],'atlas':manifest['atlas'],
              'manifest_and_file_hashes':'passed','queries':queries,
              'cameras':{'records':camera_header[3],'types':camera_types,'countries':camera_countries,'road_id_records':matched,'policy_entry_and_exit_checks':'passed'},'clock':clock,
              'journey_coordinates':args.journey,'native_journey':journey,
              'limits':'Host UBSan; no physical head-unit timing or completeness claim.'}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(journey))


if __name__ == '__main__':
    main()
