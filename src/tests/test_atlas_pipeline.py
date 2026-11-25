"""Unit tests for unified sequential/pipelined atlas scheduling and C++ helper cache."""
import json
import os
from pathlib import Path
import re
import stat
import struct
import subprocess
import sys
import tempfile
import threading
import time
import types
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'navigation-builder/tools'))
import build_atlas
import native_helper


class AtlasPipelineTest(unittest.TestCase):
    def test_workers_sizing_and_overrides(self):
        with patch.dict(os.environ, {}, clear=True):
            self.assertEqual(build_atlas.workers(), (1, 1))
            self.assertEqual(build_atlas.workers(pipeline=False), (1, 1))

        cases = [
            (8, 10 * 1024 ** 3, (2, 7)),
            (12, 16 * 1024 ** 3, (4, 11)),
            (16, 32 * 1024 ** 3, (8, 15)),
            (2, 4 * 1024 ** 3, (1, 1)),
        ]
        for cores, ram, expected in cases:
            with patch.dict(os.environ, {'CLIOPLAY_PIPELINE': '1'}, clear=True), \
                 patch.object(build_atlas, 'physical_cores', return_value=cores), \
                 patch.object(build_atlas, 'memory_total', return_value=ram):
                self.assertEqual(build_atlas.workers(), expected)

        with patch.dict(os.environ, {
            'CLIOPLAY_PIPELINE': '1',
            'CLIOPLAY_EXTRACT_JOBS': '3',
            'CLIOPLAY_BUILD_JOBS': '5',
        }, clear=True), \
             patch.object(build_atlas, 'physical_cores', return_value=16), \
             patch.object(build_atlas, 'memory_total', return_value=32 * 1024 ** 3):
            self.assertEqual(build_atlas.workers(), (3, 5))

        with patch.dict(os.environ, {
            'CLIOPLAY_PIPELINE': '1',
            'CLIOPLAY_EXTRACT_JOBS': '0',
            'CLIOPLAY_BUILD_JOBS': 'bad',
        }, clear=True), \
             patch.object(build_atlas, 'physical_cores', return_value=8), \
             patch.object(build_atlas, 'memory_total', return_value=10 * 1024 ** 3):
            self.assertEqual(build_atlas.workers(), (2, 7))

    def test_macos_and_arm_hardware_detection(self):
        vm_stat_sample = (
            'Mach Virtual Memory Statistics: (page size of 16384 bytes)\n'
            'Pages free:                               10000.\n'
            'Pages active:                             50000.\n'
            'Pages inactive:                           20000.\n'
            'Pages speculative:                         5000.\n'
            'Pages purgeable:                          15000.\n'
        )
        def fake_check_output(cmd, **kwargs):
            if cmd == ['sysctl', '-n', 'hw.memsize']:
                return '17179869184\n'
            if cmd == ['sysctl', '-n', 'hw.physicalcpu']:
                return '10\n'
            if cmd == ['vm_stat']:
                return vm_stat_sample
            raise AssertionError(cmd)

        with patch.object(build_atlas.sys, 'platform', 'darwin'), \
             patch('builtins.open', side_effect=FileNotFoundError), \
             patch.object(build_atlas.subprocess, 'check_output', side_effect=fake_check_output):
            self.assertEqual(build_atlas.memory_total(), 17179869184)
            self.assertEqual(build_atlas.physical_cores(), 10)
            self.assertEqual(build_atlas.memory_available(), 50000 * 16384)

        with patch.object(build_atlas.sys, 'platform', 'linux'), \
             patch.object(build_atlas.Path, 'read_text', side_effect=FileNotFoundError), \
             patch.object(build_atlas.Path, 'glob', return_value=[]), \
             patch.object(build_atlas.platform, 'machine', return_value='aarch64'), \
             patch.object(build_atlas.os, 'cpu_count', return_value=8):
            self.assertEqual(build_atlas.physical_cores(), 8)

    def test_deterministic_region_naming_and_splits(self):
        self.assertEqual(build_atlas.region_name((5,)), 'r0005')
        self.assertEqual(build_atlas.region_name((5, 2)), 'r0005_2')
        self.assertEqual(build_atlas.region_label('GB', (5,)), 'GB 5')
        self.assertEqual(build_atlas.region_label('GB', (5, 2)), 'GB 5.2')
        deep = build_atlas.region_name((999, 3, 3, 3, 3))
        self.assertRegex(deep, r'^[a-z0-9_-]{1,23}$')

        subs = build_atlas.split_core((0.0, 0.0, 1.96608, 1.96608))
        self.assertEqual(len(subs), 4)
        self.assertIsNone(build_atlas.split_core((0.0, 0.0, 0.16384, 0.16384)))

        # Out-of-order concurrent completion sorts identically to depth-first sequential order.
        out_of_order = [(3,), (2, 2), (1,), (2, 0), (2, 3), (2, 1)]
        self.assertEqual(sorted(out_of_order), [(1,), (2, 0), (2, 1), (2, 2), (2, 3), (3,)])

    def test_memory_gate_never_deadlocks_and_throttles_extra_workers(self):
        free_mem = [512 * 1024 ** 2]  # below 3 GiB limit
        gate = build_atlas._MemoryGate(
            max_slots=3, limit_bytes=3 * 1024 ** 3,
            avail_fn=lambda: free_mem[0], poll_s=0.05,
        )
        stop = threading.Event()
        # First worker always proceeds even when MemAvailable < limit.
        self.assertTrue(gate.acquire(stop))

        second_acquired = threading.Event()
        def worker2():
            if gate.acquire(stop):
                second_acquired.set()
                gate.release()

        t = threading.Thread(target=worker2)
        t.start()
        time.sleep(0.12)
        self.assertFalse(second_acquired.is_set(), 'Second worker bypassed RAM throttle')

        # Lifting RAM pressure allows the waiting worker through cleanly.
        free_mem[0] = 4 * 1024 ** 3
        self.assertTrue(second_acquired.wait(timeout=1.0))
        t.join(timeout=1.0)
        gate.release()

        # Cancellation wakes waiting workers immediately.
        free_mem[0] = 512 * 1024 ** 2
        self.assertTrue(gate.acquire(stop))
        cancelled_result = []
        def worker3():
            cancelled_result.append(gate.acquire(stop))
        t3 = threading.Thread(target=worker3)
        t3.start()
        time.sleep(0.08)
        stop.set()
        gate.wake_all()
        t3.join(timeout=1.0)
        self.assertEqual(cancelled_result, [False])
        gate.release()

    def test_process_tracker_fail_fast_cancellation(self):
        stop = threading.Event()
        tracker = build_atlas._ProcessTracker(stop)
        started = threading.Event()
        results = []

        def run_sleeper():
            started.set()
            results.append(tracker.run([sys.executable, '-c', 'import time; time.sleep(30)']))

        t = threading.Thread(target=run_sleeper)
        t.start()
        started.wait(timeout=1.0)
        deadline = time.monotonic() + 2.0
        while not tracker.active and time.monotonic() < deadline:
            time.sleep(0.01)
        t0 = time.monotonic()
        stop.set()
        tracker.terminate_all()
        t.join(timeout=3.0)
        self.assertFalse(t.is_alive())
        self.assertLess(time.monotonic() - t0, 3.0)
        # Once stopped, new commands are not launched.
        self.assertIsNone(tracker.run([sys.executable, '-c', 'print("nope")']))

    def test_corrupt_stamp_tolerated_and_atomic_write(self):
        with tempfile.TemporaryDirectory() as td:
            stamp = Path(td) / 'atlas-region.json'
            stamp.write_text('{truncated', encoding='utf-8')
            self.assertIsNone(build_atlas._read_stamp(stamp))
            build_atlas._write_stamp(stamp, {'builder_sha256': 'abc', 'files': {'map.cpm': '123'}})
            self.assertEqual(build_atlas._read_stamp(stamp)['builder_sha256'], 'abc')
            self.assertFalse((Path(td) / 'atlas-region.json.tmp').exists())

    def test_native_helper_private_cache_reuse_and_noexec_fallback(self):
        cc = native_helper.find_compiler()
        with tempfile.TemporaryDirectory() as td:
            base = Path(td) / 'tmp_root'
            fb = Path(td) / 'fallback'
            src = Path(td) / 'mini.cpp'
            base.mkdir()
            fb.mkdir()
            src.write_text('#include <iostream>\nint main(){std::cout<<"ok\\n";return 0;}\n', encoding='utf-8')

            exe1 = native_helper.ensure_compiled('mini', src, ['-O1', '-std=c++17'], cc=cc, cache_base=base, fallback_dir=fb)
            self.assertTrue(exe1.is_file())
            if hasattr(os, 'getuid'):
                self.assertEqual(stat.S_IMODE(exe1.parent.lstat().st_mode) & 0o077, 0)

            # Second call hits cache without compiling again.
            with patch.object(native_helper.subprocess, 'run', side_effect=AssertionError('recompiled')):
                exe2 = native_helper.ensure_compiled('mini', src, ['-O1', '-std=c++17'], cc=cc, cache_base=base, fallback_dir=fb)
            self.assertEqual(exe1, exe2)

            # Compiler identity changes cache key.
            with patch.object(native_helper, 'compiler_identity', return_value='/usr/bin/g++|12.0'):
                k1 = native_helper.cache_key(src, ['-O1'], cc)
            with patch.object(native_helper, 'compiler_identity', return_value='/usr/bin/clang++|18.0'):
                k2 = native_helper.cache_key(src, ['-O1'], cc)
            self.assertNotEqual(k1, k2)

            # Symlink squatting on private cache dir falls back cleanly to fallback_dir.
            squat_base = Path(td) / 'squat_root'
            squat_base.mkdir()
            uid = os.getuid() if hasattr(os, 'getuid') else os.environ.get('USERNAME', 'user')
            (squat_base / f'clioplay-helpers-{uid}').symlink_to(fb)
            resolved = native_helper.private_cache_dir(base_dir=squat_base, fallback_dir=fb)
            self.assertEqual(resolved, fb)

            # Simulated noexec /tmp falls back to cleaned temp dir inside fallback_dir.
            real_run = subprocess.run
            first_exec = [True]
            def flaky_run(cmd, *args, **kwargs):
                if first_exec[0] and Path(cmd[0]).parent == exe1.parent:
                    first_exec[0] = False
                    raise PermissionError(13, 'Permission denied (noexec)')
                return real_run(cmd, *args, **kwargs)

            out_fb = Path(td) / 'out_fb'
            out_fb.mkdir()
            with patch.object(native_helper.subprocess, 'run', side_effect=flaky_run):
                res = native_helper.run_compiled('mini', src, ['-O1', '-std=c++17'], [], cc=cc, cache_base=base, fallback_dir=out_fb)
            self.assertEqual(res.returncode, 0)
            self.assertEqual(list(out_fb.iterdir()), [], 'Fallback temp binary was not cleaned up')

    def test_sequential_and_pipelined_match_across_splits_and_fail_fast(self):
        hdr = struct.Struct('<4sHHII')
        rec = struct.Struct('<iiiiBBBBI')

        class FakeRegions:
            rows = []
            def intersecting(self, core):
                return [('GB', 'United Kingdom')]
            def write(self, path, bounds):
                Path(path).write_bytes(b'REG')
                return {'regions': 1}

        fake_global = types.ModuleType('global_regions')
        fake_global.Regions = FakeRegions
        fake_tz = types.ModuleType('build_timezones')
        fake_tz.build = lambda path, bounds: (Path(path).write_bytes(b'TZ'), {'zones': 1})[1]
        fake_cam = types.ModuleType('build_cameras')
        fake_cam.HDR = hdr
        fake_cam.REC = rec
        fake_af = types.ModuleType('atlas_format')
        def fake_catalog(staged, metadata):
            (Path(staged) / 'atlas.cpa').write_bytes(json.dumps(metadata).encode('utf-8'))
            return {'format': 'CPA1', 'regions': len(metadata), 'portals': 0}
        fake_af.write = fake_catalog

        def run_case(use_pipeline, fail_r2=False):
            started_regions = []
            lock = threading.Lock()
            with tempfile.TemporaryDirectory() as td:
                td = Path(td)
                pbf = td / 'test.osm.pbf'
                pbf.write_bytes(b'fake-pbf')
                work = td / 'work'
                out = td / 'out'
                cache = td / 'cache'
                work.mkdir()
                cache.mkdir()
                # 2x2 grid -> 4 top-level regions (r0001..r0004).
                bounds = (0.0, 0.0, 2.5, 2.5)

                def fake_tracker_run(self_t, cmd, stdout=None, stderr=None, text=False, check=False):
                    if self_t.stop.is_set():
                        return None
                    if cmd[0] == 'osmium' and cmd[1] == 'extract':
                        ext = Path(cmd[cmd.index('-o') + 1])
                        bbox = cmd[cmd.index('--bbox') + 1]
                        ext.write_bytes(b'X' * 120 + bbox.encode('utf-8'))
                        return subprocess.CompletedProcess(cmd, 0, '', '')
                    if cmd[0] == 'osmium' and cmd[1] == 'fileinfo':
                        ext = Path(cmd[-1])
                        # Make r0003 empty.
                        ways = 0 if 'region-r0003.osm.pbf' in ext.name else 10
                        return subprocess.CompletedProcess(cmd, 0, json.dumps({'data': {'count': {'ways': ways}}}), '')
                    # Child build_navigation.py
                    dest = Path(cmd[cmd.index('--output') + 1])
                    rname = dest.name
                    with lock:
                        started_regions.append(rname)
                    if use_pipeline and rname == 'r0001':
                        time.sleep(0.08)  # finish r0001 after r0004 to force out-of-order completion
                    if fail_r2 and rname == 'r0002':
                        stdout.write('fatal synthetic failure\n')
                        return subprocess.CompletedProcess(cmd, 1, None, None)
                    if rname == 'r0002':
                        stdout.write('Routing pack exceeds device 2 GiB seek limit\n')
                        return subprocess.CompletedProcess(cmd, 1, None, None)
                    dest.mkdir(parents=True, exist_ok=True)
                    for fn in ('map.cpm', 'route.cpr', 'route.qadj', 'route.cpn'):
                        (dest / fn).write_bytes(f'{rname}:{fn}'.encode('utf-8'))
                    cam_raw = hdr.pack(b'CPC1', 2, rec.size, 0, hdr.size)
                    (dest / 'cameras.cpc').write_bytes(cam_raw)
                    return subprocess.CompletedProcess(cmd, 0, None, None)

                with patch.dict(sys.modules, {
                    'global_regions': fake_global,
                    'build_timezones': fake_tz,
                    'build_cameras': fake_cam,
                    'atlas_format': fake_af,
                }), patch.object(build_atlas._ProcessTracker, 'run', fake_tracker_run), \
                     patch.object(build_atlas, 'workers', return_value=(2, 4) if use_pipeline else (1, 1)), \
                     patch.object(build_atlas, 'memory_available', return_value=8 * 1024 ** 3):
                    manifest = build_atlas.build(pbf, bounds, out, work, cache, pipeline=use_pipeline)
                    return manifest['regions'], (out / 'atlas.cpa').read_bytes(), started_regions

        seq_regions, seq_cpa, _ = run_case(False)
        pipe_regions, pipe_cpa, _ = run_case(True)
        # r0001, r0002 split into 4 children r0002_0..r0002_3, r0003 empty, r0004 => 6 regions in exact order.
        self.assertEqual([r['path'] for r in seq_regions], ['r0001', 'r0002_0', 'r0002_1', 'r0002_2', 'r0002_3', 'r0004'])
        self.assertEqual(seq_regions, pipe_regions)
        self.assertEqual(seq_cpa, pipe_cpa)

        with self.assertRaisesRegex(RuntimeError, 'Region build failed'):
            run_case(True, fail_r2=True)


if __name__ == '__main__':
    unittest.main()
