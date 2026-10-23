"""Explicit, source-attributed gap corrections keyed to global OSM way IDs.

These records supplement missing evidence. They never turn a routing estimate
into a legal limit or replace an existing conditional/directional speed tag.
"""
import hashlib
import json
from pathlib import Path
from global_speeds import scalar


class SpeedImport:
    def __init__(self, path):
        self.path = Path(path).expanduser().resolve()
        raw = self.path.read_bytes()
        data = json.loads(raw)
        if data.get('schema') != 'clioplay-speed-import-1' or not isinstance(data.get('source'), str) or not data['source'].strip():
            raise ValueError('Speed import needs schema clioplay-speed-import-1 and a source')
        if not isinstance(data.get('ways'), dict) or not data['ways']:
            raise ValueError('Speed import needs a nonempty ways object')
        self.ways = {}
        allowed = {'maxspeed', 'maxspeed:forward', 'maxspeed:backward'}
        for key, value in data['ways'].items():
            if not key.isdigit() or not 0 < int(key) < 2**63:
                raise ValueError('Invalid OSM way ID: '+key)
            tags = {'maxspeed': value} if isinstance(value, str) else value
            if not isinstance(tags, dict) or not tags or not set(tags) <= allowed:
                raise ValueError('Only numeric maxspeed/forward/backward corrections are supported')
            for v in tags.values():
                if not isinstance(v, str) or scalar(v)[0] <= 0 or scalar(v)[1] & ~2:
                    raise ValueError('Use a numeric speed with km/h or mph: '+str(v))
            self.ways[int(key)] = dict(tags)
        self.identity = {'name': self.path.name, 'sha256': hashlib.sha256(raw).hexdigest(),
                         'source': data['source'], 'observed': data.get('observed'),
                         'configured_ways': len(self.ways)}
        self.seen, self.applied, self.preserved = set(), set(), set()

    def apply(self, props):
        wid = int(props.get('@id', 0))
        if wid not in self.ways:
            return props
        self.seen.add(wid)
        if any(key.startswith('maxspeed') or key in ('source:maxspeed', 'zone:maxspeed')
               for key in props):
            self.preserved.add(wid)
            return props
        self.applied.add(wid)
        return dict(props, **self.ways[wid])

    def report(self):
        return dict(self.identity, matched_ways=len(self.seen),
                    corrected_ways=len(self.applied), preserved_tagged_ways=len(self.preserved),
                    unmatched_way_ids=sorted(set(self.ways)-self.seen))
