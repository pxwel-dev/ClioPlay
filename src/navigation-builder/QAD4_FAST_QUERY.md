# QAD4 compatibility format

QAD4 describes a sidecar matched to an exact CPR2-v4 `route.cpr`. The CPR geometry remains authoritative.

QAD4 retains QAD3's compact group-block query layout and small group index. The major change is reconstruction locality:

- CH arcs receive **sidecar-local IDs** ordered preorder from shortcut-DAG roots.
- Query adjacency records are rewritten to those local IDs.
- Shortcut child IDs are rewritten to the same local ID space.
- Base records carry `geom_first`, `geom_count`, base geometry flags/class, and the precomputed geometry-unit total.

This means the winning path can be traced and recursively unpacked entirely from `route.qadj` without reading CPR2 CH-arc or base-edge records for normal QAD4 base arcs. Physical geometry points are still read from the authoritative CPR2-v4 geometry table.

The embedded runtime uses a 256 KiB 4 KiB-page LRU reconstruction cache. The locality-renumbered record order is designed so parent shortcuts and the descendants immediately unpacked after them frequently share those pages.

## Current use

The current navigation builder generates QAD7. QAD4 is retained as a runtime
compatibility format for matching CPR2-v4 packs, not as an alternative build mode.
Use the complete current builder/installer for new navigation data; see
[the builder guide](README.md) and [QAD7](QAD7_FAST_QUERY.md).
