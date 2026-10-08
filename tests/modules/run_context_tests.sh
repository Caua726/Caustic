#!/bin/sh
# Real field/layout consumers, including debugger lookup of homonymous types.
# Usage: sh tests/modules/run_context_tests.sh [compiler] [linker]
set -eu
cc=${1:-${CAUSTIC_TEST_COMPILER:-./caustic}}
ld=${2:-${CAUSTIC_TEST_LINKER:-./caustic-ld}}
work=$(mktemp -d "${TMPDIR:-/tmp}/caustic-context.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
for level in 0 1 2; do
    "$cc" -q "-O$level" -g tests/modules/collision_test.cst -o "$work/direct-$level"
    "$work/direct-$level"
    "$cc" -q "-O$level" -g -c tests/modules/collision_test.cst -o "$work/consumer-$level.o"
    "$ld" -g "$work/consumer-$level.o" -o "$work/separate-$level"
    "$work/separate-$level"
done
if "$cc" -q -c tests/modules/collision_test.cst -o "$work/absent/consumer.o"; then
    echo 'module context: missing output directory falsely succeeded' >&2
    exit 1
fi
if ! command -v gdb >/dev/null 2>&1; then
    echo 'module context: gdb unavailable; debugger acceptance not exercised' >&2
    exit 1
fi
for mode in direct separate; do
    gdb -batch -x tests/modules/context_debug.gdb "$work/$mode-0"
done
