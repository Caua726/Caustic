#!/usr/bin/env bash
#
# tools/precommit.sh — native, tool-independent self-check.
#
# Run by .git/hooks/pre-commit (and safe to run by hand or in CI). It proves the
# toolchain still BUILDS and SELF-HOSTS CORRECTLY at every optimization level
# before a commit is allowed — this is exactly the class of bug that a plain
# `-O1`-only bootstrap missed once (a compiler built at `-O2` silently
# miscompiled itself while `-O0`/`-O1` were fine).
#
# DELIBERATELY NATIVE-ONLY (x86_64): every check here runs on any machine with no
# external tools, so the verdict is identical everywhere. Cross-compilation
# checks (aarch64/qemu, windows/wine, C-interop/gcc) are environment-dependent
# and therefore live in tools/check-cross.sh, which NEVER gates a commit.
#
# It tests the WORKING TREE (what you'd get after the commit), building from
# source into a scratch dir — it never mutates tracked files or ./caustic.
#
# Skip:   PRECOMMIT_SKIP=1 git commit ...      (or  git commit --no-verify)
# Force:  PRECOMMIT_FULL=1  tools/precommit.sh  (run even with no .cst staged)

set -u
ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$ROOT" || exit 1

# ---- pretty output ---------------------------------------------------------
if [ -t 1 ]; then B=$'\e[1m'; G=$'\e[32m'; R=$'\e[31m'; Y=$'\e[33m'; D=$'\e[2m'; N=$'\e[0m'
else B=; G=; R=; Y=; D=; N=; fi
step() { printf "%s\n" "${B}▸ $*${N}"; }
ok()   { printf "  ${G}✓${N} %s\n" "$*"; }
skip() { printf "  ${Y}∼${N} %s\n" "$*"; }
die()  { printf "  ${R}✗ %s${N}\n" "$*"; printf "\n${R}${B}pre-commit FAILED${N} — commit blocked (use ${B}--no-verify${N} to override).\n"; exit 1; }

if [ "${PRECOMMIT_SKIP:-0}" = "1" ]; then echo "${Y}pre-commit skipped (PRECOMMIT_SKIP=1)${N}"; exit 0; fi

# ---- fast path: nothing compiler-relevant staged ---------------------------
# (only when invoked as a hook, i.e. something is staged; a manual/CI run with
#  PRECOMMIT_FULL=1 always runs the full check.)
if [ "${PRECOMMIT_FULL:-0}" != "1" ]; then
    STAGED="$(git diff --cached --name-only 2>/dev/null)"
    if [ -n "$STAGED" ] && ! printf "%s\n" "$STAGED" | grep -qE '\.(cst|s)$|Causticfile|^tools/'; then
        echo "${D}pre-commit: no .cst/.s/Causticfile/tools changes staged — skipping self-check${N}"
        exit 0
    fi
fi

CC="$ROOT/caustic"
[ -x "$CC" ] || die "./caustic not found — build it first ( ./caustic-mk build caustic )"

TMP="$(mktemp -d "${TMPDIR:-/tmp}/caustic-precommit.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT
PROBE="$TMP/probe.cst"; printf 'fn main() as i32 { return 42; }\n' > "$PROBE"

# compile $1(compiler) $2(src) $3(out) [extra flags...]; dies on failure
compile() { local cc="$1" src="$2" out="$3"; shift 3; "$cc" -q "$@" "$src" -o "$out" >/dev/null 2>&1 || return 1; }
# run a binary, echo its exit code
runrc() { "$1" </dev/null >/dev/null 2>&1; echo $?; }

echo "${B}Caustic pre-commit self-check${N} ${D}(native x86_64; scratch=$TMP)${N}"

# ─── 1. seed sanity ─────────────────────────────────────────────────────────
step "seed compiler sanity"
compile "$CC" "$PROBE" "$TMP/seed_probe" || die "./caustic cannot compile a trivial program"
[ "$(runrc "$TMP/seed_probe")" = "42" ] || die "./caustic produced a wrong trivial binary"
ok "./caustic compiles + runs a trivial program"

# ─── 2. sub-tools build from source ─────────────────────────────────────────
step "assembler / linker / maker build from source"
compile "$CC" "caustic-assembler/main.cst" "$TMP/caustic-as" || die "caustic-as failed to build"; ok "caustic-as"
compile "$CC" "caustic-linker/main.cst"    "$TMP/caustic-ld" || die "caustic-ld failed to build"; ok "caustic-ld"
compile "$CC" "caustic-maker/main.cst"     "$TMP/caustic-mk" || die "caustic-mk failed to build"; ok "caustic-mk"

