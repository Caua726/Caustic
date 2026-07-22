#!/usr/bin/env bash
#
# tools/release-build.sh — build the RELEASE toolchain + tarball, the proper way.
#
# The binaries that ship in a release are NOT "a build" — they are the settled 4th
# generation of an -O2 self-host, produced in four SEQUENTIAL phases (never
# interleaved), each at -O2:
#
#   1. compiler -O2            — a first -O2 compiler (seed) from the working tree.
#   2. compiler -O2 → gen4     — self-host the compiler to the 4th generation
#                                (gen2 == gen3 == gen4, byte-identical fixpoint).
#   3. toolchain -O2 → gen4    — build caustic-as / caustic-ld / caustic-mk /
#                                caustic-lsp to gen4, each compiled by that gen4
#                                compiler (deterministic → gen2 == gen3 == gen4).
#   4. compiler -O2 → gen4     — recompile the compiler to gen4 ONCE MORE, now on
#                                top of the settled gen4 toolchain. The shipped
#                                compiler is this phase-4 gen4 binary; the shipped
#                                tools are the phase-3 gen4 binaries.
#
# Only after phase 4 is caustic-x86_64-linux.tar.gz assembled (bin + std source +
# libcaustic.so/.csl, via the Causticfile `dist` script). -O2 is the release tier;
# the 4th generation is the maximally-settled fixpoint (gen2 already equals gen3,
# gen4 confirms it) — the strongest reproducibility guarantee the project can make
# about a released binary.
#
# Prereqs: a working ./caustic seed (any generation — `./caustic-mk build caustic`)
# and the version already bumped (see tools/prerelease.sh). Run from the repo.
#
# Usage:  tools/release-build.sh
# Tune:   REL_OPT=-O2                        (default; the release optimization tier)

set -euo pipefail
ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$ROOT" || exit 1

OPT="${REL_OPT:--O2}"

# ---- pretty output (mirrors tools/precommit.sh) ----------------------------
if [ -t 1 ]; then B=$'\e[1m'; G=$'\e[32m'; R=$'\e[31m'; Y=$'\e[33m'; D=$'\e[2m'; N=$'\e[0m'
else B=; G=; R=; Y=; D=; N=; fi
step() { printf "%s\n" "${B}▸ $*${N}"; }
ok()   { printf "  ${G}✓${N} %s\n" "$*"; }
die()  { printf "  ${R}✗ %s${N}\n" "$*"; printf "\n${R}${B}release build FAILED${N}\n"; exit 1; }
h16()  { sha256sum "$1" | cut -c1-16; }

[ -x ./caustic ] || die "no ./caustic seed — run ./caustic-mk build caustic first"

W=".release"; rm -rf "$W"; mkdir -p "$W"
trap 'rm -rf "$W"' EXIT INT TERM

# self-host the compiler from a seed to generation 4 at $OPT; verify the fixpoint.
# args: <seed-binary> <out-prefix> ; leaves <out-prefix>.1..4, echoes the gen4 path.
bootstrap_compiler() {
    local pfx="$2" prev="$1" g
    for g in 1 2 3 4; do
        "$prev" "$OPT" src/main.cst -o "$pfx.$g" >/dev/null 2>&1 || die "compiler self-compile failed at gen $g"
        prev="$pfx.$g"
    done
    cmp -s "$pfx.2" "$pfx.3" || die "compiler: gen2 != gen3 (non-deterministic self-host)"
    cmp -s "$pfx.3" "$pfx.4" || die "compiler: gen3 != gen4 (did not settle)"
}

# ---- PHASE 1 — compiler -O2 (seed) -----------------------------------------
step "Phase 1 — compiler $OPT (seed)"
./caustic "$OPT" src/main.cst -o "$W/seed" >/dev/null 2>&1 || die "seed -O2 compile failed"
ok "seed $OPT compiler  $(h16 "$W/seed")…"

# ---- PHASE 2 — compiler -O2 → generation 4 ---------------------------------
step "Phase 2 — compiler $OPT → generation 4 (self-host fixpoint)"
bootstrap_compiler "$W/seed" "$W/c"; CC="$W/c.4"
ok "compiler gen2==gen3==gen4  $(h16 "$CC")…"

# ---- PHASE 3 — toolchain -O2 → generation 4 (built by the gen4 compiler) ----
step "Phase 3 — toolchain $OPT → generation 4 (built by the gen4 compiler)"
TOOL_NAMES=(caustic-as                 caustic-ld               caustic-mk              caustic-lsp)
TOOL_SRCS=(caustic-assembler/main.cst  caustic-linker/main.cst  caustic-maker/main.cst  lsp/main.cst)
ti=0
while [ "$ti" -lt "${#TOOL_NAMES[@]}" ]; do
    n="${TOOL_NAMES[$ti]}"; s="${TOOL_SRCS[$ti]}"
    for g in 1 2 3 4; do
        "$CC" "$OPT" "$s" -o "$W/$n.$g" >/dev/null 2>&1 || die "$n compile failed at gen $g"
    done
    cmp -s "$W/$n.2" "$W/$n.3" && cmp -s "$W/$n.3" "$W/$n.4" || die "$n: gen2..gen4 not identical"
    cp "$W/$n.4" "./$n"                    # install the gen4 tool as the active toolchain
    ok "$n gen2==gen3==gen4  $(h16 "$W/$n.4")…"
    ti=$((ti+1))
done

# ---- PHASE 4 — compiler -O2 → generation 4 again (over the gen4 toolchain) --
step "Phase 4 — compiler $OPT → generation 4 again (over the settled gen4 toolchain)"
bootstrap_compiler "$CC" "$W/f"; FINAL="$W/f.4"
cmp -s "$FINAL" "$CC" || die "phase-4 compiler != phase-2 fixpoint (toolchain changed the output)"
cp "$FINAL" ./caustic                     # ship the phase-4 gen4 compiler
ok "final compiler gen2==gen3==gen4  $(h16 ./caustic)…  (== phase-2 fixpoint)"

# ---- clean generated artifacts so none leak into the tarball ---------------
find std -type f \( -name '*.s' -o -name '*.o' \) -delete 2>/dev/null || true
rm -f src/main.cst.s src/main.cst.s.o 2>/dev/null || true

# ---- package (bin + std source + libcaustic.so/.csl) -----------------------
step "Package caustic-x86_64-linux.tar.gz"
./caustic-mk run dist >/dev/null 2>&1 || die "dist packaging failed"
[ -f caustic-x86_64-linux.tar.gz ] || die "dist did not produce the tarball"
BAD="$(tar tzf caustic-x86_64-linux.tar.gz | grep -Ec '\.(s|o)$' || true)"
[ "$BAD" = "0" ] || die "tarball contains $BAD build artifacts (.s/.o) — std not clean"
ok "caustic-x86_64-linux.tar.gz — $(tar tzf caustic-x86_64-linux.tar.gz | wc -l) files, 0 artifacts"

printf "\n${G}${B}release build OK${N} — gen4 $OPT toolchain packaged (4-phase sequence).\n"
printf "  shipped compiler (phase 4): ${B}$(sha256sum caustic | cut -c1-16)…${N}\n"
printf "${D}Next: git tag v<version> && gh release create v<version> caustic-x86_64-linux.tar.gz${N}\n"
