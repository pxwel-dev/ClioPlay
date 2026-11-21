"""Unit and C-binary-parity tests for tools/replay_history.py."""
from pathlib import Path
import io
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import replay_history as rh


def make_synthetic_cpr2(path: Path) -> tuple[int, int]:
    """Build a minimal valid CPR2 v5 pack with one primary arc (cls=3) heading north."""
    # Geometry points: (5497000, -161000) -> (5497300, -161000) -> (5497600, -161000)
    pts = [(5497000, -161000), (5497300, -161000), (5497600, -161000)]
    # Z6 tiles covering those points: tx = -161000 >> 8 = -629, ty in {21472, 21473, 21474, 21475}
    tiles_xy = sorted({(lon >> 8, lat >> 8) for lat, lon in pts} | {(-161000 >> 8, y) for y in range(21472, 21476)})
    tile_off = rh.CPR_HDR_STRUCT.size
    tref_off = tile_off + len(tiles_xy) * rh.CPR_TILE_STRUCT.size
    trefs = [0] * len(tiles_xy)
    node_off = tref_off + len(trefs) * 4
    edge_off = node_off
    base_off = edge_off
    src_off = base_off + rh.CPR_BASE_STRUCT.size
    arc_off = src_off
    upf_off = arc_off
    upr_off = upf_off
    geom_off = upr_off

    hdr = rh.CPR_HDR_STRUCT.pack(
        b'CPR2', 5, 2,  # magic, ver=5, flags=2 (has way IDs)
        0, 0, 1, 0,     # nodes, phys_nodes, base_edges=1, route_edges
        len(tiles_xy), len(trefs), 0, 0, 0, len(pts), 0,
        tile_off, tref_off, node_off, edge_off, base_off, src_off, arc_off, upf_off, upr_off, geom_off,
        5496000, -162000, 5499000, -160000,
    )
    tile_bytes = b''.join(rh.CPR_TILE_STRUCT.pack(tx, ty, i, 1) for i, (tx, ty) in enumerate(tiles_xy))
    tref_bytes = struct.pack(f'<{len(trefs)}I', *trefs)
    # cost=240 ds (24 s nominal), geom_first=0, src_first=0, to_state=0, geom_count=3, src_count=0, cls=3, flags=0, pad=0
    base_bytes = rh.CPR_BASE_STRUCT.pack(240, 0, 0, 0, len(pts), 0, 3, 0, 0)
    geom_bytes = b''.join(rh.CPR_PT_STRUCT.pack(la, lo) for la, lo in pts)
    way_bytes = rh.CPR_WAY_STRUCT.pack(10001, 20002)
    path.write_bytes(hdr + tile_bytes + tref_bytes + base_bytes + geom_bytes + way_bytes)
    payload = struct.pack('<IIiiiiii', 10001, 20002, pts[0][0], pts[0][1], pts[1][0], pts[1][1], pts[2][0], pts[2][1])
    expected_key = rh.fnv1a(payload) or 1
    return expected_key, 240


def pack_record(kind: int, epoch: int, elapsed: int, lat: int, lon: int, mph10: int, heading: int) -> bytes:
    body = struct.pack('<IIIiiHH', kind, epoch, elapsed, lat, lon, mph10, heading)
    return body + struct.pack('<I', rh.fnv1a(body))


def make_trip_records(depart: int, lat_start: int, lat_end: int, duration_s: int,
                      brake_at: int = -1, lat_jitter: int = 0) -> bytes:
    out = bytearray()
    out += pack_record(rh.MOB_BEGIN, rh.MOB_UNKNOWN, 0, lat_start + lat_jitter, -161000, 0, 0)
    out += pack_record(rh.MOB_SAMPLE, depart - 2, 1000, lat_start + lat_jitter, -161000, 0, 0)
    steps = duration_s
    for i in range(steps + 1):
        lat = lat_start + ((lat_end - lat_start) * i) // steps
        if i == 0 or i == steps:
            lat += lat_jitter
        mph10 = 260
        if brake_at >= 0 and i == brake_at:
            mph10 = 80  # 18.0 mph/s deceleration drop
        elif brake_at >= 0 and i == brake_at - 1:
            mph10 = 260
        heading = 0 if lat_end >= lat_start else 180
        out += pack_record(rh.MOB_SAMPLE, depart + i, 2000 + i * 1000, lat, -161000, mph10, heading)
    out += pack_record(rh.MOB_END, depart + steps, 2000 + steps * 1000, lat_end + lat_jitter, -161000, 0, 0)
    return bytes(out)


