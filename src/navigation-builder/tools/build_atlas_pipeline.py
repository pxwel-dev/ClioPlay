"""Pipelined regional atlas builder: extracts and region builds overlap.

Drop-in replacement for tools/build_atlas.py. Behaviour, stamps and the final
atlas assembly are identical; only scheduling changes:

  * a bounded extraction pool (memory cost ~2.5 GB per extract)
  * a bounded region-build pool (each build is a subprocess, ~200-400 MB)
  * no wave barriers: a finished region frees its slot for the next one, so a
    slow dense region no longer stalls the whole queue
  * worker counts are derived from physical cores and available RAM, so 8, 12
    and 16-core machines each get a sensible split (overridable with
    CLIOPLAY_EXTRACT_JOBS / CLIOPLAY_BUILD_JOBS)
  * live progress with done/total, elapsed time and a running ETA

Region stamps stay byte-compatible with build_atlas.py, so a partially built
atlas resumes in either implementation.
"""
import hashlib, json, math, os, shutil, struct, subprocess, sys, threading, time
from collections import deque
from concurrent.futures import FIRST_COMPLETED, ThreadPoolExecutor, wait
from pathlib import Path
HERE = Path(__file__).resolve().parents[1]
STEP = 196608  # 12 Z0 tiles, approximately two degrees
MARGIN = .03
from bundle_layout import SHARED, REGIONAL, remove_obsolete


def sha(p):
    h = hashlib.sha256()
    with Path(p).open('rb') as f:
        for b in iter(lambda: f.read(4 * 1024 * 1024), b''):
            h.update(b)
    return h.hexdigest()


def memory_total():
    try:
        with open('/proc/meminfo') as f:
            for line in f:
                if line.startswith('MemTotal:'):
                    return int(line.split()[1]) * 1024
    except OSError:
        pass
    return 8 * 1024 ** 3


