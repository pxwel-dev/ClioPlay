#!/usr/bin/env python3
"""Offline ClioPlay driving-history replay and map-rebuild migration tool.

Reads checksummed ClioPlay raw driving logs (history.prev.dat, history.dat),
denoises journey endpoints with pure-Python DBSCAN spatial clustering, snaps
1 Hz traces onto a single CPR2 v5 route.cpr or a multi-region atlas bundle to
compute map-independent traffic_key fingerprints, and writes atomic FNV-1a
checksummed mobility.dat (MOB1) and mobility-ml.dat (MML1) models.

Privacy: operates strictly locally with Python stdlib only and never prints
raw GPS coordinates or timestamps.
"""
from __future__ import annotations

import argparse
import bisect
import math
import os
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

# Binary protocol constants (must stay ABI-identical to mobility.h / mobility_ml.h)
MOB_MAGIC = 0x31424F4D       # MOB1
MOB_ML_MAGIC = 0x314C4D4D    # MML1
MOB_ML_VER = 1
MOB_BEGIN = 0x3142474D       # MGB1
MOB_SAMPLE = 0x3153474D      # MGS1
MOB_END = 0x3145474D         # MGE1
MOB_UNKNOWN = 0xFFFFFFFF

MOB_JOURNEYS = 512
MOB_TRACE = 32
MOB_TRAFFIC = 8192
MOB_ML_CLASSES = 8
MOB_ML_CHAINS = 128
MOB_ML_STOPS = 32
MOB_ML_PREF_ARCS = 512
MOB_ML_ROAD_VAR = 1024
MOB_ML_POI_AFF = 32
MOB_ML_PARKING = 32
MOB_ML_HAZARDS = 128

REC_STRUCT = struct.Struct('<IIIiiHHI')
CPR_HDR_STRUCT = struct.Struct('<4sHH21I4i')
CPR_TILE_STRUCT = struct.Struct('<iiII')
CPR_BASE_STRUCT = struct.Struct('<IIIIHHBBH')
CPR_PT_STRUCT = struct.Struct('<ii')
CPR_WAY_STRUCT = struct.Struct('<II')
CPR_ARC_GEOM_REVERSED = 2
CPR_BASE_ROUNDABOUT = 4

MOB_MODEL_SIZE = 280592
MOB_ML_SIZE = 26508


def fnv1a(data: bytes, h: int = 2166136261) -> int:
    for b in data:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def point_ok(lat: int, lon: int) -> bool:
    return -9000000 <= lat <= 9000000 and -18000000 <= lon <= 18000000


def coslat_q15(lat: int) -> int:
    rad = math.radians(lat / 100000.0)
    c = int(round(math.cos(rad) * 32768.0))
    return max(0, min(32768, c))


def lon_delta(d: int) -> int:
    while d > 18000000:
        d -= 36000000
    while d < -18000000:
        d += 36000000
    return d


def lon_scale(dlon: int, cq: int) -> int:
    return int((dlon * cq) / 32768)


