#!/usr/bin/env python3
"""Build the Global navigation bundle from one OSM PBF; Python setup is automatic."""
import argparse
import hashlib
from importlib.metadata import PackageNotFoundError, version
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import venv

HERE = Path(__file__).resolve().parent
PACKAGES = {'shapely': '2.2.0', 'pyshp': '2.3.1', 'osmium':'4.2.0','tzdata':'2026.5'}


def dependencies():
    if sys.version_info < (3,11): raise SystemExit('Python 3.11 or newer is required.')
    try:
        if all(version(name) == wanted for name,wanted in PACKAGES.items()): return
    except PackageNotFoundError: pass
    environment = HERE/'.venv'
    python = environment/('Scripts/python.exe' if os.name == 'nt' else 'bin/python')
    if os.environ.get('CLIOPLAY_DEPENDENCIES_READY'):
        raise SystemExit('Python dependencies could not be loaded from the build environment.')
    if not python.exists():
        print('Creating a local Python build environment...',flush=True)
        venv.EnvBuilder(with_pip=True).create(environment)
    subprocess.run([str(python),'-m','pip','install','-r',str(HERE/'requirements.txt')],check=True)
    env = dict(os.environ, CLIOPLAY_DEPENDENCIES_READY='1')
    os.execve(str(python),[str(python),str(Path(__file__).resolve()),*sys.argv[1:]],env)


