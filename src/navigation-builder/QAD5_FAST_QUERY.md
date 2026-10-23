# QAD5 / PERF6 fast-query sidecar

QAD5 is an optional accelerator generated from the exact CPR2-v4 `route.cpr` already used by CLIOPLAY. It does not alter `route.cpr` and can be removed at any time to fall back to the older routing path.

## Query layout

Each routing state has one self-contained block containing both upward directions. Blocks are physically written in CH-rank order. Every 16-byte query entry stores:

- target routing state
- edge cost
- locality-renumbered reconstruction arc id
- target state's QAD5 block offset

The node→block index is memory-mapped read-only and is needed only for initial seed states. Once a state is discovered, its direct block offset travels with the relaxation, avoiding repeated index lookups and `lseek/read` calls.

PERF6 uses the opposite upward list in the same block for **stall-on-demand**. No separate stall table or route.cpr change is needed.

## Correctness note

PERF6 deliberately uses the conservative CH termination condition (`min_forward >= best && min_backward >= best`). During development the earlier `min_forward + min_backward >= best` optimisation was found not to be generally valid for an upward-restricted contraction-hierarchy query and was removed. Conservative CH + stall-on-demand was checked against full Dijkstra over 146,502 randomized directed queries with zero cost mismatches.

## Reconstruction

The QAD5 reconstruction records retain QAD4's locality-renumbered shortcut DAG and precomputed base-geometry metadata. PERF6 first unpacks the winning path completely, then sorts/deduplicates the required geometry pages and faults/reads them in ascending order before emitting the route.

The runtime attempts read-only mmap for QAD5 and the CPR geometry section. Both have existing paged-read fallbacks if mmap is unavailable on the device kernel.
