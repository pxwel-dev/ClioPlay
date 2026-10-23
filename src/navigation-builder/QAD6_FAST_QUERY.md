# QAD6 / PERF7 geometry-arithmetic sidecar

QAD6 extends QAD5 without changing `route.cpr`.

## Query

The CH query layout is unchanged from QAD5:

- routing-state blocks are written in CH-rank order;
- each adjacency entry carries target state, cost, sidecar-local reconstruction arc id and target block offset;
- the sidecar is mmap'd read-only when supported;
- conservative exact CH termination and validated stall-on-demand are retained.

## Reconstruction arithmetic

For every stored CPR geometry point, QAD6 appends a compact record containing:

- cumulative Q16 fraction from the physical geometry start;
- cumulative Q16 fraction from the physical geometry end;
- integer metres for the preceding segment in the stored direction;
- integer metres for the same segment in the reverse direction;
- exact 65536-fraction flags.

The fractions are generated with the same denominator-reduction behaviour as the embedded `route_ratio_q16()`. Forward and reverse metre fields are separate because the legacy ARM signed-shift approximation can differ by one metre by direction.

For full physical base arcs the runtime therefore replaces:

```text
segment-units calculation -> software division -> cumulative cost
segment-distance calculation -> software division
```

with mmap reads plus multiplication/addition. Start and destination partial-edge snapping still uses the original runtime arithmetic because the snap location can lie at an arbitrary sub-segment fraction.

PERF7 also stores per-route-segment metres while emitting the route. Remaining distance can then be updated by subtraction as `route_head` advances, and the old post-build `route_compute_m()` pass is no longer needed.

## Correctness

The builder-side inspector recomputes QAD6 metadata from `route.cpr`. Development tests cover ordinary and multi-point geometries, forward and reversed physical arcs, cumulative-cost equality and direction-sensitive metre equality.
