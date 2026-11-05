#!/usr/bin/env bash
# tools/noop-bench.sh <dir> <target> [runs] — what `caustic-mk build` costs when nothing
# changed. The output must already be built. Prints the median wall and the largest
# RSS of an invocation; the first run is reported apart, since it is the one that used
# to compile the whole program to find out its dependencies.
set -u
dir=$1; target=$2; runs=${3:-20}
mk=${CAUSTIC_MK:-caustic-mk}
cd "$dir" || exit 1
tmp=$(mktemp)
for _ in $(seq "$runs"); do
    /usr/bin/time -a -o "$tmp" -f '%e %M' "$mk" build "$target" -q >/dev/null 2>&1
done
sort -n "$tmp" | awk -v first="$(head -1 "$tmp")" '
    { w[NR]=$1; if ($2>m) m=$2 }
    END { printf "first: %s s/KB; median: %s s; max rss: %.1f MB; runs: %d\n", first, w[int((NR+1)/2)], m/1024, NR }'
rm -f "$tmp"
