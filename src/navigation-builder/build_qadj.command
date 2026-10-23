#!/bin/sh
set -eu
HERE="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROUTE="${1:-$HERE/route.cpr}"
OUT="${2:-$HERE/route.qadj}"
python3 "$HERE/tools/build_qadj.py" "$ROUTE" "$OUT"
echo "QAD7 sidecar ready: $OUT"
