# QAD4 fast-query + locality reconstruction sidecar (DEV5.28 PERF5)

QAD4 is derived directly from the exact CPR2-v4 `route.cpr` already installed in the car. No OSM rebuild is required and `route.cpr` remains unchanged.

QAD4 retains QAD3's compact group-block query layout and small group index. The major change is reconstruction locality:

- CH arcs receive **sidecar-local IDs** ordered preorder from shortcut-DAG roots.
- Query adjacency records are rewritten to those local IDs.
- Shortcut child IDs are rewritten to the same local ID space.
- Base records carry `geom_first`, `geom_count`, base geometry flags/class, and the precomputed geometry-unit total.

This means the winning path can be traced and recursively unpacked entirely from `route.qadj` without reading CPR2 CH-arc or base-edge records for normal QAD4 base arcs. Physical geometry points are still read from the authoritative CPR2-v4 geometry table.

The embedded runtime uses a 256 KiB 4 KiB-page LRU reconstruction cache. The locality-renumbered record order is designed so parent shortcuts and the descendants immediately unpacked after them frequently share those pages.

Generate from an existing CPR2-v4 file:

```sh
./build_qadj.command /path/to/route.cpr /path/to/route.qadj
```

Then copy `route.qadj` beside `route.cpr` in `clio843/maps/`.

DEV5.28 accepts QAD4, QAD3, QAD2, QAD1, or no sidecar. If QAD4 is absent or invalid, it safely falls back to older acceleration formats / CPR2 as available.
