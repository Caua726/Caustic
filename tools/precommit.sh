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

# ---- installers ------------------------------------------------------------
# Defined here, not at the end: an installer-only commit takes the fast path
# out of this script, and that is exactly the commit whose installers need
# checking. Costs a couple of seconds, so both paths can afford to call it.
check_installers() {
    step "installer scripts"
    for f in install.sh update.sh uninstall.sh; do
        sh -n "$ROOT/$f" || die "$f is not valid POSIX shell"
    done
    # Parsing says nothing about whether the thing BLOCKS. install.sh grew
    # arrow-key menus that were reached whenever a terminal happened to be
    # attached, so `install.sh --user --format=elf` and even `--dry-run` sat
    # waiting on a keypress — a hang in any Makefile or provisioning script
    # that runs with a controlling terminal, with no output to explain it.
    #
    # The check needs a real pty, because without one the bug cannot happen.
    # `script` is how a shell gets one; where it is absent this is skipped
    # rather than quietly passed.
    if command -v script >/dev/null 2>&1; then
        for args in "--dry-run" "--yes --dry-run" "--user --format=elf --dry-run"; do
            if ! timeout 20 script -qec "sh '$ROOT/install.sh' $args" /dev/null \
                    </dev/null >/dev/null 2>&1; then
                die "install.sh $args did not finish on a terminal (blocked on a prompt?)"
            fi
        done
        ok "install.sh runs to completion on a tty when told what to do"
    fi
    # Non-ASCII would arrive mangled on Windows PowerShell 5.1, which reads a
    # BOM-less .ps1 with the system ANSI codepage.
    for f in install.ps1 update.ps1 uninstall.ps1; do
        if LC_ALL=C grep -qP '[^\x00-\x7F]' "$ROOT/$f" 2>/dev/null; then
            die "$f contains non-ASCII — Windows PowerShell 5.1 will mangle it"
        fi
    done
    if command -v pwsh >/dev/null 2>&1; then
        # Parsing catches the syntax; the compatibility rules catch the constructs
        # that parse here on 7 but do not exist on 5.1. Both classes of bug have
        # shipped before.
        ROOT="$ROOT" pwsh -NoProfile -Command '
            $bad = 0
            foreach ($f in @("install.ps1","update.ps1","uninstall.ps1")) {
                $errs = $null
                [void][System.Management.Automation.Language.Parser]::ParseFile(
                    (Join-Path $env:ROOT $f), [ref]$null, [ref]$errs)
                if ($errs) { $bad = 1; $errs | ForEach-Object { "  $f`:$($_.Extent.StartLineNumber): $($_.Message)" } }
            }
            if (Get-Module -ListAvailable PSScriptAnalyzer) {
                $s = @{ IncludeRules = @("PSUseCompatibleSyntax")
                        Rules = @{ PSUseCompatibleSyntax = @{ Enable = $true; TargetVersions = @("5.1","7.0") } } }
                foreach ($f in @("install.ps1","update.ps1","uninstall.ps1")) {
                    $r = Invoke-ScriptAnalyzer -Path (Join-Path $env:ROOT $f) -Settings $s
                    if ($r) { $bad = 1; $r | ForEach-Object { "  $f`:$($_.Line): $($_.Message)" } }
                }
            }
            exit $bad' \
            || die "PowerShell installer scripts have syntax or 5.1-compatibility errors"
        ok "install/update/uninstall parse as POSIX sh and as PowerShell 5.1 + 7"
    else
        ok "install/update/uninstall parse as POSIX sh (pwsh absent, .ps1 checked for ASCII only)"
    fi
}

# ---- fast path: nothing compiler-relevant staged ---------------------------
# (only when invoked as a hook, i.e. something is staged; a manual/CI run with
#  PRECOMMIT_FULL=1 always runs the full check.)
if [ "${PRECOMMIT_FULL:-0}" != "1" ]; then
    STAGED="$(git diff --cached --name-only 2>/dev/null)"
    if [ -n "$STAGED" ] && ! printf "%s\n" "$STAGED" | grep -qE '\.(cst|s)$|Causticfile|^tools/'; then
        if printf "%s\n" "$STAGED" | grep -qE '^(install|update|uninstall)\.(sh|ps1)$'; then
            check_installers
        fi
        echo "${D}pre-commit: no .cst/.s/Causticfile/tools changes staged — skipping self-check${N}"
        exit 0
    fi
fi

# ---- host guard --------------------------------------------------------------
# The gate self-hosts the toolchain, which needs it to run NATIVELY on this host.
# Caustic is developed and self-hosts on Linux; Windows/macOS are cross-compile
# *targets* (validated as targets by tools/check-cross.sh under wine, not by
# self-hosting here). So on a non-Linux host — e.g. running this hook under Git
# Bash on Windows — skip cleanly with a clear message instead of failing
# cryptically or wrongly blocking a commit.
HOST="$(uname -s 2>/dev/null || echo unknown)"
if [ "$HOST" != "Linux" ]; then
    echo "${Y}pre-commit: native self-host gate only runs on Linux (host is ${HOST}).${N}"
    echo "${D}  Caustic self-hosts on Linux; Windows/macOS are cross-compile targets.${N}"
    echo "${D}  Validate them with tools/check-cross.sh (wine/qemu) or run this gate on Linux.${N}"
    exit 0
fi

