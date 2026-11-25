"""Bounded overlapping regional atlas from one verified PBF.

Cores align with CPM Z0 tiles, so no tile straddles two owners. Smart extraction
keeps complete restriction and area relations. CH boundary states contract last;
all such states participate in the overlay, without proximity-based connectors.

Set CLIOPLAY_PIPELINE=1 to enable bounded concurrent extraction and region-build
pools (sized from host RAM and physical cores, overridable via
CLIOPLAY_EXTRACT_JOBS and CLIOPLAY_BUILD_JOBS). Sequential and pipelined modes
share the exact same region naming, stamps, split rules and assembly, so a
partially built atlas can resume cleanly in either mode with identical output.
"""
from collections import deque
from concurrent.futures import FIRST_COMPLETED, ThreadPoolExecutor, wait
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import threading
import time

HERE = Path(__file__).resolve().parents[1]
STEP = 196608  # 12 Z0 tiles, approximately two degrees
MARGIN = 0.03
from bundle_layout import SHARED, REGIONAL, remove_obsolete


def sha(p):
    h = hashlib.sha256()
    with Path(p).open('rb') as f:
        for b in iter(lambda: f.read(4 * 1024 * 1024), b''):
            h.update(b)
    return h.hexdigest()


def pipeline_enabled(override=None):
    if override is not None:
        return bool(override)
    raw = os.environ.get('CLIOPLAY_PIPELINE', '').strip().lower()
    return raw not in ('', '0', 'false', 'no', 'off')


def memory_total():
    """Return total host RAM in bytes (Linux, macOS, POSIX fallback)."""
    try:
        with open('/proc/meminfo', encoding='utf-8') as f:
            for line in f:
                if line.startswith('MemTotal:'):
                    return int(line.split()[1]) * 1024
    except OSError:
        pass
    if sys.platform == 'darwin':
        try:
            out = subprocess.check_output(
                ['sysctl', '-n', 'hw.memsize'], text=True, timeout=5
            ).strip()
            if out:
                return int(out)
        except (OSError, ValueError, subprocess.SubprocessError):
            pass
    try:
        if hasattr(os, 'sysconf'):
            pages = os.sysconf('SC_PHYS_PAGES')
            size = os.sysconf('SC_PAGE_SIZE')
            if pages > 0 and size > 0:
                return int(pages) * int(size)
    except (OSError, ValueError):
        pass
    return 8 * 1024 ** 3


def memory_available():
    """Return currently available RAM in bytes, or None if unavailable."""
    try:
        with open('/proc/meminfo', encoding='utf-8') as f:
            for line in f:
                if line.startswith('MemAvailable:'):
                    return int(line.split()[1]) * 1024
    except OSError:
        pass
    if sys.platform == 'darwin':
        try:
            out = subprocess.check_output(['vm_stat'], text=True, timeout=5)
            m = re.search(r'page size of (\d+) bytes', out)
            page_size = int(m.group(1)) if m else 4096
            pages = 0
            for label in ('Pages free', 'Pages inactive', 'Pages speculative', 'Pages purgeable'):
                hit = re.search(rf'^{re.escape(label)}:\s+(\d+)\.', out, re.MULTILINE)
                if hit:
                    pages += int(hit.group(1))
            if pages > 0:
                return pages * page_size
        except (OSError, ValueError, subprocess.SubprocessError):
            pass
    return None


