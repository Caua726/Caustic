#!/usr/bin/env bash
# check-no-sse.sh [compiler]: what --no-sse promises.
#   - no instruction of the built program names an xmm, ymm, zmm, mmx or x87
#     register (by objdump: raw .byte encodings count), at -O0, -O1 and -O2;
#   - the program gives what its normal build gives;
#   - floating point that reaches the code is refused, by name, with exit 1.
# The programs: tests/no_sse_test.cst, tests/no_sse/ and every example and test
# that builds without it (those using floats must be refused, not miscompiled).
set -u
ulimit -c 0
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC="${1:-$ROOT/caustic}"
case "$CC" in /*) ;; *) CC="$PWD/$CC" ;; esac
cd "$ROOT"
export CAUSTIC_LIB="$ROOT" CAUSTIC_NO_OBJ_CACHE=1
T="$(mktemp -d "${TMPDIR:-/tmp}/check-no-sse.XXXXXX")"
trap 'rm -rf "$T"' EXIT
fail=0
bad() { echo "FAIL: $1"; fail=1; }
simd() { objdump -d -M intel --no-show-raw-insn "$1" | sed 's/<[^>]*>//g' | awk -F'\t' 'NF>=2 && ($0 ~ /[xyz]mm[0-9]|[^a-z]mm[0-7]|[^a-z]st\(|[^a-z]st[,$]/ || $2 ~ /^(f(ld|st|add|mul|div|sub|ild|ist)[a-z]*|emms|ldmxcsr|vzero[a-z]*)( |$)/)' | wc -l; }

for l in O0 O1 O2; do
    "$CC" -q -$l --no-sse tests/no_sse_test.cst -o "$T/p" || { bad "build -$l"; continue; }
    n=$(simd "$T/p"); [ "$n" = 0 ] || bad "-$l: $n simd instructions"
    "$T/p"; rc=$?; [ $rc = 0 ] || bad "-$l: exit $rc"
    "$CC" -q -$l tests/no_sse_test.cst -o "$T/q" && "$T/q" || bad "normal build -$l"
    if [ $l = O2 ]; then [ "$(simd "$T/q")" -gt 0 ] || bad "the normal -O2 build has no simd: the test shows nothing"; fi
    for s in tests/no_sse/refuse_*.cst; do
        "$CC" -q -$l --no-sse "$s" -o "$T/r" > "$T/out" 2>&1; rc=$?
        [ $rc = 1 ] && grep -q "no-sse: function '.*' uses floating point" "$T/out" || bad "-$l $s: exit $rc, $(head -1 "$T/out")"
    done
    "$CC" -q -$l --no-sse tests/no_sse/ok_dead.cst -o "$T/d" && "$T/d" && [ "$(simd "$T/d")" = 0 ] || bad "-$l ok_dead"
    "$CC" -q -$l --no-sse tests/no_sse/ok_asm.cst -o "$T/d" && "$T/d" && [ "$(simd "$T/d")" = 1 ] || bad "-$l ok_asm"
done
"$CC" -q -O2 --no-sse -c tests/no_sse_test.cst -o "$T/p.o" && [ "$(simd "$T/p.o")" = 0 ] || bad "-c"
"$CC" -q -O2 --no-sse --target=linux-aarch64 tests/no_sse_test.cst -o "$T/x" 2>/dev/null && bad "accepted for aarch64"
"$CC" -q -O2 --no-sse --cpu=x86-64-v3 tests/no_sse_test.cst -o "$T/x" 2>/dev/null && bad "accepted with --cpu=x86-64-v3"

built=0; refused=0; ran=0; skipped=0
for s in examples/*.cst tests/*_test.cst; do
    for l in O1 O2; do
        "$CC" -q -$l "$s" -o "$T/q" >/dev/null 2>&1 || { skipped=$((skipped+1)); continue; }
        if "$CC" -q -$l --no-sse "$s" -o "$T/p" > "$T/out" 2>&1; then
            built=$((built+1))
            # an asm block or a fixed encoding in the source is not the compiler's
            if ! grep -q 'xmm' "$s"; then n=$(simd "$T/p"); [ "$n" = 0 ] || bad "$s -$l: $n simd instructions"; fi
            case "$s" in *thread*|*term*|*poll*|*net*|*ipv6*|*time*|*random*|*proc*|*lock*) continue ;; esac
            (cd "$ROOT" && timeout 20 "$T/q" < /dev/null > "$T/o1" 2>/dev/null); r1=$?
            (cd "$ROOT" && timeout 20 "$T/p" < /dev/null > "$T/o2" 2>/dev/null); r2=$?
            ran=$((ran+1))
            [ $r1 = $r2 ] || bad "$s -$l: exit $r2, the normal build $r1"
            case "$s" in examples/bench*) ;; *) cmp -s "$T/o1" "$T/o2" || bad "$s -$l: prints differently" ;; esac   # bench*: timings
        else
            grep -q "no-sse: function '.*' uses floating point" "$T/out" && refused=$((refused+1)) || bad "$s -$l: $(head -1 "$T/out")"
        fi
    done
done
echo "check-no-sse: $built builds without simd, $ran compared, $refused refused for floats, $skipped do not build at all"
[ $fail = 0 ] && echo "check-no-sse: ok" || exit 1
