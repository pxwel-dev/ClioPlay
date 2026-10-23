# QAD7 / PERF8 unified reconstruction packets

QAD7 keeps the safe QAD5 query architecture (rank-ordered node blocks, direct successor offsets, mmap and validated stall-on-demand) and replaces QAD6's split reconstruction streams with self-contained geometry packets.

For every unique physical geometry range used by CH base arcs, the builder stores one canonical packet stream containing:

- latitude / longitude
- exact cumulative Q16 cost fraction in both directions
- exact integer incoming segment metres in both directions
- exact 65536-fraction sentinel flags

Opposite directed base edges share the same packet. Each locality-renumbered reconstruction record points directly to its packet, so full CH base-arc reconstruction no longer needs CPR geometry reads or a separate QAD geometry-metadata lookup.

Start and destination partial edges still use the original CPR arithmetic because their live snap point is not known offline.

`route.cpr` remains unchanged and authoritative. If `route.qadj` is absent, invalid, or rejected, ClioPlay falls back to CPR2 routing.