def run(script,*args):
    subprocess.run([sys.executable,str(HERE/'tools'/script),*map(str,args)],check=True)


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda:source.read(4*1024*1024),b''): digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pbf',type=Path,nargs='?',help='OSM country or regional .osm.pbf extract')
    parser.add_argument('--atlas',action='store_true',help='build overlapping regions with cross-pack routing; automatic for extracts above 350 MB')
    parser.add_argument('--single-region',action='store_true',help='disable automatic atlas splitting for a measured regional build')
    parser.add_argument('--portal-core',nargs=4,type=float,help=argparse.SUPPRESS)
    parser.add_argument('--yes',action='store_true',help='accept detected region non-interactively')
    parser.add_argument('--country',action='append',default=[],help='expected ISO code; fail if absent from extract bounds')
    parser.add_argument('--speed-import',type=Path,help='optional JSON corrections for untagged OSM ways, with source attribution')
    parser.add_argument('--camera-import',type=Path,action='append',default=[],help='optional camera CSV, repeatable')
    parser.add_argument('--output',type=Path,default=HERE/'output')
    parser.add_argument('--work',type=Path,help='reusable scratch directory')
    parser.add_argument('--cache',type=Path,default=HERE/'.cache')
    parser.add_argument('--atlas-region',action='store_true',help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.atlas_region and not args.portal_core: parser.error('--atlas-region requires --portal-core')
    if args.pbf is None:
        if not sys.stdin.isatty(): parser.error('PBF path required in non-interactive mode')
        import shlex
        entered=input('1. Path to the .osm.pbf file (you may drag it here): ').strip()
        parts=shlex.split(entered)
        args.pbf=Path(parts[0] if len(parts)==1 else entered)
        sys.argv.insert(1,str(args.pbf)) # preserve selection when dependency bootstrap re-executes
    pbf = args.pbf.expanduser().resolve()
    if not pbf.is_file(): parser.error(f'PBF not found: {pbf}')
    if not shutil.which('osmium'): parser.error('Install osmium-tool first; see README.md.')
    if not shutil.which('c++'): parser.error('Install a C++17 compiler (command: c++); see README.md.')
    for path in args.camera_import:
        if not path.expanduser().is_file(): parser.error(f'camera CSV not found: {path}')
    if args.speed_import and not args.speed_import.expanduser().is_file():parser.error('Speed import not found')
    dependencies()
    sys.path.insert(0,str(HERE/'tools'))
    from speed_import import SpeedImport
    speed_import=SpeedImport(args.speed_import) if args.speed_import else None
    from global_regions import Regions
    from bundle_layout import REGIONAL, NAMES, remove_obsolete
    work=(args.work or HERE/'.build'/pbf.stem).expanduser().resolve();work.mkdir(parents=True,exist_ok=True)
    stamp=work/'source-fileinfo-cache.json';fingerprint={'path':str(pbf),'sha256':sha(pbf)};cached=None
    if stamp.exists():
        try:cached=json.loads(stamp.read_text())
        except (ValueError,OSError):pass
    if cached and cached.get('source')==fingerprint:metadata=cached['fileinfo']
    else:
        metadata=json.loads(subprocess.check_output(['osmium','fileinfo','-e','-j',str(pbf)],text=True))
        stamp.write_text(json.dumps({'source':fingerprint,'fileinfo':metadata},indent=2)+'\n')
    data=metadata['data'];raw_bounds=data.get('bbox');bounds=raw_bounds
    boxes=metadata['header'].get('boxes',[])
    if len(boxes)==1 and raw_bounds:
        h=boxes[0]
        if h[0]<h[2] and h[1]<h[3]:bounds=[max(h[0],raw_bounds[0]),max(h[1],raw_bounds[1]),min(h[2],raw_bounds[2]),min(h[3],raw_bounds[3])]
    # Complete international relations can pull remote nodes into a country
    # extract. The declared extract envelope defines installation coverage;
    # actual node extent/counts are retained separately for audit.
    if metadata['file']['format']!='PBF' or metadata['header'].get('with_history') or data.get('multiple_versions') or not data['count']['ways']:
        parser.error('Use a current, single-version .osm.pbf extract with road ways')
    if not bounds or bounds[2]-bounds[0]>180 or bounds[1]<-85 or bounds[3]>85:
        parser.error('Choose a regional extract within 85 degrees latitude and not spanning the date line; a planet pack exceeds device limits')
    (work/'source-fileinfo.json').write_text(json.dumps(metadata,indent=2)+'\n')
    regions=Regions();countries=regions.intersecting(bounds)
    codes={code for code,name in countries}
    if not countries: parser.error('Country could not be verified from the actual extract bounds')
    if any(code.upper() not in codes for code in args.country):parser.error('Expected country does not intersect the PBF data bounds')
    print('1. Road data source: '+pbf.name)
    print('2. Selected PBF envelope (west, south, east, north): '+', '.join(f'{n:.5f}' for n in bounds))
    print('   Country polygons intersecting that rectangle (not additional road extracts): '+', '.join(f'{name} ({code})' for code,name in countries))
    print(f"   {data['count']['nodes']:,} nodes; {data['count']['ways']:,} ways; latest object {data['timestamp']['last']}")
    print('   Roads come only from the selected PBF, including any border overlaps it contains.')
    print('   The rectangle is an envelope, not a list of installed countries or proof of full coverage.')
    if not args.yes:
        if not sys.stdin.isatty():parser.error('Use --yes with --country ISO for an unattended build')
        if input('Is this the country/region you intend to install? [y/N] ').strip().lower() not in ('y','yes'):raise SystemExit('Cancelled before building files.')
    output = args.output.expanduser().resolve()
    if output == work or work in output.parents or output in work.parents:
        parser.error('output and work directories must be separate')
    print(f'Output folder: {output}\nBuild work: {work}',flush=True)
    if args.atlas or (not args.single_region and pbf.stat().st_size>350*1024*1024):
        from build_atlas import build
        build(pbf,bounds,output,work,args.cache,args.camera_import,args.country,args.speed_import);return
    staged = work/'result'; staged.mkdir(parents=True,exist_ok=True)
    steps=4 if args.atlas_region else 5
    print(f'[1/{steps}] Complete CPM5 map: roads, water, greenery, sea and estuaries',flush=True)
    run('build_map.py','--pbf',pbf,'--out',staged/'map.cpm','--work',work/'map','--cache',args.cache,*(['--speed-import',args.speed_import.expanduser().resolve()] if args.speed_import else []))
    if args.portal_core:
        from clip_map import crop_to_core
        crop_to_core(staged/'map.cpm',args.portal_core)
    run('inspect_map.py',staged/'map.cpm','--full')
    print(f'[2/{steps}] CPR2 v5 routing graph',flush=True)
    run('build_route.py','--pbf',pbf,'--speed-db',work/'map/speed.sqlite','--out',staged/'route.cpr','--work',work/'route',*(['--portal-core',*args.portal_core] if args.portal_core else []))
    run('inspect_route.py',staged/'route.cpr')
    print(f'[3/{steps}] Matching QAD7 acceleration file',flush=True)
    run('build_qadj.py',staged/'route.cpr',staged/'route.qadj')
    run('inspect_qadj.py',staged/'route.qadj',staged/'route.cpr')
    region_report=timezone_report=None
    if not args.atlas_region:
        print('[4/5] Offline country, unit and timezone metadata',flush=True)
        region_report=regions.write(staged/'regions.cpg',bounds)
        from build_timezones import build as build_timezones
        timezone_report=build_timezones(staged/'timezones.cpt',bounds)
    print(f'[{steps}/{steps}] OSM cameras and optional imports (regional policy applied)',flush=True)
    from build_cameras import build as build_cameras
    camera_report=build_cameras(pbf,[p.expanduser().resolve() for p in args.camera_import],staged/'cameras.cpc',regions)
    print(json.dumps(camera_report,indent=2))
    if not args.atlas_region:
        font=HERE/'data/global-fonts.cpf'
        font_info=json.loads(font.with_suffix('.cpf.json').read_text())
        if sha(font)!=font_info['sha256']:parser.error('Bundled native font pack failed checksum validation')
        shutil.copyfile(font,staged/'fonts.cpf')
    names = (*REGIONAL,'cameras.cpc') if args.atlas_region else NAMES
    if any((staged/n).stat().st_size>=0x80000000 for n in names):raise RuntimeError('Navigation file exceeds the device 2 GiB seek limit; use a smaller region')
    report = {'builder':'global-atlas-region-2' if args.atlas_region else 'global-navigation-2','speed_import':speed_import.identity if speed_import else None,'source':{'name':pbf.name,'sha256':fingerprint['sha256'],'bounds':bounds,'intersecting_countries':countries},'regions':region_report,'timezones':timezone_report,'cameras':camera_report,'files':{
        name:{'bytes':(staged/name).stat().st_size,'sha256':sha(staged/name)} for name in names}}
    # Publish only after every component has built and passed its inspector.
    output.mkdir(parents=True,exist_ok=True)
    for name in names:
        temporary = output/(name+'.new')
        shutil.copyfile(staged/name,temporary); os.replace(temporary,output/name)
    if args.portal_core:shutil.copyfile(staged/'route.cpn',output/'route.cpn')
    remove_obsolete(output,names)
    (output/'build.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f'Navigation ready: {output}\nInstall the complete bundle using the guided installer.',flush=True)


if __name__ == '__main__':
    try: main()
    except (OSError,subprocess.CalledProcessError,RuntimeError,ValueError) as error:
        raise SystemExit(f'Build failed: {error}')
