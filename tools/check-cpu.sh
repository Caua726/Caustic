#!/usr/bin/env bash
# check-cpu.sh [compiler]: what --cpu= promises about the code a program gets.
#   x86-64 (default): nothing above SSE2, no cpuid, on a program with vector loops
#   auto: AVX2 behind the run-time test
#   x86-64-v3, znver3: AVX2 with no test
# Each build must also run and pass its self-check. With qemu-x86_64, the default
# and auto builds must run on a CPU without SSE3 or AVX, and a v3 build must not.
set -u
ulimit -c 0   # the v3 build is meant to die under qemu: no core file for it
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC="${1:-$ROOT/caustic}"
SRC="$ROOT/benchmarks/kernels/array_divide.cst"
SMALL="$ROOT/benchmarks/kernels/linalg/gemver.cst"
T="$(mktemp -d /tmp/check-cpu.XXXXXX)"
trap 'rm -rf "$T"' EXIT
ABOVE='^(v[a-z0-9]+|movddup|movs[hl]dup|haddp[sd]|hsubp[sd]|addsubp[sd]|lddqu|pshufb|palignr|pabs[bwd]|pmaddubsw|phadd[a-z]*|phsub[a-z]*|pmulld|pmuldq|pmins[bd]|pmaxs[bd]|pminu[wd]|pmaxu[wd]|ptest|pblend[a-z]*|blendv?p[sd]|round[sp][sd]|pmov[sz]x[a-z]*|pextr[bdq]|pinsr[bdq]|insertps|extractps|pcmpeqq|pcmpgtq|packusdw|popcnt|lzcnt|tzcnt|cpuid|xgetbv)$'
fail=0
bad() { echo "FAIL: $1"; fail=1; }
count() { objdump -d --no-show-raw-insn "$1" | awk -F'\t' -v p="$2" 'NF>=2{split($2,a," "); if(a[1] ~ p) n++} END{print n+0}'; }

for cpu in "" x86-64 auto x86-64-v3 znver3; do
    b="$T/b${cpu:-default}"
    "$CC" -q -O2 ${cpu:+--cpu=$cpu} "$SRC" -o "$b" || { bad "build --cpu=$cpu"; continue; }
    "$b" >/dev/null || bad "run --cpu=$cpu"
    above=$(count "$b" "$ABOVE"); test_=$(count "$b" '^(cpuid|xgetbv)$')
    ymm=$(objdump -d --no-show-raw-insn "$b" | grep -c ymm)
    case "$cpu" in
        ""|x86-64)        [ "$above" = 0 ] || bad "--cpu=$cpu has $above instructions above SSE2" ;;
        auto)             { [ "$ymm" -gt 0 ] && [ "$test_" -gt 0 ]; } || bad "--cpu=auto: ymm=$ymm test=$test_" ;;
        x86-64-v3|znver3) { [ "$ymm" -gt 0 ] && [ "$test_" = 0 ]; } || bad "--cpu=$cpu: ymm=$ymm test=$test_" ;;
    esac
done

"$CC" -q -O2 --cpu=nope "$SRC" -o "$T/x" 2>/dev/null && bad "--cpu=nope was accepted"
"$CC" -q -O2 --target=linux-aarch64 --cpu=znver3 "$SRC" -o "$T/x" 2>/dev/null && bad "--cpu=znver3 was accepted for aarch64"

if command -v qemu-x86_64 >/dev/null; then
    OLD="qemu64,-sse3,-ssse3,-sse4.1,-sse4.2,-avx,-avx2"
    "$CC" -q -O2 "$SMALL" -o "$T/old" && qemu-x86_64 -cpu "$OLD" "$T/old" >/dev/null 2>&1 || bad "default build on a CPU with only SSE2"
    "$CC" -q -O2 --cpu=auto "$SMALL" -o "$T/old" && qemu-x86_64 -cpu "$OLD" "$T/old" >/dev/null 2>&1 || bad "--cpu=auto on a CPU with only SSE2"
    "$CC" -q -O2 --cpu=x86-64-v3 "$SMALL" -o "$T/old" && qemu-x86_64 -cpu "$OLD" "$T/old" >/dev/null 2>&1 && bad "--cpu=x86-64-v3 ran without AVX2: the test above proves nothing"
fi
[ "$fail" = 0 ] && echo "check-cpu: ok"
exit "$fail"