# ─── 3. self-host fixpoint at every optimization level (4 generations) ──────
# THE core guard. For each -OX: gen1=seed·src, gen2=gen1·src, gen3=gen2·src,
# gen4=gen3·src. Require the fixpoint to hold to the FOURTH generation
# (gen2==gen3==gen4 byte-identical) AND the deepest generation — a compiler that
# was itself built by an -OX-built compiler — to actually WORK. gen4 at -O2 is a
# hard gate: the commit is NEVER allowed if that compiler is broken (this is
# exactly the failure mode the old -O1-only bootstrap could not see). gen4-O2
# then compiles the examples + test suite below, so "gen4 works" is proven by the
# whole corpus, not just a trivial probe.
CUR=""
for OPT in -O0 -O1 -O2; do
    step "self-host fixpoint $OPT (4 generations)"
    compile "$CC"          "src/main.cst" "$TMP/g1$OPT" "$OPT" || die "$OPT gen1 build failed"
    compile "$TMP/g1$OPT"  "src/main.cst" "$TMP/g2$OPT" "$OPT" || die "$OPT gen2 build failed"
    compile "$TMP/g2$OPT"  "src/main.cst" "$TMP/g3$OPT" "$OPT" || die "$OPT gen3 build failed"
    compile "$TMP/g3$OPT"  "src/main.cst" "$TMP/g4$OPT" "$OPT" || die "$OPT gen4 build failed"
    cmp -s "$TMP/g2$OPT" "$TMP/g3$OPT" || die "$OPT NOT a fixpoint (gen2 != gen3) — non-deterministic self-host"
    cmp -s "$TMP/g3$OPT" "$TMP/g4$OPT" || die "$OPT fixpoint breaks at gen4 (gen3 != gen4) — self-host not stable"
    compile "$TMP/g4$OPT" "$PROBE" "$TMP/p$OPT" || die "$OPT gen4 compiler cannot compile a program"
    [ "$(runrc "$TMP/p$OPT")" = "42" ] || die "$OPT gen4 compiler MISCOMPILES (wrong output) — this is the -O2-class bug"
    ok "$OPT gen2==gen3==gen4 byte-identical + gen4 output correct"
    [ "$OPT" = "-O2" ] && CUR="$TMP/g4$OPT"
done

# ─── 4. examples: identical behaviour across -O0/-O1/-O2 ────────────────────
# Catches any optimization that changes a real program's result. Uses the
# current-source -O2 compiler to compile each example at all three levels.
step "examples differential (-O0 vs -O1 vs -O2)"
# skipped: timing (bench/benchmark), entropy (random), non-compiling (version),
#          known pre-existing crash (ffi).
SKIP_RE='/(bench|benchmark|random|version|ffi)\.cst$'
nex=0
for f in examples/*.cst; do
    printf "%s\n" "$f" | grep -qE "$SKIP_RE" && continue
    # capture stdout to a file (NUL-safe) + exit code, for each level
    for OPT in -O0 -O1 -O2; do
        b="$TMP/exbin"
        if ! compile "$CUR" "$f" "$b" "$OPT"; then die "example $(basename "$f") failed to compile at $OPT"; fi
        "$b" </dev/null >"$TMP/out$OPT" 2>/dev/null; echo $? >"$TMP/rc$OPT"
    done
    if ! cmp -s "$TMP/out-O0" "$TMP/out-O1" || ! cmp -s "$TMP/out-O0" "$TMP/out-O2" \
       || [ "$(cat "$TMP/rc-O0")" != "$(cat "$TMP/rc-O1")" ] \
       || [ "$(cat "$TMP/rc-O0")" != "$(cat "$TMP/rc-O2")" ]; then
        die "example $(basename "$f") DIVERGES across opt levels (exit O0=$(cat "$TMP/rc-O0") O1=$(cat "$TMP/rc-O1") O2=$(cat "$TMP/rc-O2"))"
    fi
    nex=$((nex+1))
done
ok "$nex examples identical across -O0/-O1/-O2"

# ─── 5. test suite ──────────────────────────────────────────────────────────
step "test suite (tests/run_tests.cst)"
if ! "$CUR" -q "tests/run_tests.cst" -o "$TMP/tests" >/dev/null 2>&1; then die "test runner failed to build"; fi
if ! "$TMP/tests" > "$TMP/testout.txt" 2>&1; then
    tail -20 "$TMP/testout.txt"; die "test suite FAILED"
fi
grep -q "ALL PASSED" "$TMP/testout.txt" || { tail -20 "$TMP/testout.txt"; die "test suite did not report ALL PASSED"; }
ok "$(grep -oE 'pass=[0-9]+ fail=[0-9]+' "$TMP/testout.txt" | head -1)"

# ─── 6. toolchain smoke: freshly-built as + ld assemble+link+run ────────────
step "toolchain smoke (caustic-as + caustic-ld)"
"$CUR" -q "$PROBE" >/dev/null 2>&1 || die "could not emit .s for toolchain smoke"
"$TMP/caustic-as" "$TMP/probe.cst.s" >/dev/null 2>&1 || die "caustic-as failed on probe.s"
"$TMP/caustic-ld" "$TMP/probe.cst.s.o" -o "$TMP/probe_tc" >/dev/null 2>&1 || die "caustic-ld failed on probe.o"
[ "$(runrc "$TMP/probe_tc")" = "42" ] || die "as+ld toolchain produced a wrong binary"
rm -f "$TMP/probe.cst.s" "$TMP/probe.cst.s.o"
ok "caustic-as + caustic-ld assemble/link/run a program"

printf "\n${G}${B}pre-commit OK${N} — toolchain builds and self-hosts correctly at -O0/-O1/-O2.\n"
exit 0
