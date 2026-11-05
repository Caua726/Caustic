#!/usr/bin/env bash
# tools/cmp-clean.sh <dir> <target> <output> — a warm build must equal a clean one.
#
# Builds with whatever cache is there, sets the result aside, drops the output and
# every cache except fetched dependencies, builds again and compares the two.
set -u
dir=$1; target=$2; out=$3
mk=${CAUSTIC_MK:-caustic-mk}
cd "$dir" || exit 1
"$mk" build "$target" -q ${CMP_FLAGS:-} || exit 1
keep=$(mktemp)
cp "$out" "$keep"
rm -f "$out"
find .caustic -mindepth 1 -maxdepth 1 ! -name deps -exec rm -rf {} +
"$mk" build "$target" -q ${CMP_FLAGS:-} || { rm -f "$keep"; exit 1; }
if cmp "$keep" "$out"; then echo "cmp-clean: identical"; rc=0; else echo "cmp-clean: DIFFERENT"; rc=1; fi
rm -f "$keep"
exit $rc
