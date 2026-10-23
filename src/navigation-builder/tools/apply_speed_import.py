#!/usr/bin/env python3
"""Fill missing CPM5 limits without rebuilding roads, areas or routing.

Only exact source-way geometry and original metadata matches are eligible.
No street-name/radius matching and no inferred legal defaults are used. A backup
directory is required for commits. Use the same import with build_navigation.py
--speed-import to reproduce the limits on subsequent fresh builds.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

from build_map import ROAD, tile_paths, geom_blob, road_meta_blob
from global_speeds import directions
from speed_import import SpeedImport
from bundle_layout import bundle_names
from inspect_map import HDR, IDX, validate_labels, _validate_meta_container


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024*1024), b''):
            h.update(block)
    return h.hexdigest()


def source_records(pbf, corrections, work):
    import osmium
    ids = work/'ways.txt'
    ids.write_text(''.join('w'+str(wid)+'\n' for wid in corrections.ways))
    source = work/'ways.osm.pbf'
    subprocess.run(['osmium', 'getid', str(pbf), '-r', '--id-file', str(ids),
                    '-o', str(source), '--overwrite'], check=True)
    records = {}

    class Ways(osmium.SimpleHandler):
        def way(self, way):
            props = {tag.k: tag.v for tag in way.tags}
            props['@id'] = way.id
            updated = corrections.apply(props)
            if updated is props:
                return
            if props.get('highway') not in ROAD:
                raise ValueError('Correction is not a rendered road: '+str(way.id))
            coords = [(node.lon, node.lat) for node in way.nodes]
            name, ref = props.get('name', ''), props.get('ref', '')
            old = road_meta_blob(name, ref, 0, directions(props))
            new = road_meta_blob(name, ref, 0, directions(updated))
            cls = ROAD[props['highway']][0]
            for lvl, tx, ty, points in tile_paths(coords, 6, True):
                raw = geom_blob(0, cls, points)
                # Earlier CPM5 builders left the new no-estimate flag clear.
                # Accept that exact legacy record too, retaining geometry checks.
                legacy = bytearray(old) if old is not None else None
                if legacy is not None:
                    legacy[1] &= ~16;legacy[3] &= ~16
                    legacy = bytes(legacy)
                for meta in (old, legacy, new):
                    key = (lvl, tx, ty, raw[:3]+raw[4:], meta)
                    if key in records and records[key][0] != new:
                        raise ValueError('Ambiguous overlapping source-way corrections')
                    records[key] = (new, way.id)

    Ways().apply_file(str(source), locations=True)
    missing = set(corrections.ways)-corrections.seen
    if missing:
        raise ValueError('OSM way IDs absent from source PBF: '+str(sorted(missing)))
    return records


def patch_tile(raw, lvl, tx, ty, records, matched):
    _validate_meta_container(raw, 5)
    ng, nl, nm, reserved = struct.unpack_from('<4H', raw)
    pos, geometry = 8, []
    for _ in range(ng):
        start = pos
        pos += 4+raw[pos+2]*4
        geometry.append((start, pos))
    geom_end = pos
    meta_start = validate_labels(raw, pos, nl)
    pos, metas = meta_start, []
    for _ in range(nm):
        end = pos+5+raw[pos+4]
        metas.append(raw[pos:end])
        pos = end
    edits = {}
    for i, (start, end) in enumerate(geometry):
        if raw[start] != 0:
            continue
        mi = raw[start+3]
        old = None if mi == 255 else metas[mi]
        key = (lvl, tx, ty, raw[start:start+3]+raw[start+4:end], old)
        if key in records:
            new, wid = records[key]
            matched.add(wid)
            if new != old:edits[i] = new
    if not edits:
        return raw
    # Metadata can be shared by unrelated features. Reindex each geometry's
    # chosen record instead of overwriting a shared record in place.
    geom = bytearray(raw[8:geom_end])
    new_metas, slots = [], {}
    for i, (start, _) in enumerate(geometry):
        mi = raw[start+3]
        value = edits.get(i, None if mi == 255 else metas[mi])
        if value is None:
            continue
        if value not in slots:
            if len(slots) >= 255:
                raise ValueError('Corrected tile exceeds 255 metadata records')
            slots[value] = len(slots)
            new_metas.append(value)
        geom[start-8+3] = slots[value]
    result = (struct.pack('<4H', ng, nl, len(new_metas), reserved)+geom+
              raw[geom_end:meta_start]+b''.join(new_metas))
    if len(result) > 32768:
        raise ValueError('Corrected tile exceeds native 32 KiB tile limit')
    _validate_meta_container(result, 5)
    return bytes(result)


def patch_map(source, destination, records, matched):
    tiles = {key[:3] for key in records}
    with source.open('rb') as src:
        header = list(HDR.unpack(src.read(HDR.size)))
        if header[:2] != [b'CPM5', 5]:
            raise ValueError('Existing-map corrections require CPM5')
        src.seek(header[6])
        rows = [list(IDX.unpack(src.read(IDX.size))) for _ in range(header[5])]
        edits = {}
        for i, row in enumerate(rows):
            if tuple(row[j] for j in (0, 2, 3)) not in tiles:
                continue
            src.seek(header[7]+row[4])
            raw = src.read(row[5])
            updated = patch_tile(raw, row[0], row[2], row[3], records, matched)
            if updated != raw:
                edits[i] = updated
        if not edits:
            return 0
        destination.parent.mkdir(parents=True, exist_ok=True)
        with destination.open('w+b') as dst:
            dst.seek(header[7])
            offsets, offset = {}, 0
            for i, row in enumerate(rows):
                key = (row[4], row[5], i if i in edits else None)
                if key not in offsets:
                    src.seek(header[7]+row[4])
                    blob = edits.get(i)
                    if blob is None:
                        blob = src.read(row[5])
                    if len(blob) != (len(edits[i]) if i in edits else row[5]):
                        raise ValueError('Short source map tile')
                    offsets[key] = offset
                    dst.write(blob)
                    offset += len(blob)
                if i in edits:
                    ng, nl, nm = struct.unpack_from('<3H', edits[i])
                    row[5], row[6] = len(edits[i]), ng+nl+nm
                row[4] = offsets[key]
            if dst.tell() >= 0x80000000:
                raise ValueError('Corrected map exceeds native seek limit')
            dst.seek(0)
            dst.write(HDR.pack(*header))
            for row in rows:
                dst.write(IDX.pack(*row))
            dst.flush()
            os.fsync(dst.fileno())
    return len(edits)


def apply(maps, pbf, imports, backup):
    maps, backup = Path(maps).resolve(), Path(backup).resolve()
    if backup == maps or maps in backup.parents or backup in maps.parents:
        raise ValueError('Backup directory must be separate from the map bundle')
    manifest_path = maps/'build.json'
    original = manifest_path.read_bytes()
    manifest = json.loads(original)
    names = [name for name in bundle_names(manifest) if name == 'map.cpm' or name.endswith('/map.cpm')]
    corrections = SpeedImport(imports)
    with tempfile.TemporaryDirectory(prefix='clioplay-speed-stage-', dir=maps.parent) as temp:
        work = Path(temp)
        records = source_records(Path(pbf).resolve(), corrections, work)
        matched, changed = set(), {}
        for name in names:
            count = patch_map(maps/name, work/name, records, matched)
            if not count:
                continue
            if sha(maps/name) != manifest['files'][name]['sha256']:
                raise ValueError('Existing bundle checksum mismatch: '+name)
            subprocess.run([shutil.which('python3') or 'python3',
                            str(Path(__file__).with_name('inspect_map.py')), str(work/name), '--full'], check=True)
            manifest['files'][name] = {'bytes': (work/name).stat().st_size, 'sha256': sha(work/name)}
            changed[name] = count
        if corrections.applied-matched:
            raise ValueError('No exact map geometry/metadata match for way IDs: '+str(sorted(corrections.applied-matched)))
        report = dict(corrections.report(), changed_tiles=changed,
                      policy='Exact source-way geometry and original metadata only; tagged evidence preserved')
        if not changed:
            return report
        if manifest_path.read_bytes() != original:
            raise ValueError('Map manifest changed during correction preparation')
        manifest.setdefault('speed_import_history', []).append(report)
        backup.mkdir(parents=True, exist_ok=False)
        shutil.copy2(manifest_path, backup/'build.json')
        for name in changed:
            target = backup/name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(maps/name, target)
        committed = []
        try:
            for name in changed:
                os.replace(work/name, maps/name)
                committed.append(name)
            staged_manifest = work/'build.json'
            staged_manifest.write_text(json.dumps(manifest, indent=2)+'\n')
            os.replace(staged_manifest, manifest_path)
        except BaseException:
            for name in committed:
                shutil.copy2(backup/name, maps/name)
            manifest_path.write_bytes(original)
            raise
        return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--maps', type=Path, required=True)
    parser.add_argument('--pbf', type=Path, required=True)
    parser.add_argument('--speed-import', type=Path, required=True)
    parser.add_argument('--backup-dir', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(apply(args.maps, args.pbf, args.speed_import, args.backup_dir), indent=2))


if __name__ == '__main__':
    main()