class ReplayHistoryTest(unittest.TestCase):
    def test_dbscan_clusters_jitter_and_preserves_noise(self):
        pts = [
            (5497000, -161000), (5497015, -161005), (5496990, -160995),  # cluster A
            (5498500, -161000), (5498512, -161004),                      # cluster B
            (5502000, -165000),                                          # noise
        ]
        denoised, count = rh.dbscan_endpoints(pts, eps_m=95, min_pts=2)
        self.assertEqual(count, 2)
        self.assertEqual(denoised[0], denoised[1])
        self.assertEqual(denoised[1], denoised[2])
        self.assertEqual(denoised[3], denoised[4])
        self.assertEqual(denoised[5], (5502000, -165000))

    def test_replay_and_c_binary_parity(self):
        with tempfile.TemporaryDirectory(prefix='clioplay-replay-') as tmp:
            t = Path(tmp)
            (t / 'clio843').mkdir()
            cpr = t / 'route.cpr'
            expected_key, _ = make_synthetic_cpr2(cpr)

            # Build 4 chained commute days:
            # Leg A: Home (5496980) -> Coffee Stop (5497620) traversing the CPR2 arc with varying speed + hard braking
            # Leg B (12 min later, <= 45 min dwell): Coffee Stop (5497620) -> Work (5498600)
            hist_data = bytearray()
            base_epoch = 315792000  # Monday 08:00 UTC in 2000-epoch
            durations = [70, 95, 72, 105]
            for d, dur in enumerate(durations):
                day_ep = base_epoch + d * 7 * 86400
                jit = (d % 3) * 12  # ~13-26 m GPS drift, cleaned by DBSCAN
                hist_data += make_trip_records(day_ep, 5496980, 5497620, dur, brake_at=dur // 2, lat_jitter=jit)
                hist_data += make_trip_records(day_ep + dur + 720, 5497620, 5498600, 65, lat_jitter=jit)
            # Append a corrupted 28-byte record to ensure checksum filtering works
            hist_data += b'\xde\xad\xbe\xef' * 7
            hist_path = t / 'history.dat'
            hist_path.write_bytes(bytes(hist_data))

            # Seed an existing mobility-ml.dat with UI preferences to verify preservation across map rebuilds
            old_ml = bytearray(rh.MOB_ML_SIZE)
            struct.pack_into('<II', old_ml, 0, rh.MOB_ML_MAGIC, rh.MOB_ML_VER)
            for i in range(rh.MOB_ML_CLASSES):
                struct.pack_into('<IHHHH', old_ml, 32 + i * 12, 256, 0, 256, 0, 0)
            tail_off = rh.MOB_ML_SIZE - 12
            # zoom_bias_q4=[12, 0, -12], zoom_samples=[4, 0, 4], bright_day=90, bright_night=30, n=3, 3
            struct.pack_into('<3b3B6B', old_ml, tail_off, 12, 0, -12, 4, 0, 4, 90, 30, 3, 3, 0, 0)
            (t / 'mobility-ml.dat').write_bytes(bytes(old_ml) + struct.pack('<I', rh.fnv1a(old_ml)))

            # Run CLI with --verify and verify stdout never exposes raw coordinates
            buf = io.StringIO()
            with redirect_stdout(buf):
                rc = rh.main(['--history', str(hist_path), '--maps', str(cpr), '--out-dir', str(t), '--verify'])
            self.assertEqual(rc, 0)
            out = buf.getvalue()
            self.assertIn('journeys: 8', out)
            self.assertIn('corrupt_records: 1', out)
            self.assertIn('preserved_ui_profile: 1', out)
            for secret in ('5496980', '5497620', '5498600', '-161000'):
                self.assertNotIn(secret, out)

            # Compile a real C UBSan harness linking clio040.c to verify 100% binary parity!
            harness = t / 'parity.c'
            harness.write_text(f'''
#define HOST_TEST 1
#define main unused_device_entry
#include "{(ROOT / 'clio843/src/clio040.c').as_posix()}"
#undef main
#include <assert.h>
int main(void){{
 pref.learn_history=1;pref.destination_predictions=1;
 mob_load();ml_load();
 assert(mob.n==8&&mob.serial==8);
 u32 key={expected_key}u;
 unsigned b=mob_bucket({base_epoch}u);
 struct MobRoad*r=mob_road_find(key,b,0);
 assert(r&&r->count==4&&r->mean_ds>240u);
 assert(mob_ml.road_cls[3].count==4&&mob_ml.road_cls[3].ratio_q8>256u);
 assert(ml_class_factor_q8(3,0,0)>256u);
 assert(ml_pref_score(key)>=40);
 assert(ml_road_mad(key,{base_epoch}u)>0u);
 assert(ml_hazard_level(key,0)==1);
 /* Trip chain & recurring intermediate stop habit learned from 4 chained legs */
 int found_chain=0,found_stop=0;
 for(int i=0;i<MOB_ML_CHAINS;i++)if(mob_ml.chains[i].count>=3)found_chain=1;
 for(int i=0;i<MOB_ML_STOPS;i++)if(mob_ml.stops[i].count>=3)found_stop=1;
 assert(found_chain&&found_stop);
 /* Preserved UI preferences survived replay */
 assert(ml_autozoom_offset(100)==1&&ml_autozoom_offset(500)==-1);
 assert(ml_learned_brightness(0)==90&&ml_learned_brightness(1)==30);
 return 0;
}}
''')
            exe = t / 'parity'
            subprocess.run([
                'cc', '-O1', '-g', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                f'-DCLIO_HOST_ROOT="{t.as_posix()}/"', str(harness), '-lm', '-o', str(exe),
            ], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()
