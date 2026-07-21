#!/usr/bin/env bash
#
# tools/check-cross.sh — OPT-IN cross-target / interop checks.
#
# These need external tools (qemu-aarch64, wine, gcc) that are NOT present on
# every machine, so they are DELIBERATELY kept out of the pre-commit gate — a
# commit gate must give the same verdict everywhere. Run this by hand or in CI
# when you have the tools:  tools/check-cross.sh
#
# Behaviour: a target whose tool is missing is SKIPPED (not a failure). The
# script fails (exit 1) only when a tool IS present and its target is broken.

set -u
ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"; cd "$ROOT" || exit 1

if [ -t 1 ]; then B=$'\e[1m'; G=$'\e[32m'; R=$'\e[31m'; Y=$'\e[33m'; D=$'\e[2m'; N=$'\e[0m'
else B=; G=; R=; Y=; D=; N=; fi
step() { printf "%s\n" "${B}▸ $*${N}"; }
ok()   { printf "  ${G}✓${N} %s\n" "$*"; }
skip() { printf "  ${Y}∼ skip${N} %s\n" "$*"; }
bad()  { printf "  ${R}✗ %s${N}\n" "$*"; FAILED=1; }

CC="$ROOT/caustic"
[ -x "$CC" ] || { echo "${R}./caustic not found — build it first${N}"; exit 1; }
TMP="$(mktemp -d "${TMPDIR:-/tmp}/caustic-cross.XXXXXX")"; trap 'rm -rf "$TMP"' EXIT
FAILED=0
cst() { "$CC" -q "$@" >/dev/null 2>&1; }

echo "${B}Caustic cross-target checks${N} ${D}(opt-in; never gates a commit)${N}"

# ─── aarch64 via qemu-user ──────────────────────────────────────────────────
step "linux-aarch64 (qemu-aarch64)"
if command -v qemu-aarch64 >/dev/null 2>&1; then
    # native reference vs cross result must match for a few programs
    for t in examples/test_sum.cst tests/aarch64/scalar_abi.cst tests/aarch64/linux_syscalls.cst; do
        [ -f "$t" ] || continue
        if ! cst "$t" -o "$TMP/nat"; then bad "$t failed to build (native)"; continue; fi
        "$TMP/nat" </dev/null >/dev/null 2>&1; nrc=$?
        if ! cst --target=linux-aarch64 "$t" -o "$TMP/aa"; then bad "$t failed to cross-build (aarch64)"; continue; fi
        qemu-aarch64 "$TMP/aa" </dev/null >/dev/null 2>&1; arc=$?
        if [ "$nrc" = "$arc" ]; then ok "$(basename "$t") native==aarch64 (exit $arc)"
        else bad "$(basename "$t") native($nrc) != aarch64($arc)"; fi
    done
    # cross-build the compiler itself and run --version under qemu
    if cst --target=linux-aarch64 --cache "$TMP/aacache" src/main.cst -o "$TMP/caustic-aa"; then
        if qemu-aarch64 "$TMP/caustic-aa" --version >/dev/null 2>&1; then ok "aarch64 self-cross-build runs (--version)"
        else bad "aarch64 cross-built compiler does not run"; fi
    else bad "compiler failed to cross-build for aarch64"; fi
else skip "qemu-aarch64 not installed"; fi

# ─── windows via wine ───────────────────────────────────────────────────────
step "windows-x86_64 (wine)"
if command -v wine >/dev/null 2>&1; then
    export WINEDEBUG="${WINEDEBUG:-fixme-all,err-all}"
    # hello.exe should run and exit 0
    if cst --target=windows-x86_64 tests/windows/hello.cst -o "$TMP/hello.exe"; then
        wine "$TMP/hello.exe" </dev/null >/dev/null 2>&1
        [ $? = 0 ] && ok "hello.exe runs (exit 0)" || bad "hello.exe wrong exit"
    else bad "hello.cst failed to cross-build"; fi
    # recurse: windows result must match native
    if cst tests/windows/recurse.cst -o "$TMP/rec_nat" && cst --target=windows-x86_64 tests/windows/recurse.cst -o "$TMP/rec.exe"; then
        "$TMP/rec_nat" </dev/null >/dev/null 2>&1; nrc=$?
        wine "$TMP/rec.exe" </dev/null >/dev/null 2>&1; wrc=$?
        [ "$nrc" = "$wrc" ] && ok "recurse native==windows (exit $wrc)" || bad "recurse native($nrc) != windows($wrc)"
    else bad "recurse cross-build failed"; fi
    # exit42: a linux generator that writes a hand-rolled PE, run it under wine
    if cst tests/windows/exit42.cst -o "$TMP/gen42" && "$TMP/gen42" >/dev/null 2>&1; then
        wine /tmp/exit42.exe </dev/null >/dev/null 2>&1
        [ $? = 42 ] && ok "hand-rolled exit42.exe -> 42" || bad "exit42.exe wrong exit"
    else bad "exit42 generator failed"; fi
else skip "wine not installed"; fi

# ─── C interop via gcc (shared .so callable from C, System V ABI) ───────────
step "C interop (gcc + .so)"
if command -v gcc >/dev/null 2>&1; then
    printf 'fn cst_add(a as i32, b as i32) as i32 { return a + b; }\n' > "$TMP/lib.cst"
    printf 'extern int cst_add(int,int);\nint main(void){ return cst_add(40,2); }\n' > "$TMP/cmain.c"
    if cst --shared "$TMP/lib.cst" -o "$TMP/libfoo.so"; then
        if gcc "$TMP/cmain.c" -L"$TMP" -lfoo -o "$TMP/cmain" 2>/dev/null; then
            LD_LIBRARY_PATH="$TMP" "$TMP/cmain" </dev/null >/dev/null 2>&1
            [ $? = 42 ] && ok "C program links + calls a Caustic .so (cst_add(40,2)=42)" || bad "C interop wrong result"
        else bad "gcc could not link against the Caustic .so"; fi
    else bad ".so failed to build (--shared)"; fi
else skip "gcc not installed"; fi

echo
if [ "$FAILED" = 0 ]; then printf "${G}${B}cross checks OK${N} (present targets all pass).\n"; exit 0
else printf "${R}${B}cross checks FAILED${N} — a present target is broken.\n"; exit 1; fi
