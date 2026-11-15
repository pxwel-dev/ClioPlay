# QAD5 compatibility format

The runtime retains QAD5 compatibility for matching CPR2-v4 packs. New navigation
builds generate [QAD7](QAD7_FAST_QUERY.md); this document describes the QAD5 layout
and query rules still supported by the current reader.

## Query layout

Each routing state has one self-contained block containing both upward directions. Blocks are physically written in CH-rank order. Every 16-byte query entry stores:

- target routing state
- edge cost
- locality-renumbered reconstruction arc id
- target state's QAD5 block offset

The node→block index is memory-mapped read-only and is needed only for initial seed states. Once a state is discovered, its direct block offset travels with the relaxation, avoiding repeated index lookups and `lseek/read` calls.

QAD5 uses the opposite upward list in the same block for **stall-on-demand**. No separate stall table or route.cpr change is needed.

## Correctness note

The reader uses conservative CH termination
(`min_forward >= best && min_backward >= best`) and stall-on-demand. The sum of
both minima is not a valid general termination bound for the upward-restricted
query.

## Reconstruction

The QAD5 reconstruction records retain QAD4's locality-renumbered shortcut DAG and precomputed base-geometry metadata. QAD5 first unpacks the winning path completely, then sorts/deduplicates the required geometry pages and faults/reads them in ascending order before emitting the route.

The runtime attempts read-only mmap for QAD5 and the CPR geometry section. Both have existing paged-read fallbacks if mmap is unavailable on the device kernel.
