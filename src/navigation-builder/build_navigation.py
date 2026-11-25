#!/usr/bin/env python3
"""Build the Global navigation bundle from one or more OSM PBF extracts; Python setup is automatic."""
import argparse
import hashlib
from importlib.metadata import PackageNotFoundError, version
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
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


def prompt_extracts():
    paths = []
    while True:
        prompt = (
            'Path to the first .osm.pbf file (you can drag it here): ' if not paths else
            'Path to another .osm.pbf file, or Enter to continue: '
        )
        entered = input(prompt).strip()
        if not entered:
            return paths
        # Keep literal paths with spaces or apostrophes, as well as shell-quoted
        # and escaped paths pasted or dragged into a terminal.
        if Path(entered).expanduser().is_file():
            paths.append(Path(entered))
        else:
            try:
                parts = shlex.split(entered)
            except ValueError:
                parts = []
            paths.extend(map(Path, parts if parts and all(x.lower().endswith('.pbf') for x in parts)
                             else [entered]))


def fileinfo(pbf, extended=False):
    args = ['osmium', 'fileinfo', '-F', 'pbf', '-j']
    if extended:
        args.append('-e')
    return json.loads(subprocess.check_output([*args, str(pbf)], text=True))


def data_bounds(metadata, envelope=None):
    data = metadata['data']
    bounds = data.get('bbox')
    boxes = metadata['header'].get('boxes', [])
    if envelope is None and len(boxes) == 1:
        envelope = boxes[0]
    if bounds and envelope and envelope[0] < envelope[2] and envelope[1] < envelope[3]:
        bounds = [max(envelope[0], bounds[0]), max(envelope[1], bounds[1]),
                  min(envelope[2], bounds[2]), min(envelope[3], bounds[3])]
    return bounds


def validate_extract(metadata, merged=False):
    data = metadata['data']
    if merged and data.get('multiple_versions'):
        raise ValueError('Overlapping extracts contain different versions of the same OSM object. '
                         'Download fresh matching extracts from the same provider and snapshot, '
                         'or select one extract covering the whole area.')
    if (metadata['file']['format'] != 'PBF' or metadata['header'].get('with_history')
            or data.get('multiple_versions') or not data['count']['ways']):
        raise ValueError('Use a current, single-version .osm.pbf extract with road ways')


