#!/bin/sh
# Real lexical-scope consumers; malformed declarations must emit no executable.
# Usage: sh tests/run_scope_tests.sh [compiler]
set -eu
cc=${1:-${CAUSTIC_TEST_COMPILER:-./caustic}}
work=$(mktemp -d "${TMPDIR:-/tmp}/caustic-scope.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
for level in 0 1 2; do
    "$cc" -q "-O$level" tests/scope_shadow_test.cst -o "$work/shadow-$level"
    "$work/shadow-$level"
    "$cc" -q "-O$level" tests/inline_asm_test.cst -o "$work/naked-$level"
    "$work/naked-$level"
    for name in dup_global dup_local dup_param dup_param_local dup_param_naked dup_param_extern dup_param_generic; do
        output="$work/$name-$level"
        if "$cc" -q "-O$level" "tests/reject/$name.cst" -o "$output"; then
            echo "FAIL scope: $name accepted at -O$level" >&2
            exit 1
        fi
        if [ -e "$output" ]; then
            echo "FAIL scope: $name left an executable at -O$level" >&2
            exit 1
        fi
    done
done
echo 'scope declarations: O0/O1/O2 rejection and legal-shadow/naked consumers passed'