def physical_cores():
    logical = os.cpu_count() or 4
    try:
        pairs = set()
        for block in Path('/proc/cpuinfo').read_text().split('\n\n'):
            if not block.strip():
                continue
            fields = dict(line.split(':', 1) for line in block.splitlines() if ':' in line)
            if 'physical id' in fields and 'core id' in fields:
                pairs.add((fields['physical id'].strip(), fields['core id'].strip()))
        return len(pairs) or max(1, logical // 2)
    except OSError:
        return max(1, logical // 2)


def workers():
    """Extraction slots are RAM-bound; build slots are core-bound."""
    mem = memory_total()
    cores = physical_cores()
    extract = max(1, min(8, int((mem - 2 * 1024 ** 3) // (3500 * 1024 ** 2))))
    build = max(2, min(16, cores - 1))
    try:
        extract = max(1, int(os.environ['CLIOPLAY_EXTRACT_JOBS']))
    except (KeyError, ValueError):
        pass
    try:
        build = max(1, int(os.environ['CLIOPLAY_BUILD_JOBS']))
    except (KeyError, ValueError):
        pass
    return extract, build


def build(pbf, bounds, output, work, cache, imports=(), countries=(), speed_import=None, source=None):
    from speed_import import SpeedImport
    correction = SpeedImport(speed_import) if speed_import else None
    from global_regions import Regions
    from build_timezones import build as timezones
    from build_cameras import HDR as CHDR, REC as CREC
    from atlas_format import write as write_catalog
    staged = work / 'atlas-result'
    staged.mkdir(parents=True, exist_ok=True)
    west, south, east, north = [round(n * 1e5) for n in bounds]
    queue = []
    for y in range(math.floor(south / STEP) * STEP, north + 1, STEP):
        for x in range(math.floor(west / STEP) * STEP, east + 1, STEP):
            queue.append((x / 1e5, y / 1e5, (x + STEP) / 1e5, (y + STEP) / 1e5))

    def builder_fingerprint():
        h = hashlib.sha256()
        files = [HERE / 'build_navigation.py', HERE / 'requirements.txt']
        files += sorted(p for p in (HERE / 'tools').glob('*') if p.suffix in ('.py', '.cpp'))
        files += sorted(p for p in (HERE / 'data').iterdir() if p.is_file() and p.name != '.DS_Store')
        for p in files:
            h.update(str(p.relative_to(HERE)).encode() + b'\0')
            with p.open('rb') as f:
                for chunk in iter(lambda: f.read(1024 * 1024), b''):
                    h.update(chunk)
        if correction:
            h.update(correction.path.read_bytes())
        return h.hexdigest()

    builder_sha = builder_fingerprint()
    source_stat = (pbf.stat().st_size, pbf.stat().st_mtime_ns)

    def unchanged():
        if builder_fingerprint() != builder_sha:
            raise RuntimeError('Builder source/data changed during the atlas build; rerun to resume with one consistent version')
        if (pbf.stat().st_size, pbf.stat().st_mtime_ns) != source_stat:
            raise RuntimeError('Source PBF changed during the atlas build; rerun from a stable extract')

    cameras = {}
    regions = Regions()
    from shapely.geometry import box
    from shapely.ops import unary_union
    coverage = unary_union([g for iso, name, g, p in regions.rows if iso in {c.upper() for c in countries}]).buffer(MARGIN) if countries else None

    jobs = []
    seq = 0
    for core in queue:
        if coverage is not None and not coverage.intersects(box(*core)):
            continue
        seq += 1
        jobs.append((seq, f'r{seq:04d}', core))
    total = len(jobs)
    extract_slots, build_slots = workers()
    print(f'Pipeline: {extract_slots} extraction + {build_slots} build workers for {total} regions', flush=True)
    extract_gate = threading.Semaphore(extract_slots)
    build_gate = threading.Semaphore(build_slots)
    stop = threading.Event()

    def run_region(job):
        number, name, core = job
        x, y, xx, yy = core
        extract = work / ('region-' + name + '.osm.pbf')
        childwork = work / ('work-' + name)
        dest = staged / name
        log = work / (name + '.log')

        def tidy():
            extract.unlink(missing_ok=True)
            shutil.rmtree(childwork, ignore_errors=True)

        if stop.is_set():
            return None
        if shutil.disk_usage(work).free < 3 * 1024 ** 3:
            raise RuntimeError('Less than 3 GiB free; atlas build paused before creating another region. Free scratch space and rerun.')
        with extract_gate:
            if stop.is_set():
                return None
            subprocess.run(['osmium', 'extract', '--bbox', ','.join(map(str, (max(-180, x - MARGIN), max(-85, y - MARGIN), min(180, xx + MARGIN), min(85, yy + MARGIN)))), '--strategy', 'smart', '-S', 'types=multipolygon,restriction,enforcement', str(pbf), '-o', str(extract), '--overwrite'], check=True)
            if extract.stat().st_size <= 100:  # empty PBF header; no valid bbox
                tidy()
                return ('empty', core)
            info = json.loads(subprocess.check_output(['osmium', 'fileinfo', '-e', '-j', str(extract)], text=True))
            if not info['data']['count']['ways']:
                tidy()
                return ('empty', core)
            # A successful cached region must match source/core and every output hash.
            stamp = dest / 'atlas-region.json'
            cached = None
            if stamp.exists():
                cached = json.loads(stamp.read_text())
                if cached.get('builder_sha256') != builder_sha or cached.get('source') != sha(extract) or cached.get('core') != list(core) or any(not (dest / n).is_file() or sha(dest / n) != h for n, h in cached['files'].items()):
                    cached = None
        if cached:
            tidy()
            return ('ok', core, dest)
        shutil.rmtree(childwork, ignore_errors=True)
        shutil.rmtree(dest, ignore_errors=True)
        cmd = [sys.executable, str(HERE / 'build_navigation.py'), str(extract), '--yes', '--single-region', '--atlas-region', '--portal-core', *map(str, core), '--output', str(dest), '--work', str(childwork), '--cache', str(cache)]
        if correction:
            cmd += ['--speed-import', str(correction.path)]
        for p in imports:
            cmd += ['--camera-import', str(p)]
        with build_gate:
            if stop.is_set():
                tidy()
                return None
            _wait_for_memory()
            with log.open('w') as f:
                result = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT)
        if result.returncode:
            text = log.read_text(errors='replace')
            if 'No routable motorcar roads in region' in text:
                tidy()
                return ('empty', core)
            if '2 GiB' in text or 'too many transfer states' in text:
                # Split each dimension independently; odd tile counts need not halve evenly.
                wx = round((xx - x) * 1e5)
                hy = round((yy - y) * 1e5)
                xs = [x, xx] if wx <= 16384 else [x, (round(x * 1e5) + (wx // 32768) * 16384) / 1e5, xx]
                ys = [y, yy] if hy <= 16384 else [y, (round(y * 1e5) + (hy // 32768) * 16384) / 1e5, yy]
                tidy()
                if len(xs) == len(ys) == 2:
                    raise RuntimeError('Minimum atlas core still exceeds device limits; ' + str(log))
                return ('split', [(a, b, c, d) for b, d in zip(ys, ys[1:]) for a, c in zip(xs, xs[1:])])
            tidy()
            raise RuntimeError(f'Region build failed; inspect {log}\n' + text[-1800:])
        region_source = sha(extract)
        files = {n: sha(dest / n) for n in (*REGIONAL, 'cameras.cpc', 'route.cpn')}
        stamp = dest / 'atlas-region.json'
        stamp.write_text(json.dumps({'builder_sha256': builder_sha, 'source': region_source, 'core': list(core), 'files': files}, indent=2) + '\n')
        tidy()
        return ('ok', core, dest)

    completed = []
    started = time.monotonic()
    pending = deque(jobs)
    futures = set()
    with ThreadPoolExecutor(max_workers=min(len(jobs), extract_slots + build_slots)) as pool:
        def submit(job):
            futures.add(pool.submit(run_region, job))
        for job in list(pending):
            submit(job)
        pending.clear()
        while futures:
            done, _ = wait(futures, return_when=FIRST_COMPLETED)
            for future in done:
                futures.discard(future)
                result = future.result()
                if result is None:
                    continue
                if result[0] == 'split':
                    for core in result[1]:
                        seq += 1
                        submit((seq, f'r{seq:04d}', core))
                    continue
                if result[0] != 'ok':
                    continue
                _, core, dest = result
                raw = (dest / 'cameras.cpc').read_bytes()
                h = CHDR.unpack_from(raw)
                if h[:3] != (b'CPC1', 2, CREC.size):
                    raise ValueError('Camera version mismatch')
                for i in range(h[3]):
                    r = CREC.unpack_from(raw, 16 + i * CREC.size)
                    cameras[(r[0], r[1], r[2], r[3], r[5], r[8])] = r
                completed.append((core, dest))
                elapsed = time.monotonic() - started
                rate = elapsed / len(completed)
                left = total - len(completed)
                print(f'[{len(completed)}/{total}] {dest.name} done in {elapsed / 60:.1f} min; ~{left * rate / 60:.0f} min left', flush=True)
                unchanged()
    if not completed:
        raise ValueError('No regional road data in this input')
    unchanged()
    metadata = []
    for core, dest in sorted(completed, key=lambda item: item[1].name):
        intersections = regions.intersecting(core)
        label = '/'.join(code for code, name in intersections) or 'Region'
        metadata.append({'path': dest.name, 'name': label + ' ' + str(int(dest.name[1:])), 'core': list(core)})
    report = write_catalog(staged, metadata)
    regions.write(staged / 'regions.cpg', bounds)
    tz = timezones(staged / 'timezones.cpt', bounds)
    font = HERE / 'data/global-fonts.cpf'
    if sha(font) != json.loads(font.with_suffix('.cpf.json').read_text())['sha256']:
        raise ValueError('Bundled native font pack failed checksum validation')
    shutil.copyfile(font, staged / 'fonts.cpf')
    with (staged / 'cameras.cpc').open('wb') as f:
        records = sorted(cameras.values())
        f.write(CHDR.pack(b'CPC1', 2, CREC.size, len(records), CHDR.size))
        for r in records:
            f.write(CREC.pack(*r))
    names = ['atlas.cpa', *SHARED]
    for m in metadata:
        names.extend(m['path'] + '/' + n for n in REGIONAL)
    manifest = {'builder': 'global-atlas-2', 'speed_import': correction.identity if correction else None, 'builder_sha256': builder_sha, 'source': source or {'name': pbf.name, 'sha256': sha(pbf), 'bounds': bounds}, 'atlas': report, 'timezones': tz, 'regions': metadata, 'files': {n: {'bytes': (staged / n).stat().st_size, 'sha256': sha(staged / n)} for n in names}}
    if not output.exists():
        for path in staged.rglob('*'):
            if path.is_file() and path.relative_to(staged).as_posix() not in names:
                path.unlink()
        (staged / 'build.json').write_text(json.dumps(manifest, indent=2) + '\n')
        output.parent.mkdir(parents=True, exist_ok=True)
        os.replace(staged, output)
        print(f'Atlas ready: {output}: {report["regions"]} regions, {report["portals"]} legal transfer states.', flush=True)
        return manifest
    output.mkdir(parents=True, exist_ok=True)
    # Per-file atomic publication, manifest last. Installer commits as a transaction.
    for name in names:
        dst = output / name
        dst.parent.mkdir(parents=True, exist_ok=True)
        tmp = dst.with_suffix(dst.suffix + '.new')
        shutil.copyfile(staged / name, tmp)
        os.replace(tmp, dst)
    remove_obsolete(output, names)
    (output / 'build.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Atlas ready: {output}: {report["regions"]} regions, {report["portals"]} legal transfer states.', flush=True)
    return manifest


def _available():
    try:
        with open('/proc/meminfo') as f:
            for line in f:
                if line.startswith('MemAvailable:'):
                    return int(line.split()[1]) * 1024
    except OSError:
        pass
    return None


def _wait_for_memory(limit=3 * 1024 ** 3):
    while True:
        free = _available()
        if free is None or free >= limit:
            return
        time.sleep(5)