def physical_cores():
    """Return physical CPU core count (Linux x86/ARM and macOS)."""
    logical = os.cpu_count() or 4
    if sys.platform == 'darwin':
        try:
            out = subprocess.check_output(
                ['sysctl', '-n', 'hw.physicalcpu'], text=True, timeout=5
            ).strip()
            if out and int(out) > 0:
                return int(out)
        except (OSError, ValueError, subprocess.SubprocessError):
            pass
    try:
        pairs = set()
        for block in Path('/proc/cpuinfo').read_text(encoding='utf-8', errors='replace').split('\n\n'):
            if not block.strip():
                continue
            fields = {}
            for line in block.splitlines():
                if ':' in line:
                    k, v = line.split(':', 1)
                    fields[k.strip()] = v.strip()
            if 'physical id' in fields and 'core id' in fields:
                pairs.add((fields['physical id'], fields['core id']))
        if pairs:
            return len(pairs)
    except OSError:
        pass
    try:
        topo_pairs = set()
        for cpu in Path('/sys/devices/system/cpu').glob('cpu[0-9]*'):
            pkg = cpu / 'topology/physical_package_id'
            cid = cpu / 'topology/core_id'
            if pkg.is_file() and cid.is_file():
                topo_pairs.add((pkg.read_text().strip(), cid.read_text().strip()))
        if topo_pairs:
            return len(topo_pairs)
    except OSError:
        pass
    arch = platform.machine().lower()
    if arch.startswith(('arm', 'aarch64')):
        return max(1, logical)
    return max(1, logical // 2)


def workers(pipeline=None):
    """Size extraction (RAM-bound) and region-build (core-bound) worker pools."""
    if not pipeline_enabled(pipeline):
        return 1, 1
    mem = memory_total()
    cores = physical_cores()
    extract = max(1, min(8, int((mem - 2 * 1024 ** 3) // (3500 * 1024 ** 2))))
    build_n = max(1, min(16, cores - 1))
    for env_key, is_extract in (('CLIOPLAY_EXTRACT_JOBS', True), ('CLIOPLAY_BUILD_JOBS', False)):
        raw = os.environ.get(env_key)
        if raw is not None:
            try:
                val = int(raw)
                if val >= 1:
                    if is_extract:
                        extract = val
                    else:
                        build_n = val
            except ValueError:
                pass
    return extract, build_n


def region_name(key):
    """Deterministic region path matching [a-z0-9_-]{1,23}.

    Top-level grid cells use r0001..rNNNN (preserving existing cache paths);
    split sub-regions append deterministic child indices (e.g. r0005_0).
    """
    head = f'r{key[0]:04d}'
    if len(key) == 1:
        return head
    return head + '_' + '_'.join(str(part) for part in key[1:])


def region_label(code_label, key):
    if len(key) == 1:
        return f'{code_label} {key[0]}'
    return f'{code_label} ' + '.'.join(str(part) for part in key)


def split_core(core):
    """Split an oversized core along each axis > 16384 E5 units, or return None."""
    x, y, xx, yy = core
    wx = round((xx - x) * 1e5)
    hy = round((yy - y) * 1e5)
    xs = [x, xx] if wx <= 16384 else [x, (round(x * 1e5) + (wx // 32768) * 16384) / 1e5, xx]
    ys = [y, yy] if hy <= 16384 else [y, (round(y * 1e5) + (hy // 32768) * 16384) / 1e5, yy]
    if len(xs) == len(ys) == 2:
        return None
    return [(a, b, c, d) for b, d in zip(ys, ys[1:]) for a, c in zip(xs, xs[1:])]


class _MemoryGate:
    """Bounded build-slot gate with deadlock-free, thundering-herd-free RAM throttling."""

    def __init__(self, max_slots, limit_bytes=3 * 1024 ** 3, avail_fn=None, poll_s=5.0):
        self.max_slots = max(1, int(max_slots))
        self.limit_bytes = limit_bytes
        self.avail_fn = avail_fn or memory_available
        self.poll_s = poll_s
        self.cond = threading.Condition()
        self.active = 0
        self.warned = False

    def acquire(self, stop):
        with self.cond:
            while not stop.is_set():
                if self.active >= self.max_slots:
                    self.cond.wait(timeout=1.0)
                    continue
                if self.active == 0:
                    # Always allow at least one active build so low-RAM hosts never deadlock.
                    self.active += 1
                    return True
                free = self.avail_fn()
                if free is None or free >= self.limit_bytes:
                    self.active += 1
                    return True
                if not self.warned:
                    print(
                        f'Waiting for available RAM ({free / 1024 ** 3:.1f} GiB free) '
                        'before starting another parallel region build...',
                        flush=True,
                    )
                    self.warned = True
                self.cond.wait(timeout=self.poll_s)
            return False

    def release(self):
        with self.cond:
            if self.active > 0:
                self.active -= 1
            self.cond.notify()

    def wake_all(self):
        with self.cond:
            self.cond.notify_all()


class _ProcessTracker:
    """Run child subprocesses with fail-fast cancellation on error or Ctrl-C."""

    def __init__(self, stop):
        self.stop = stop
        self.lock = threading.Lock()
        self.active = set()

    def run(self, cmd, stdout=None, stderr=None, text=False, check=False):
        with self.lock:
            if self.stop.is_set():
                return None
            proc = subprocess.Popen(cmd, stdout=stdout, stderr=stderr, text=text)
            self.active.add(proc)
        try:
            out, err = proc.communicate()
        finally:
            with self.lock:
                self.active.discard(proc)
        if check and proc.returncode != 0 and not self.stop.is_set():
            raise subprocess.CalledProcessError(proc.returncode, cmd, output=out, stderr=err)
        return subprocess.CompletedProcess(cmd, proc.returncode, out, err)

    def terminate_all(self):
        with self.lock:
            procs = list(self.active)
        for proc in procs:
            try:
                proc.terminate()
            except OSError:
                pass
        deadline = time.monotonic() + 2.0
        for proc in procs:
            left = max(0.0, deadline - time.monotonic())
            try:
                proc.wait(timeout=left)
            except (OSError, subprocess.TimeoutExpired):
                try:
                    proc.kill()
                except OSError:
                    pass


def _read_stamp(path):
    if not path.exists():
        return None
    try:
        data = json.loads(path.read_text(encoding='utf-8'))
        return data if isinstance(data, dict) and isinstance(data.get('files'), dict) else None
    except (OSError, ValueError, TypeError):
        return None


def _write_stamp(path, payload):
    tmp = path.with_suffix(path.suffix + '.tmp')
    tmp.write_text(json.dumps(payload, indent=2) + '\n', encoding='utf-8')
    os.replace(tmp, path)


def build(pbf, bounds, output, work, cache, imports=(), countries=(), speed_import=None, source=None, pipeline=None):
    from speed_import import SpeedImport
    correction = SpeedImport(speed_import) if speed_import else None
    from global_regions import Regions
    from build_timezones import build as timezones
    from build_cameras import HDR as CHDR, REC as CREC
    from atlas_format import write as write_catalog

    staged = work / 'atlas-result'
    staged.mkdir(parents=True, exist_ok=True)
    west, south, east, north = [round(n * 1e5) for n in bounds]
    grid = []
    for y in range(math.floor(south / STEP) * STEP, north + 1, STEP):
        for x in range(math.floor(west / STEP) * STEP, east + 1, STEP):
            grid.append((x / 1e5, y / 1e5, (x + STEP) / 1e5, (y + STEP) / 1e5))

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

    regions = Regions()
    coverage = None
    if countries:
        from shapely.geometry import box
        from shapely.ops import unary_union
        coverage = unary_union(
            [g for iso, name, g, p in regions.rows if iso in {c.upper() for c in countries}]
        ).buffer(MARGIN)

    initial_jobs = []
    seq = 0
    for core in grid:
        if coverage is not None and not coverage.intersects(box(*core)):
            continue
        seq += 1
        initial_jobs.append(((seq,), core))

    use_pipeline = pipeline_enabled(pipeline)
    extract_slots, build_slots = workers(pipeline=use_pipeline)
    stop = threading.Event()
    tracker = _ProcessTracker(stop)
    extract_gate = threading.Semaphore(extract_slots)
    build_gate = _MemoryGate(build_slots)

    def cancel_all():
        stop.set()
        build_gate.wake_all()
        tracker.terminate_all()

    def run_region(key, core):
        name = region_name(key)
        x, y, xx, yy = core
        extract = work / f'region-{name}.osm.pbf'
        childwork = work / f'work-{name}'
        dest = staged / name
        log = work / f'{name}.log'

        def tidy():
            extract.unlink(missing_ok=True)
            shutil.rmtree(childwork, ignore_errors=True)

        if stop.is_set():
            return None
        if not use_pipeline:
            print(f'Atlas region {name}: ' + ', '.join(f'{v:.5f}' for v in core), flush=True)
        if shutil.disk_usage(work).free < 3 * 1024 ** 3:
            raise RuntimeError(
                'Less than 3 GiB free; atlas build paused before creating another region. '
                'Free scratch space and rerun.'
            )
        with extract_gate:
            if stop.is_set():
                return None
            extract.unlink(missing_ok=True)
            bbox = ','.join(map(str, (
                max(-180, x - MARGIN), max(-85, y - MARGIN),
                min(180, xx + MARGIN), min(85, yy + MARGIN),
            )))
            res = tracker.run(
                [
                    'osmium', 'extract', '--bbox', bbox, '--strategy', 'smart',
                    '-S', 'types=multipolygon,restriction,enforcement',
                    str(pbf), '-o', str(extract), '--overwrite',
                ],
                check=True,
            )
            if res is None or stop.is_set():
                tidy()
                return None
            if extract.stat().st_size <= 100:  # empty PBF header; no valid bbox
                tidy()
                return ('empty', key, core)
            info_res = tracker.run(
                ['osmium', 'fileinfo', '-e', '-j', str(extract)],
                stdout=subprocess.PIPE,
                text=True,
                check=True,
            )
            if info_res is None or stop.is_set():
                tidy()
                return None
            info = json.loads(info_res.stdout)
            if not info['data']['count']['ways']:
                tidy()
                return ('empty', key, core)
            # A successful cached region must match source/core and every output hash.
            stamp = dest / 'atlas-region.json'
            cached = _read_stamp(stamp)
            region_source = sha(extract)
            if cached and (
                cached.get('builder_sha256') != builder_sha
                or cached.get('source') != region_source
                or cached.get('core') != list(core)
                or any(not (dest / n).is_file() or sha(dest / n) != h for n, h in cached['files'].items())
            ):
                cached = None

        if cached:
            tidy()
            return ('ok', key, core, dest, True)

        shutil.rmtree(childwork, ignore_errors=True)
        shutil.rmtree(dest, ignore_errors=True)
        cmd = [
            sys.executable, str(HERE / 'build_navigation.py'), str(extract),
            '--yes', '--single-region', '--atlas-region',
            '--portal-core', *map(str, core),
            '--output', str(dest), '--work', str(childwork), '--cache', str(cache),
        ]
        if correction:
            cmd += ['--speed-import', str(correction.path)]
        for p in imports:
            cmd += ['--camera-import', str(p)]

        if not build_gate.acquire(stop):
            tidy()
            return None
        try:
            if stop.is_set():
                tidy()
                return None
            with log.open('w', encoding='utf-8') as f:
                result = tracker.run(cmd, stdout=f, stderr=subprocess.STDOUT)
        finally:
            build_gate.release()

        if result is None or stop.is_set():
            tidy()
            return None
        if result.returncode:
            text = log.read_text(errors='replace')
            if 'No routable motorcar roads in region' in text:
                shutil.rmtree(dest, ignore_errors=True)
                tidy()
                return ('empty', key, core)
            if '2 GiB' in text or 'too many transfer states' in text:
                sub_cores = split_core(core)
                shutil.rmtree(dest, ignore_errors=True)
                tidy()
                if not sub_cores:
                    raise RuntimeError('Minimum atlas core still exceeds device limits; ' + str(log))
                return ('split', key, [((*key, idx), sub) for idx, sub in enumerate(sub_cores)])
            tidy()
            raise RuntimeError(f'Region build failed; inspect {log}\n' + text[-1800:])

        files = {n: sha(dest / n) for n in (*REGIONAL, 'cameras.cpc', 'route.cpn')}
        _write_stamp(
            dest / 'atlas-region.json',
            {'builder_sha256': builder_sha, 'source': region_source, 'core': list(core), 'files': files},
        )
        tidy()
        return ('ok', key, core, dest, False)

    cameras = {}
    completed = []

    def record_ok(key, core, dest):
        raw = (dest / 'cameras.cpc').read_bytes()
        h = CHDR.unpack_from(raw)
        if h[:3] != (b'CPC1', 2, CREC.size):
            raise ValueError('Camera version mismatch')
        for i in range(h[3]):
            r = CREC.unpack_from(raw, 16 + i * CREC.size)
            cameras[(r[0], r[1], r[2], r[3], r[5], r[8])] = r
        completed.append((key, core, dest))

    if not use_pipeline:
        pending = deque(initial_jobs)
        try:
            while pending:
                unchanged()
                key, core = pending.popleft()
                result = run_region(key, core)
                if result is None:
                    break
                if result[0] == 'split':
                    for item in reversed(result[2]):
                        pending.appendleft(item)
                elif result[0] == 'ok':
                    _, rkey, rcore, dest, _ = result
                    unchanged()
                    record_ok(rkey, rcore, dest)
        except BaseException:
            cancel_all()
            raise
    else:
        remaining_total = len(initial_jobs)
        built_count = 0
        started = time.monotonic()
        first_build_started = None
        print(
            f'Pipeline: {extract_slots} extraction + {build_slots} build workers for {remaining_total} regions',
            flush=True,
        )
        max_workers = max(1, min(max(1, len(initial_jobs)), extract_slots + build_slots))
        pool = ThreadPoolExecutor(max_workers=max_workers)
        futures = set()
        try:
            def submit(job):
                futures.add(pool.submit(run_region, job[0], job[1]))

            for job in initial_jobs:
                submit(job)

            while futures:
                done, _ = wait(futures, return_when=FIRST_COMPLETED)
                for future in done:
                    futures.discard(future)
                    result = future.result()
                    if result is None:
                        continue
                    if result[0] == 'empty':
                        remaining_total = max(len(completed), remaining_total - 1)
                        continue
                    if result[0] == 'split':
                        children = result[2]
                        remaining_total += len(children) - 1
                        for child in children:
                            submit(child)
                        continue
                    _, rkey, rcore, dest, was_cached = result
                    unchanged()
                    record_ok(rkey, rcore, dest)
                    now = time.monotonic()
                    elapsed = now - started
                    if not was_cached:
                        built_count += 1
                        if first_build_started is None:
                            first_build_started = started
                    left = max(0, remaining_total - len(completed))
                    if built_count > 0:
                        eta_s = left * (elapsed / max(1, len(completed)))
                        eta_text = f'~{eta_s / 60:.0f} min left'
                    else:
                        eta_text = 'cached'
                    print(
                        f'[{len(completed)}/{remaining_total}] {dest.name} done in {elapsed / 60:.1f} min; {eta_text}',
                        flush=True,
                    )
        except BaseException:
            cancel_all()
            pool.shutdown(wait=True, cancel_futures=True)
            raise
        else:
            pool.shutdown(wait=True)

    if not completed:
        raise ValueError('No regional road data in this input')
    unchanged()
    metadata = []
    for key, core, dest in sorted(completed, key=lambda item: item[0]):
        intersections = regions.intersecting(core)
        label = '/'.join(code for code, name in intersections) or 'Region'
        metadata.append({'path': dest.name, 'name': region_label(label, key), 'core': list(core)})
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
    manifest = {
        'builder': 'global-atlas-2',
        'speed_import': correction.identity if correction else None,
        'builder_sha256': builder_sha,
        'source': source or {'name': pbf.name, 'sha256': sha(pbf), 'bounds': bounds},
        'atlas': report,
        'timezones': tz,
        'regions': metadata,
        'files': {n: {'bytes': (staged / n).stat().st_size, 'sha256': sha(staged / n)} for n in names},
    }
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