# Resolve the compiler (tolerate a .exe suffix if one is ever present).
CC=""
for c in "$ROOT/caustic" "$ROOT/caustic.exe"; do [ -x "$c" ] && { CC="$c"; break; }; done
[ -n "$CC" ] || die "caustic binary not found — build it first ( ./caustic-mk build caustic )"

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


# ─── 7. every target still builds ──────────────────────────────────────────
# Compiling for another architecture or OS needs no external tool — only
# RUNNING the result does, which is why check-cross.sh stays opt-in. Building
# them, though, gives the same verdict on every machine, so it belongs here.
#
# This exists because two real bugs shipped through a green gate: std/os/
# causticos.cst emitted a bare `mfence` and so could not be assembled for
# AArch64 at all, and ARM64 COFF relocations were translated through the AMD64
# table, linking a windows-aarch64 PE with unrelocated call sites. Nothing
# compiled either target, so nothing noticed.
step "cross-target builds (no external tools needed)"
CROSS_PROBE="$TMP/cross_probe.cst"
cat > "$CROSS_PROBE" <<'EOF'
use "std/io.cst" as io;
use "std/os.cst" as os;
fn main() as i32 { io.printf("%d\n", cast(i64, os.current)); return 0; }
EOF
for T in linux-x86_64 linux-aarch64 windows-x86_64 windows-aarch64 \
         caustic-x86_64 caustic-aarch64 caustic; do
    OUT="$TMP/cross_$T"
    if ! "$CUR" -q --target="$T" "$CROSS_PROBE" -o "$OUT" >"$TMP/cross.log" 2>&1; then
        tail -5 "$TMP/cross.log"; die "--target=$T failed to build"
    fi
    # A relocation the linker could not apply leaves the binary quietly wrong.
    if grep -q "unhandled .* relocation" "$TMP/cross.log"; then
        grep -m3 "unhandled .* relocation" "$TMP/cross.log"
        die "--target=$T left relocations unapplied"
    fi
done
# The shared stdlib is a shipped artifact in three flavours, and each one takes
# a different path through the backend. libcaustic.dll was impossible to produce
# at all until the syscall gate learned to honour --allow-unsupported: a shared
# build keeps every symbol, so std/os/linux.cst's wrappers are emitted whether or
# not the target has syscalls.
for SHARED in "linux-x86_64:so" "windows-x86_64:dll" "caustic-x86_64:csl"; do
    ST="${SHARED%%:*}"; SX="${SHARED##*:}"
    if ! "$CUR" -q --target="$ST" --shared --allow-unsupported \
            std/libcaustic.cst -o "$TMP/libcaustic.$SX" >"$TMP/shared.log" 2>&1; then
        tail -5 "$TMP/shared.log"; die "libcaustic.$SX ($ST) failed to build"
    fi
done
ok "linux/windows/caustic × x86_64/aarch64 all build clean; libcaustic.so/.dll/.csl too"

# ─── 8. CSE containers and the shared-library loader ───────────────────────
# The .csl form was broken in main for a whole release: its header grew and the
# loader kept reading the old offset, so csl_open failed for every library the
# writer produced. Nothing exercised it. This does.
step "CSE container + .csl round-trip"
cat > "$TMP/csl_lib.cst" <<'EOF'
fn cst_add(a as i64, b as i64) as i64 { return a + b; }
EOF
"$CUR" -q --target=caustic-x86_64 --shared --mode=pure "$TMP/csl_lib.cst" -o "$TMP/lib.csl" >/dev/null 2>&1 \
    || die "could not build a .csl"
cat > "$TMP/csl_use.cst" <<EOF
use "$ROOT/std/csl_loader.cst" as csl;
fn main() as i32 {
    if (csl.csl_open("$TMP/lib.csl") == 0) { return 10; }
    let is i64 as f = csl.csl_resolve("cst_add");
    if (f == 0) { return 11; }
    return cast(i32, call(cast(*u8, f), 40, 2));
}
EOF
"$CUR" -q "$TMP/csl_use.cst" -o "$TMP/csl_use" >/dev/null 2>&1 || die "could not build the .csl loader probe"
CSLRC="$(runrc "$TMP/csl_use")"
[ "$CSLRC" = "42" ] || die ".csl round-trip returned $CSLRC (10 = open failed, 11 = symbol missing)"

# The multi-arch container must carry one slice per architecture, each a
# complete image — a single-arch build wrapping one slice has the same shape.
"$CUR" -q --target=caustic "$CROSS_PROBE" -o "$TMP/fat" >/dev/null 2>&1 || die "multi-arch .cse failed to build"
# --target=caustic is a pure multi-arch container: CST_ at offset 0, no PE
# anywhere in it, so it is named ".cse". The other two names are kept because
# an older compiler being tested here would have produced them.
FATOUT="$TMP/fat.cse"
[ -f "$FATOUT" ] || FATOUT="$TMP/fat.cse.exe"
[ -f "$FATOUT" ] || FATOUT="$TMP/fat"
[ -f "$FATOUT" ] || die "multi-arch .cse produced no output under any known name"
NIMG=$(od -An -tu1 -j6 -N1 "$FATOUT" | tr -d ' ')
[ "$NIMG" = "2" ] || die "multi-arch .cse declares $NIMG images, expected 2"
ok ".csl resolves through csl_loader; container carries 2 slices"

check_installers

printf "\n${G}${B}pre-commit OK${N} — toolchain builds and self-hosts correctly at -O0/-O1/-O2.\n"
exit 0
