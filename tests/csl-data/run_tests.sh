#!/bin/sh
# What a program linked with a .csl carries of the library: not its code, not its
# data. Only the CausticOS kernel loads these images, so this checks the files; the
# run in the guest is causticos's scripts/test-csl-data.sh, over these same sources.
# Usage from the compiler root: sh tests/csl-data/run_tests.sh [compiler]
set -eu
root=$(pwd)
cc=${1:-${CAUSTIC_TEST_COMPILER:-./caustic}}
cc=$(command -v "$cc")
case "$cc" in /*) ;; *) cc="$root/$cc" ;; esac
export CAUSTIC_LIB="$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/caustic-csl-data.XXXXXX")
trap 'rm -rf "$work"' EXIT
src="$root/tests/csl-data"
cd "$work"

# The first 16 bytes of font.cst's GLYPHS.
glyphs='\x00\x00\x7e\xc3\xbd\xbd\xfd\xfb\xf7\xf7\xff\xf7\xff\x7e\x00\x00'
has_glyphs() { LC_ALL=C grep -qaP "$glyphs" "$1"; }

"$cc" -q --target=caustic-x86_64 --shared --base=0x32000000 "$src/font_root.cst" -o libbase.csl
has_glyphs libbase.csl || { echo "FAIL: the library does not hold the glyphs"; exit 1; }
for level in 0 1 2; do
    "$cc" -q "-O$level" --target=caustic-x86_64 "$src/font_prog.cst" -o alone.cse
    has_glyphs alone.cse || { echo "FAIL: -O$level a program with no library lost the glyphs"; exit 1; }
    "$cc" -q "-O$level" --target=caustic-x86_64 -lbase -L. "$src/font_prog.cst" -o linked.cse
    if has_glyphs linked.cse; then echo "FAIL: -O$level the program carries the library's glyphs"; exit 1; fi
    alone=$(wc -c < alone.cse)
    linked=$(wc -c < linked.cse)
    [ "$linked" -lt 1024 ] || { echo "FAIL: -O$level a one-call program is $linked bytes"; exit 1; }
    echo "font: -O$level alone $alone B, linked $linked B"
done
echo "csl-data: passed"