def geo_seg_m(la0: int, lo0: int, la1: int, lo1: int) -> int:
    if abs(la0 - la1) > 1000000 or abs(lon_delta(lo0 - lo1)) > 1000000:
        return 1000000
    cq = coslat_q15((la0 + la1) // 2)
    dx = abs(lon_scale(lon_delta(lo1 - lo0), cq))
    dy = abs(la1 - la0)
    hi = dx if dx > dy else dy
    lo = dy if dx > dy else dx
    q = hi + ((lo * 3) >> 3)
    return (q * 111 + 50) // 100


def mob_day(epoch: int) -> int:
    return (epoch // 86400) % 7


def mob_bucket(epoch: int) -> int:
    return mob_day(epoch) * 12 + (epoch % 86400) // 7200


def dwell_bucket(dwell_sec: int) -> int:
    if dwell_sec < 900:
        return 0
    if dwell_sec < 5400:
        return 1
    if dwell_sec < 18000:
        return 2
    return 3


def heading_deg(la0: int, lo0: int, la1: int, lo1: int) -> int:
    cq = coslat_q15((la0 + la1) // 2)
    x = lon_scale(lon_delta(lo1 - lo0), cq)
    y = la1 - la0
    if x == 0 and y == 0:
        return -1
    ang = math.degrees(math.atan2(x, y))
    if ang < 0:
        ang += 360.0
    return int(round(ang)) % 360


def heading_delta(a: int, b: int) -> int:
    d = abs(a - b) % 360
    return 360 - d if d > 180 else d


def tile_div_z6(v: int) -> int:
    # Z6 tile span is 256 in 1e-5 degree units (floor division).
    return v >> 8


def project_seg(ax: int, ay: int, bx: int, by: int) -> Tuple[int, int]:
    """Return (d2, frac_q16) of origin (0,0) onto segment (a->b)."""
    vx = bx - ax
    vy = by - ay
    len2 = vx * vx + vy * vy
    if len2 <= 0:
        return ax * ax + ay * ay, 0
    dot = -(ax * vx + ay * vy)
    if dot <= 0:
        return ax * ax + ay * ay, 0
    if dot >= len2:
        return bx * bx + by * by, 65536
    q = (dot * 65536) // len2
    px = ax + (vx * q) // 65536
    py = ay + (vy * q) // 65536
    return px * px + py * py, q


def seg_units(la0: int, lo0: int, la1: int, lo1: int) -> int:
    cq = coslat_q15((la0 + la1) // 2)
    dx = abs(lon_scale(lon_delta(lo1 - lo0), cq))
    dy = abs(la1 - la0)
    hi = dx if dx > dy else dy
    lo = dy if dx > dy else dx
    return hi + ((lo * 3) >> 3)


@dataclass
class RawSample:
    epoch: int
    elapsed: int
    lat: int
    lon: int
    mph10: int
    heading: int


@dataclass
class RawSession:
    origin: Tuple[int, int]
    samples: List[RawSample] = field(default_factory=list)
    completed: bool = False
    end_epoch: int = MOB_UNKNOWN
    end_point: Tuple[int, int] = (0, 0)


def parse_history_files(paths: Sequence[Path]) -> Tuple[List[RawSession], int, int]:
    """Read raw history files, verifying every 28-byte record's FNV-1a checksum."""
    sessions: List[RawSession] = []
    current: Optional[RawSession] = None
    valid_records = 0
    corrupt_records = 0

    for path in paths:
        if not path.is_file():
            continue
        raw = path.read_bytes()
        usable = len(raw) - (len(raw) % REC_STRUCT.size)
        for off in range(0, usable, REC_STRUCT.size):
            chunk = raw[off:off + REC_STRUCT.size]
            kind, epoch, elapsed, lat, lon, mph10, heading, check = REC_STRUCT.unpack(chunk)
            if fnv1a(chunk[:-4]) != check:
                corrupt_records += 1
                continue
            valid_records += 1
            if kind == MOB_BEGIN:
                if current is not None:
                    sessions.append(current)
                orig = (lat, lon) if point_ok(lat, lon) else (0, 0)
                current = RawSession(origin=orig)
            elif kind == MOB_SAMPLE:
                if current is None:
                    current = RawSession(origin=(lat, lon) if point_ok(lat, lon) else (0, 0))
                if point_ok(lat, lon):
                    current.samples.append(RawSample(epoch, elapsed, lat, lon, mph10, heading))
            elif kind == MOB_END:
                if current is not None and point_ok(lat, lon):
                    current.completed = True
                    current.end_epoch = epoch
                    current.end_point = (lat, lon)
                    sessions.append(current)
                    current = None
    if current is not None:
        sessions.append(current)
    return sessions, valid_records, corrupt_records


def dbscan_endpoints(points: Sequence[Tuple[int, int]], eps_m: int = 95, min_pts: int = 2) -> Tuple[List[Tuple[int, int]], int]:
    """Pure-Python DBSCAN spatial clustering returning denoised point per input and cluster count."""
    n = len(points)
    if n == 0:
        return [], 0
    labels = [-2] * n  # -2 = unvisited, -1 = noise, >= 0 = cluster id
    cid = 0

    def region_query(idx: int) -> List[int]:
        la, lo = points[idx]
        out = []
        for j in range(n):
            pla, plo = points[j]
            if abs(la - pla) <= 180 and abs(lon_delta(lo - plo)) <= 350:
                if geo_seg_m(la, lo, pla, plo) <= eps_m:
                    out.append(j)
        return out

    for i in range(n):
        if labels[i] != -2:
            continue
        nbrs = region_query(i)
        if len(nbrs) < min_pts:
            labels[i] = -1
            continue
        labels[i] = cid
        queue = list(nbrs)
        qi = 0
        while qi < len(queue):
            j = queue[qi]
            qi += 1
            if labels[j] == -1:
                labels[j] = cid
            if labels[j] != -2:
                continue
            labels[j] = cid
            jnbrs = region_query(j)
            if len(jnbrs) >= min_pts:
                queue.extend(jnbrs)
        cid += 1

    # Compute medoid / integer centroid per cluster
    clusters: Dict[int, List[int]] = {}
    for i, c in enumerate(labels):
        if c >= 0:
            clusters.setdefault(c, []).append(i)

    canonical: Dict[int, Tuple[int, int]] = {}
    for c, idxs in clusters.items():
        avg_lat = sum(points[i][0] for i in idxs) // len(idxs)
        avg_lon = sum(points[i][1] for i in idxs) // len(idxs)
        # Pick the real observed point closest to the cluster mean so canonical endpoints stay on-road.
        best_i = min(idxs, key=lambda i: geo_seg_m(points[i][0], points[i][1], avg_lat, avg_lon))
        canonical[c] = points[best_i]

    denoised = [canonical[labels[i]] if labels[i] >= 0 else points[i] for i in range(n)]
    return denoised, cid


class CprPack:
    """Lazy reader for a single CPR2 v5 route.cpr file."""

    def __init__(self, path: Path):
        self.path = path
        self._fp = path.open('rb')
        hdr = CPR_HDR_STRUCT.unpack(self._fp.read(CPR_HDR_STRUCT.size))
        if hdr[0] != b'CPR2' or hdr[1] != 5:
            self._fp.close()
            raise ValueError(f'Unsupported route pack format in {path}')
        self.flags = hdr[2]
        self.base_edges = hdr[5]
        self.tiles_n = hdr[7]
        self.geom_points = hdr[12]
        self.tile_off = hdr[14]
        self.tref_off = hdr[15]
        self.base_off = hdr[18]
        self.geom_off = hdr[23]
        self.minlat, self.minlon, self.maxlat, self.maxlon = hdr[24:28]
        self.has_ways = bool(self.flags & 2)
        self._tiles: Optional[Dict[Tuple[int, int], Tuple[int, int]]] = None
        self._base_cache: Dict[int, Tuple[int, int, int, int, int, int, int, int]] = {}
        self._geom_cache: Dict[int, List[Tuple[int, int]]] = {}
        self._key_cache: Dict[int, int] = {}

    def close(self) -> None:
        self._fp.close()

    def covers(self, lat: int, lon: int) -> bool:
        return (self.minlat - 500 <= lat <= self.maxlat + 500 and
                self.minlon - 500 <= lon <= self.maxlon + 500)

    def _ensure_tiles(self) -> Dict[Tuple[int, int], Tuple[int, int]]:
        if self._tiles is None:
            self._fp.seek(self.tile_off)
            raw = self._fp.read(self.tiles_n * CPR_TILE_STRUCT.size)
            tiles: Dict[Tuple[int, int], Tuple[int, int]] = {}
            for off in range(0, len(raw), CPR_TILE_STRUCT.size):
                tx, ty, first, count = CPR_TILE_STRUCT.unpack_from(raw, off)
                tiles[(tx, ty)] = (first, count)
            self._tiles = tiles
        return self._tiles

    def tile_arcs(self, tx: int, ty: int) -> List[int]:
        entry = self._ensure_tiles().get((tx, ty))
        if not entry:
            return []
        first, count = entry
        self._fp.seek(self.tref_off + first * 4)
        raw = self._fp.read(count * 4)
        return list(struct.unpack(f'<{count}I', raw))

    def read_base(self, arc_id: int) -> Optional[Tuple[int, int, int, int, int, int, int, int]]:
        if arc_id >= self.base_edges:
            return None
        cached = self._base_cache.get(arc_id)
        if cached is not None:
            return cached
        self._fp.seek(self.base_off + arc_id * CPR_BASE_STRUCT.size)
        raw = self._fp.read(CPR_BASE_STRUCT.size)
        if len(raw) != CPR_BASE_STRUCT.size:
            return None
        vals = CPR_BASE_STRUCT.unpack(raw)[:8]  # cost, geom_first, src_first, to_state, geom_count, src_count, cls, flags
        self._base_cache[arc_id] = vals
        return vals

    def read_geom(self, arc_id: int) -> List[Tuple[int, int]]:
        cached = self._geom_cache.get(arc_id)
        if cached is not None:
            return cached
        base = self.read_base(arc_id)
        if base is None:
            return []
        _, geom_first, _, _, geom_count, _, _, flags = base
        if geom_count < 2 or geom_first + geom_count > self.geom_points:
            return []
        self._fp.seek(self.geom_off + geom_first * CPR_PT_STRUCT.size)
        raw = self._fp.read(geom_count * CPR_PT_STRUCT.size)
        pts = [CPR_PT_STRUCT.unpack_from(raw, i * 8) for i in range(geom_count)]
        if flags & CPR_ARC_GEOM_REVERSED:
            pts.reverse()
        self._geom_cache[arc_id] = pts
        return pts

    def traffic_key(self, arc_id: int) -> int:
        cached = self._key_cache.get(arc_id)
        if cached is not None:
            return cached
        if not self.has_ways:
            self._key_cache[arc_id] = 0
            return 0
        pts = self.read_geom(arc_id)
        if len(pts) < 2:
            self._key_cache[arc_id] = 0
            return 0
        way_off = self.geom_off + self.geom_points * 8 + arc_id * 8
        self._fp.seek(way_off)
        raw = self._fp.read(8)
        if len(raw) != 8:
            self._key_cache[arc_id] = 0
            return 0
        w0, w1 = CPR_WAY_STRUCT.unpack(raw)
        p0 = pts[0]
        p1 = pts[len(pts) // 2]
        p2 = pts[-1]
        payload = struct.pack('<IIiiiiii', w0, w1, p0[0], p0[1], p1[0], p1[1], p2[0], p2[1])
        h = fnv1a(payload)
        key = h if h else 1
        self._key_cache[arc_id] = key
        return key

    def snap(self, lat: int, lon: int, speed_mph10: int, heading: int, active_arc: int = -1) -> Optional[Tuple[int, int, int, int]]:
        """Snap a 1 Hz GPS sample to an unambiguous directed base arc.
        Returns (arc_id, along_u, total_u, d2) or None.
        """
        if not self.covers(lat, lon):
            return None
        tx, ty = tile_div_z6(lon), tile_div_z6(lat)
        arcs = self.tile_arcs(tx, ty)
        if not arcs:
            return None
        cq = coslat_q15(lat)
        best = None
        best_d2 = 626
        second_d2 = 0xFFFFFFFF
        seen = set()
        for arc_id in arcs:
            if arc_id in seen:
                continue
            seen.add(arc_id)
            pts = self.read_geom(arc_id)
            if len(pts) < 2:
                continue
            b_d2 = 0xFFFFFFFF
            b_seg = 0
            b_frac = 0
            b_prefix = 0
            b_su = 0
            total = 0
            for i in range(1, len(pts)):
                la0, lo0 = pts[i - 1]
                la1, lo1 = pts[i]
                ax = lon_scale(lon_delta(lo0 - lon), cq)
                ay = la0 - lat
                bx = lon_scale(lon_delta(lo1 - lon), cq)
                by = la1 - lat
                d2, q = project_seg(ax, ay, bx, by)
                su = seg_units(la0, lo0, la1, lo1)
                if d2 < b_d2:
                    b_d2 = d2
                    b_seg = i - 1
                    b_frac = q
                    b_prefix = total
                    b_su = su
                total += su
            if b_d2 > 625 or total < 35 or total > 100000:
                continue
            if speed_mph10 >= 32:
                h = heading_deg(pts[b_seg][0], pts[b_seg][1], pts[b_seg + 1][0], pts[b_seg + 1][1])
                if h < 0 or heading_delta(h, heading) > 45:
                    continue
            elif arc_id != active_arc:
                continue
            along = min(total, b_prefix + ((b_su * b_frac) >> 16))
            if b_d2 < best_d2:
                second_d2 = best_d2
                best_d2 = b_d2
                best = (arc_id, along, total, b_d2)
            elif b_d2 < second_d2:
                second_d2 = b_d2
        if best is None:
            return None
        if second_d2 != 0xFFFFFFFF and second_d2 <= best[3] + 16 and best[0] != active_arc:
            return None
        return best


def discover_route_packs(maps_path: Optional[Path]) -> List[CprPack]:
    if maps_path is None:
        return []
    candidates: List[Path] = []
    if maps_path.is_file():
        candidates.append(maps_path)
    elif maps_path.is_dir():
        direct = maps_path / 'route.cpr'
        if direct.is_file():
            candidates.append(direct)
        else:
            for sub in sorted(maps_path.iterdir()):
                rp = sub / 'route.cpr'
                if sub.is_dir() and rp.is_file():
                    candidates.append(rp)
    packs: List[CprPack] = []
    for c in candidates:
        packs.append(CprPack(c))
    return packs


def pick_pack(packs: Sequence[CprPack], lat: int, lon: int) -> Optional[CprPack]:
    for p in packs:
        if p.covers(lat, lon):
            return p
    return None


def read_existing_ml_ui(path: Path) -> Optional[bytes]:
    """Read and validate an existing mobility-ml.dat so UI prefs/POI affinity/parking can be preserved."""
    if not path.is_file():
        return None
    raw = path.read_bytes()
    if len(raw) != MOB_ML_SIZE + 4:
        return None
    body, check = raw[:-4], struct.unpack('<I', raw[-4:])[0]
    if fnv1a(body) != check:
        return None
    magic, ver = struct.unpack_from('<II', body, 0)
    if magic != MOB_ML_MAGIC or ver != MOB_ML_VER:
        return None
    return body


@dataclass
class JourneyOut:
    origin: Tuple[int, int]
    dest: Tuple[int, int]
    depart: int
    serial: int
    distance: int
    duration: int
    trace: List[Tuple[int, int]]


def replay(
    history_paths: Sequence[Path],
    maps_path: Optional[Path],
    existing_ml_path: Optional[Path] = None,
) -> Tuple[bytes, bytes, Dict[str, int]]:
    raw_sessions, valid_recs, corrupt_recs = parse_history_files(history_paths)
    packs = discover_route_packs(maps_path)
    try:
        # 1. Extract valid completed journeys matching on-device mobility.h thresholds (>= 150 m, >= 60 s).
        extracted = []
        for sess in raw_sessions:
            if not sess.samples:
                continue
            origin = sess.origin if point_ok(*sess.origin) and sess.origin != (0, 0) else (sess.samples[0].lat, sess.samples[0].lon)
            if geo_seg_m(origin[0], origin[1], sess.samples[0].lat, sess.samples[0].lon) > 100:
                origin = (sess.samples[0].lat, sess.samples[0].lon)
            started = False
            motion_n = 0
            motion_ms = 0
            motion_epoch = MOB_UNKNOWN
            motion_origin = origin
            depart_ms = 0
            depart_epoch = MOB_UNKNOWN
            dist_m = 0
            trace: List[Tuple[int, int]] = []
            prev: Optional[RawSample] = None
            dest = origin
            duration = 0

            def add_trace(pt: Tuple[int, int]) -> None:
                if len(trace) < MOB_TRACE and (not trace or geo_seg_m(trace[-1][0], trace[-1][1], pt[0], pt[1]) >= 75):
                    trace.append(pt)

            for smp in sess.samples:
                pt = (smp.lat, smp.lon)
                if prev is not None and smp.elapsed - prev.elapsed < 850:
                    continue
                gap = smp.elapsed - prev.elapsed if prev is not None else 0
                step_m = geo_seg_m(prev.lat, prev.lon, smp.lat, smp.lon) if prev is not None else 0
                continuous = prev is not None and gap <= 3000 and step_m <= 200
                if not started:
                    if smp.mph10 >= 32:
                        if motion_n == 0 or not continuous:
                            motion_ms = smp.elapsed
                            motion_epoch = smp.epoch
                            motion_origin = (prev.lat, prev.lon) if prev is not None else pt
                            motion_n = 1
                        else:
                            motion_n = min(100, motion_n + 1)
                    else:
                        motion_n = 0
                    if motion_n >= 5 and smp.elapsed - motion_ms >= 5000 and geo_seg_m(origin[0], origin[1], pt[0], pt[1]) >= 50:
                        started = True
                        depart_ms = motion_ms
                        depart_epoch = motion_epoch
                        if depart_epoch == MOB_UNKNOWN and smp.epoch != MOB_UNKNOWN and smp.epoch >= (smp.elapsed - depart_ms) // 1000:
                            depart_epoch = smp.epoch - (smp.elapsed - depart_ms) // 1000
                        dist_m = geo_seg_m(origin[0], origin[1], pt[0], pt[1])
                        add_trace(origin)
                        add_trace(motion_origin)
                else:
                    if continuous and smp.mph10 >= 10:
                        dist_m = min(0xFFFFFFFF, dist_m + step_m)
                if started:
                    if depart_epoch == MOB_UNKNOWN and smp.epoch != MOB_UNKNOWN and smp.epoch >= (smp.elapsed - depart_ms) // 1000:
                        depart_epoch = smp.epoch - (smp.elapsed - depart_ms) // 1000
                    add_trace(pt)
                    dest = pt
                    duration = (smp.elapsed - depart_ms) // 1000
                prev = smp

            if started and dist_m >= 150 and duration >= 60:
                extracted.append((origin, dest, depart_epoch, dist_m, duration, trace, sess.samples))

        # 2. Pure-Python DBSCAN denoising across all origins and destinations.
        all_pts: List[Tuple[int, int]] = []
        for origin, dest, *_ in extracted:
            all_pts.append(origin)
            all_pts.append(dest)
        denoised, cluster_count = dbscan_endpoints(all_pts, eps_m=95, min_pts=2)

        # 3. Replay into bounded MOB1 + MML1 structures.
        journeys: List[Optional[JourneyOut]] = [None] * MOB_JOURNEYS
        j_head = 0
        j_n = 0
        serial = 0

        # MOB1 road hash table: key, stamp, mean_ds, bucket, count
        road_table: List[List[int]] = [[0, 0, 0, 0, 0] for _ in range(MOB_TRAFFIC)]
        # MML1 tables
        road_cls: List[List[int]] = [[256, 0, 256, 0] for _ in range(MOB_ML_CLASSES)]  # ratio_q8, count, cold_q8, cold_n
        chains: List[List[int]] = []    # [p_lat, p_lon, o_lat, o_lon, d_lat, d_lon, dwell_b, count, stamp]
        stops: List[List] = []          # [o_lat, o_lon, d_lat, d_lon, s_lat, s_lon, count, day_mask, depart_min, stamp, name_bytes]
        pref_arcs: List[List[int]] = [[0, 0, 0] for _ in range(MOB_ML_PREF_ARCS)]  # key, score, stamp
        road_var: List[List[int]] = [[0, 0, 0, 0, 0] for _ in range(MOB_ML_ROAD_VAR)]  # key, bucket, mad_ds, count, stamp
        hazards: List[List[int]] = []   # [key, lat, lon, count, cold_count, stamp]

        def mob_road_find(key: int, bucket: int) -> Optional[List[int]]:
            at = (key ^ ((bucket * 2654435761) & 0xFFFFFFFF)) & (MOB_TRAFFIC - 1)
            oldest = None
            for n in range(32):
                r = road_table[(at + n) & (MOB_TRAFFIC - 1)]
                if r[0] == key and r[3] == bucket:
                    return r
                if r[0] == 0:
                    oldest = r
                    break
                if oldest is None or r[1] < oldest[1]:
                    oldest = r
            if oldest is None:
                return None
            oldest[:] = [key, 0, 0, bucket, 0]
            return oldest

        def ml_var_find(key: int, bucket: int) -> Optional[List[int]]:
            at = (key ^ ((bucket * 2654435761) & 0xFFFFFFFF)) & (MOB_ML_ROAD_VAR - 1)
            oldest = None
            for n in range(24):
                v = road_var[(at + n) & (MOB_ML_ROAD_VAR - 1)]
                if v[0] == key and v[1] == bucket:
                    return v
                if v[0] == 0:
                    oldest = v
                    break
                if oldest is None or v[4] < oldest[4]:
                    oldest = v
            if oldest is None:
                return None
            oldest[:] = [key, bucket, 0, 0, 0]
            return oldest

        def ml_pref_find(key: int) -> Optional[List[int]]:
            at = ((key * 2654435761) & 0xFFFFFFFF) & (MOB_ML_PREF_ARCS - 1)
            oldest = None
            for n in range(24):
                e = pref_arcs[(at + n) & (MOB_ML_PREF_ARCS - 1)]
                if e[0] == key:
                    return e
                if e[0] == 0:
                    oldest = e
                    break
                if oldest is None or e[2] < oldest[2]:
                    oldest = e
            if oldest is None:
                return None
            oldest[:] = [key, 0, 0]
            return oldest

        last_valid = 0
        last_origin = (0, 0)
        last_dest = (0, 0)
        last_arrival = MOB_UNKNOWN
        snapped_traversals = 0

        for idx, (_, _, depart_epoch, dist_m, duration, trace, samples) in enumerate(extracted):
            origin = denoised[idx * 2]
            dest = denoised[idx * 2 + 1]
            serial += 1
            journeys[j_head] = JourneyOut(origin, dest, depart_epoch, serial, dist_m, duration, trace)
            j_head = (j_head + 1) % MOB_JOURNEYS
            if j_n < MOB_JOURNEYS:
                j_n += 1

            # Per-session staging
            stage_roads: Dict[Tuple[int, int], List[int]] = {}  # (key, bucket) -> [mean_ds, count]
            stage_cls: List[List[int]] = [[0, 0] for _ in range(MOB_ML_CLASSES)]  # sum_q8, count
            stage_pref: Dict[int, int] = {}
            stage_brakes: Dict[int, Tuple[int, int]] = {}

            if packs:
                tl_pack: Optional[CprPack] = None
                tl_arc = -1
                tl_key = 0
                tl_epoch = MOB_UNKNOWN
                tl_first_ms = 0
                tl_last_ms = 0
                tl_first_u = 0
                tl_last_u = 0
                tl_total = 0
                tl_stop_ms = 0
                tl_armed = False
                tl_done = False
                prev_speed = -1
                prev_ms = 0

                for smp in samples:
                    pk = pick_pack(packs, smp.lat, smp.lon)
                    if pk is None:
                        tl_arc = -1
                        tl_armed = tl_done = False
                        prev_speed = smp.mph10
                        prev_ms = smp.elapsed
                        continue
                    active = tl_arc if pk is tl_pack else -1
                    hit = pk.snap(smp.lat, smp.lon, smp.mph10, smp.heading, active)
                    if hit is None:
                        tl_arc = -1
                        tl_armed = tl_done = False
                        prev_speed = smp.mph10
                        prev_ms = smp.elapsed
                        continue
                    arc_id, along_u, total_u, _ = hit
                    margin = max(5, min(20, total_u // 8))
                    if (pk is not tl_pack or tl_arc != arc_id or
                            smp.elapsed - tl_last_ms > 3000 or along_u + 20 < tl_last_u):
                        tl_pack = pk
                        tl_arc = arc_id
                        tl_total = total_u
                        tl_key = pk.traffic_key(arc_id)
                        tl_first_u = along_u
                        tl_first_ms = smp.elapsed
                        tl_epoch = smp.epoch
                        tl_stop_ms = 0
                        tl_done = False
                        tl_armed = bool(tl_key and tl_epoch != MOB_UNKNOWN and along_u <= margin and smp.mph10 >= 32)
                    if smp.mph10 < 10:
                        if tl_stop_ms == 0:
                            tl_stop_ms = smp.elapsed
                        elif smp.elapsed - tl_stop_ms > 600000:
                            tl_armed = False
                    else:
                        tl_stop_ms = 0
                    tl_last_u = along_u
                    tl_last_ms = smp.elapsed

                    # Hard-braking observation (>= 9.0 mph/s from >= 20 mph)
                    if prev_speed >= 200 and smp.elapsed > prev_ms:
                        dt = smp.elapsed - prev_ms
                        if 800 <= dt <= 2200 and prev_speed > smp.mph10:
                            drop = prev_speed - smp.mph10
                            if drop * 1000 >= 90 * dt and tl_key and tl_key not in stage_brakes:
                                stage_brakes[tl_key] = (smp.lat, smp.lon)
                    prev_speed = smp.mph10
                    prev_ms = smp.elapsed

                    if tl_armed and not tl_done and along_u + margin >= tl_total and along_u > tl_first_u:
                        covered = along_u - tl_first_u
                        elapsed_ds = (smp.elapsed - tl_first_ms) // 100
                        if covered >= (tl_total * 3) // 4 and 10 <= elapsed_ds <= 36000:
                            ds = (elapsed_ds * tl_total) // covered
                            if 10 <= ds <= 36000:
                                b = mob_bucket(tl_epoch)
                                cur = stage_roads.get((tl_key, b))
                                if cur is None:
                                    stage_roads[(tl_key, b)] = [ds, 1]
                                elif cur[1] < 100:
                                    cur[0] = (cur[0] * cur[1] + ds) // (cur[1] + 1)
                                    cur[1] += 1
                                base = pk.read_base(tl_arc)
                                if base is not None:
                                    base_cost, _, _, _, _, _, cls, flags = base
                                    is_rbt = bool(flags & CPR_BASE_ROUNDABOUT)
                                    cidx = 0 if is_rbt else (cls if 1 <= cls < MOB_ML_CLASSES else -1)
                                    if cidx >= 0 and 20 <= base_cost <= 36000:
                                        q8 = max(180, min(512, (ds * 256 + base_cost // 2) // base_cost))
                                        if stage_cls[cidx][1] < 200:
                                            stage_cls[cidx][0] += q8
                                            stage_cls[cidx][1] += 1
                                stage_pref[tl_key] = 10
                                snapped_traversals += 1
                                tl_done = True

            # Commit staged roads + variance
            st16 = (serial & 0xFFFF) or 1
            for (key, b), (obs_ds, _) in stage_roads.items():
                r = mob_road_find(key, b)
                if r is None:
                    continue
                had_prev = r[4] > 0
                prev_mean = r[2]
                n = min(20, r[4])
                if r[1] and serial - r[1] > 128:
                    n = min(4, n)
                r[2] = (r[2] * n + obs_ds) // (n + 1)
                r[4] = n + 1
                r[1] = serial
                if had_prev:
                    diff = min(36000, abs(obs_ds - prev_mean))
                    v = ml_var_find(key, b)
                    if v is not None:
                        vn = min(16, v[3])
                        v[2] = min(36000, (v[2] * vn + diff) // (vn + 1))
                        v[3] = min(60000, v[3] + 1)
                        v[4] = st16

            # Commit road classes
            for cidx in range(MOB_ML_CLASSES):
                s_sum, s_cnt = stage_cls[cidx]
                if s_cnt > 0:
                    obs = (s_sum + s_cnt // 2) // s_cnt
                    p = road_cls[cidx]
                    n = min(32, p[1])
                    p[0] = max(218, min(384, (p[0] * n + obs) // (n + 1)))
                    p[1] = min(60000, p[1] + 1)

            # Commit preferred arcs
            for key, delta in stage_pref.items():
                e = ml_pref_find(key)
                if e is not None:
                    sc = e[1]
                    age = (st16 - e[2]) & 0xFFFF
                    if e[2] and age > 128:
                        sc = int(sc / 2)
                    sc = max(-120, min(120, sc + delta))
                    e[1] = sc
                    e[2] = st16

            # Commit braking hazards
            for key, (hlat, hlon) in stage_brakes.items():
                found = None
                for h in hazards:
                    if h[0] == key:
                        found = h
                        break
                if found is not None:
                    found[1] = hlat
                    found[2] = hlon
                    found[3] = min(250, found[3] + 1)
                    found[5] = st16
                else:
                    if len(hazards) < MOB_ML_HAZARDS:
                        hazards.append([key, hlat, hlon, 1, 0, st16])
                    else:
                        oldest_i = min(range(len(hazards)), key=lambda i: hazards[i][5])
                        hazards[oldest_i] = [key, hlat, hlon, 1, 0, st16]

            # Commit trip chains & recurring intermediate stop habits
            if (last_valid and depart_epoch != MOB_UNKNOWN and last_arrival != MOB_UNKNOWN and
                    depart_epoch >= last_arrival and geo_seg_m(origin[0], origin[1], last_dest[0], last_dest[1]) <= 300):
                dwell = depart_epoch - last_arrival
                if dwell <= 7 * 86400 and geo_seg_m(origin[0], origin[1], dest[0], dest[1]) >= 100:
                    db = dwell_bucket(dwell)
                    matched = False
                    for ch in chains:
                        if (ch[6] == db and
                                geo_seg_m(ch[0], ch[1], last_origin[0], last_origin[1]) <= 300 and
                                geo_seg_m(ch[2], ch[3], origin[0], origin[1]) <= 250 and
                                geo_seg_m(ch[4], ch[5], dest[0], dest[1]) <= 100):
                            ch[7] = min(60000, ch[7] + 1)
                            ch[8] = serial
                            matched = True
                            break
                    if not matched:
                        rec = [last_origin[0], last_origin[1], origin[0], origin[1], dest[0], dest[1], db, 1, serial]
                        if len(chains) < MOB_ML_CHAINS:
                            chains.append(rec)
                        else:
                            oldest_i = min(range(len(chains)), key=lambda i: chains[i][8])
                            chains[oldest_i] = rec
                if dwell <= 2700 and geo_seg_m(last_origin[0], last_origin[1], dest[0], dest[1]) >= 250 and \
                        geo_seg_m(last_origin[0], last_origin[1], origin[0], origin[1]) >= 150 and \
                        geo_seg_m(origin[0], origin[1], dest[0], dest[1]) >= 150:
                    mins = (depart_epoch % 86400) // 60
                    day_bit = 1 << mob_day(depart_epoch)
                    matched_s = False
                    for st in stops:
                        if (geo_seg_m(st[0], st[1], last_origin[0], last_origin[1]) <= 300 and
                                geo_seg_m(st[2], st[3], dest[0], dest[1]) <= 300 and
                                geo_seg_m(st[4], st[5], origin[0], origin[1]) <= 150):
                            st[6] = min(60000, st[6] + 1)
                            st[7] |= day_bit
                            st[8] = (st[8] * 3 + mins) // 4
                            st[9] = serial
                            matched_s = True
                            break
                    if not matched_s:
                        rec_s = [last_origin[0], last_origin[1], dest[0], dest[1], origin[0], origin[1], 1, day_bit, mins, serial, b'\x00' * 48]
                        if len(stops) < MOB_ML_STOPS:
                            stops.append(rec_s)
                        else:
                            oldest_i = min(range(len(stops)), key=lambda i: stops[i][9])
                            stops[oldest_i] = rec_s

            last_origin = origin
            last_dest = dest
            last_arrival = min(0xFFFFFFFF, depart_epoch + duration) if depart_epoch != MOB_UNKNOWN else MOB_UNKNOWN
            last_valid = 1

        # 4. Serialize MOB1 (mobility.dat)
        mob_buf = bytearray(MOB_MODEL_SIZE)
        struct.pack_into('<IIII', mob_buf, 0, MOB_MAGIC, serial, j_n, j_head)
        off = 16
        for i in range(MOB_JOURNEYS):
            j = journeys[i]
            if j is not None:
                struct.pack_into('<iiiiIIIII', mob_buf, off,
                                 j.origin[0], j.origin[1], j.dest[0], j.dest[1],
                                 j.depart, j.serial, j.distance, j.duration, len(j.trace))
                toff = off + 36
                for pt in j.trace[:MOB_TRACE]:
                    struct.pack_into('<ii', mob_buf, toff, pt[0], pt[1])
                    toff += 8
            off += 292
        for r in road_table:
            struct.pack_into('<IIIHH', mob_buf, off, r[0], r[1], r[2], r[3], r[4])
            off += 16
        assert off == MOB_MODEL_SIZE
        mob_bytes = bytes(mob_buf) + struct.pack('<I', fnv1a(mob_buf))

        # 5. Serialize MML1 (mobility-ml.dat), preserving existing UI prefs/POI affinity/parking if present.
        ml_buf = bytearray(MOB_ML_SIZE)
        struct.pack_into('<IIIIiiii', ml_buf, 0,
                         MOB_ML_MAGIC, MOB_ML_VER, serial, last_arrival,
                         last_origin[0], last_origin[1], last_dest[0], last_dest[1])
        off = 32
        for p in road_cls:
            struct.pack_into('<IHHHH', ml_buf, off, p[0], p[1], p[2], p[3], 0)
            off += 12
        for i in range(MOB_ML_CHAINS):
            if i < len(chains):
                ch = chains[i]
                struct.pack_into('<iiiiiiHHI', ml_buf, off, *ch)
            off += 32
        for i in range(MOB_ML_STOPS):
            if i < len(stops):
                st = stops[i]
                struct.pack_into('<iiiiiiHHII48s', ml_buf, off, *st)
            off += 84
        for e in pref_arcs:
            struct.pack_into('<IhH', ml_buf, off, e[0], e[1], e[2])
            off += 8
        for v in road_var:
            struct.pack_into('<IHHHH', ml_buf, off, v[0], v[1], v[2], v[3], v[4])
            off += 12
        poi_off = off
        off += MOB_ML_POI_AFF * 16
        park_off = off
        off += MOB_ML_PARKING * 20
        for i in range(MOB_ML_HAZARDS):
            if i < len(hazards):
                h = hazards[i]
                struct.pack_into('<IiiBBH', ml_buf, off, *h)
            off += 16
        tail_off = off
        struct.pack_into('<3b3B6B', ml_buf, off, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, last_valid, 0)
        off += 12
        assert off == MOB_ML_SIZE

        preserved_ui = 0
        if existing_ml_path is not None:
            old = read_existing_ml_ui(existing_ml_path)
            if old is not None:
                # Preserve POI affinity, parking spots, auto-zoom bias, and day/night brightness.
                ml_buf[poi_off:poi_off + MOB_ML_POI_AFF * 16] = old[poi_off:poi_off + MOB_ML_POI_AFF * 16]
                ml_buf[park_off:park_off + MOB_ML_PARKING * 20] = old[park_off:park_off + MOB_ML_PARKING * 20]
                ml_buf[tail_off:tail_off + 10] = old[tail_off:tail_off + 10]
                preserved_ui = 1

        ml_bytes = bytes(ml_buf) + struct.pack('<I', fnv1a(ml_buf))

        stats = {
            'valid_records': valid_recs,
            'corrupt_records': corrupt_recs,
            'journeys': j_n,
            'dbscan_clusters': cluster_count,
            'route_packs': len(packs),
            'snapped_traversals': snapped_traversals,
            'road_profiles': sum(1 for r in road_table if r[0] != 0),
            'variance_buckets': sum(1 for v in road_var if v[0] != 0),
            'road_classes_trained': sum(1 for p in road_cls if p[1] > 0),
            'trip_chains': len(chains),
            'stop_habits': len(stops),
            'preferred_arcs': sum(1 for e in pref_arcs if e[0] != 0),
            'braking_hazards': len(hazards),
            'preserved_ui_profile': preserved_ui,
        }
        return mob_bytes, ml_bytes, stats
    finally:
        for p in packs:
            p.close()


def atomic_write(target: Path, data: bytes) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    tmp = target.with_suffix('.tmp')
    tmp.write_bytes(data)
    os.replace(tmp, target)


def verify_written(out_dir: Path) -> None:
    for name, expected_size, expected_magic in (
        ('mobility.dat', MOB_MODEL_SIZE, MOB_MAGIC),
        ('mobility-ml.dat', MOB_ML_SIZE, MOB_ML_MAGIC),
    ):
        p = out_dir / name
        raw = p.read_bytes()
        if len(raw) != expected_size + 4:
            raise ValueError(f'{name}: unexpected byte size {len(raw)}')
        body, check = raw[:-4], struct.unpack('<I', raw[-4:])[0]
        if fnv1a(body) != check:
            raise ValueError(f'{name}: FNV-1a checksum mismatch')
        magic = struct.unpack_from('<I', body, 0)[0]
        if magic != expected_magic:
            raise ValueError(f'{name}: bad magic 0x{magic:08x}')


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--card', type=Path, help='Mounted ClioPlay SD card root (contains clio843/)')
    parser.add_argument('--history', type=Path, nargs='*', help='Raw history files in chronological order')
    parser.add_argument('--maps', type=Path, help='CPR2 v5 route.cpr file, single-pack dir, or atlas bundle dir')
    parser.add_argument('--out-dir', type=Path, help='Output directory for mobility.dat and mobility-ml.dat')
    parser.add_argument('--dry-run', action='store_true', help='Analyse and report aggregate stats without writing')
    parser.add_argument('--verify', action='store_true', help='Re-read written models and verify checksums')
    args = parser.parse_args(argv)

    history_paths: List[Path] = []
    maps_path = args.maps
    out_dir = args.out_dir

    if args.card is not None:
        clio = args.card / 'clio843'
        if not args.history:
            for nm in ('history.prev.dat', 'history.dat'):
                if (clio / nm).is_file():
                    history_paths.append(clio / nm)
        if maps_path is None and (clio / 'maps').exists():
            maps_path = clio / 'maps'
        if out_dir is None:
            out_dir = clio
    if args.history:
        history_paths.extend(args.history)

    if not history_paths:
        parser.error('Provide --card or at least one --history file')

    existing_ml = (out_dir / 'mobility-ml.dat') if out_dir is not None else None
    mob_bytes, ml_bytes, stats = replay(history_paths, maps_path, existing_ml_path=existing_ml)

    if not args.dry_run:
        if out_dir is None:
            parser.error('Provide --out-dir or --card when not using --dry-run')
        atomic_write(out_dir / 'mobility.dat', mob_bytes)
        atomic_write(out_dir / 'mobility-ml.dat', ml_bytes)
        if args.verify:
            verify_written(out_dir)

    mode = 'dry-run' if args.dry_run else 'written'
    print(f'ClioPlay history replay ({mode}):')
    for k, v in stats.items():
        print(f'  {k}: {v}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