def merge_extracts(paths, work):
    """Merge compatible extracts into the reusable local build directory."""
    if len(paths) < 2:
        raise ValueError('Select at least two distinct extracts to merge.')
    merged = work/'merged.osm.pbf'
    stamp = work/'merged-extracts.json'
    sources = [{'path': str(path), 'sha256': sha(path)} for path in paths]
    try:
        cached = json.loads(stamp.read_text())
        if cached['sources'] == sources and cached['sha256'] == sha(merged):
            print('Reusing verified merged map extract.', flush=True)
            return merged, cached['fileinfo'], cached['envelope'], sources, cached['sha256']
    except (OSError, ValueError, KeyError):
        pass

    boxes = []
    complete_bounds = True
    for path in paths:
        header = fileinfo(path)['header']
        if header.get('with_history'):
            raise ValueError(f'Use a current OSM snapshot, without history: {path.name}')
        declared = header.get('boxes', [])
        if len(declared) == 1 and declared[0][0] < declared[0][2] and declared[0][1] < declared[0][3]:
            boxes.append(declared[0])
        else:
            complete_bounds = False
    # Osmium merge drops the PBF header bounds. Retain the input envelopes to
    # avoid sizing the map to remote nodes pulled in by complete relations.
    envelope = ([min(b[0] for b in boxes), min(b[1] for b in boxes),
                 max(b[2] for b in boxes), max(b[3] for b in boxes)] if complete_bounds else None)
    print(f'Merging {len(paths)} extracts into one navigation build…', flush=True)
    with tempfile.TemporaryDirectory(prefix='merge-', dir=work) as temp:
        staged = Path(temp)/merged.name
        subprocess.run(['osmium', 'merge', '-F', 'pbf', '-o', str(staged), *map(str, paths)],
                       check=True)
        metadata = fileinfo(staged, extended=True)
        validate_extract(metadata, merged=True)
        digest = sha(staged)
        metadata['file']['name'] = str(merged)
        new_stamp = Path(temp)/stamp.name
        new_stamp.write_text(json.dumps({'sources': sources, 'sha256': digest,
                                         'fileinfo': metadata, 'envelope': envelope}, indent=2)+'\n')
        os.replace(staged, merged)
        os.replace(new_stamp, stamp)
    return merged, metadata, envelope, sources, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pbf',type=Path,nargs='*',help='one or more country or regional .osm.pbf extracts')
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
    paths = args.pbf
    if not paths:
        if not sys.stdin.isatty():
            parser.error('At least one PBF path is required in non-interactive mode')
        paths = prompt_extracts()
        if not paths:
            raise SystemExit('Cancelled before building files.')
        # Preserve the entire selection when dependency bootstrap re-executes.
        sys.argv[1:1] = [str(path.expanduser().resolve()) for path in paths]
    paths = [path.expanduser().resolve() for path in paths]
    for path in paths:
        if not path.is_file() or path.suffix.lower() != '.pbf':
            parser.error(f'Expected an existing .osm.pbf file: {path}')
    if len(set(paths)) != len(paths):
        parser.error('Select each extract only once.')
    if args.atlas_region and len(paths) != 1:
        parser.error('An internal atlas region requires a single PBF')
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
    pbf = paths[0]
    work_name = pbf.stem
    if len(paths) > 1:
        # Stable across input order and reruns. Content changes invalidate the
        # merge and downstream caches without abandoning reusable build work.
        paths.sort()
        work_name = 'merged-' + hashlib.sha256(
            json.dumps([str(path) for path in paths]).encode()).hexdigest()[:16]
    work = (args.work or HERE/'.build'/work_name).expanduser().resolve()
    output = args.output.expanduser().resolve()
    if output == work or work in output.parents or output in work.parents:
        parser.error('output and work directories must be separate')
    work.mkdir(parents=True, exist_ok=True)
    sources = []
    envelope = None
    if len(paths) > 1:
        if (work/'merged.osm.pbf') in paths:
            parser.error('Choose a different work directory for these input files')
        pbf, metadata, envelope, sources, digest = merge_extracts(paths, work)
    else:
        stamp = work/'source-fileinfo-cache.json'
        digest = sha(pbf)
        fingerprint = {'path': str(pbf), 'sha256': digest}
        try:
            cached = json.loads(stamp.read_text())
        except (ValueError, OSError):
            cached = None
        if cached and cached.get('source') == fingerprint:
            metadata = cached['fileinfo']
        else:
            metadata = fileinfo(pbf, extended=True)
            stamp.write_text(json.dumps({'source': fingerprint, 'fileinfo': metadata}, indent=2)+'\n')
    validate_extract(metadata, merged=bool(sources))
    data = metadata['data']
    bounds = data_bounds(metadata, envelope)
    if (not bounds or bounds[2] < bounds[0] or bounds[3] < bounds[1]
            or bounds[2]-bounds[0] > 180 or bounds[1] < -85 or bounds[3] > 85):
        parser.error('Choose regional extracts within 85 degrees latitude and not spanning the date line; a planet pack exceeds device limits')
    (work/'source-fileinfo.json').write_text(json.dumps(metadata,indent=2)+'\n')
    regions=Regions();countries=regions.intersecting(bounds)
    codes={code for code,name in countries}
    if not countries: parser.error('Country could not be verified from the actual extract bounds')
    if any(code.upper() not in codes for code in args.country):parser.error('Expected country does not intersect the PBF data bounds')
    source = {'name': pbf.name, 'sha256': digest, 'bounds': bounds}
    if sources:
        source['extracts'] = [{'name': path.name, 'sha256': info['sha256']}
                              for path, info in zip(paths, sources)]
    print('1. Road data source: '+', '.join(path.name for path in paths))
    print('2. Selected PBF envelope (west, south, east, north): '+', '.join(f'{n:.5f}' for n in bounds))
    print('   Country polygons intersecting that rectangle (not additional road extracts): '+', '.join(f'{name} ({code})' for code,name in countries))
    print(f"   {data['count']['nodes']:,} nodes; {data['count']['ways']:,} ways; latest object {data['timestamp']['last']}")
    print('   Roads come only from the selected extracts. Routes can cross borders where the road network connects.')
    print('   The rectangle is an envelope, not a list of installed countries or proof of full coverage.')
    if not args.yes:
        if not sys.stdin.isatty():parser.error('Use --yes with --country ISO for an unattended build')
        if input('Is this the country/region you intend to install? [y/N] ').strip().lower() not in ('y','yes'):raise SystemExit('Cancelled before building files.')
    print(f'Output folder: {output}\nBuild work: {work}',flush=True)
    if args.atlas or (not args.single_region and pbf.stat().st_size>350*1024*1024):
        from build_atlas import build
        build(pbf,bounds,output,work,args.cache,args.camera_import,args.country,args.speed_import,source=source);return
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
    report = {'builder':'global-atlas-region-2' if args.atlas_region else 'global-navigation-2','speed_import':speed_import.identity if speed_import else None,'source':{**source,'intersecting_countries':countries},'regions':region_report,'timezones':timezone_report,'cameras':camera_report,'files':{
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
