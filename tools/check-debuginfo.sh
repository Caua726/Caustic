#!/usr/bin/env bash
#
# tools/check-debuginfo.sh — OPT-IN debug-info regression checks.
#
# These need external tools (llvm-dwarfdump, llvm-pdbutil, gdb, g++,
# llvm-readobj) that are NOT present on every machine, so they are
# DELIBERATELY kept out of the pre-commit gate — a commit gate must give the
# same verdict everywhere. Run this by hand or in CI when you have the tools:
#   tools/check-debuginfo.sh
#
# Behaviour: a check whose tool is missing is SKIPPED (not a failure). The
# script fails (exit 1) only when a tool IS present and its check is broken.
#
# What it pins, per area:
#   ELF DWARF   — llvm-dwarfdump --verify clean; enum / struct / param types
#                 present; a pointer parameter and a local read correctly at a
#                 breakpoint (the location list and the -g mem2reg skip); an
#                 enum prints by name.
#   PE PDB      — every llvm-pdbutil dump mode exits 0; LF_ENUM with its
#                 enumerators; one module per object in a multi-file link,
#                 each with its own source.
#   CFI         — FDEs in an ELF executable and an AArch64 binary; a C++
#                 exception thrown from a Caustic .so callback unwinds
#                 through it and reaches the caller's catch.

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
TMP="$(mktemp -d "${TMPDIR:-/tmp}/caustic-dbg.XXXXXX")"; trap 'rm -rf "$TMP"' EXIT
FAILED=0
cst() { "$CC" -q "$@" >/dev/null 2>&1; }
have() { command -v "$1" >/dev/null 2>&1; }

echo "${B}Caustic debug-info checks${N} ${D}(opt-in; never gates a commit)${N}"

# ─── fixtures ───────────────────────────────────────────────────────────────
cat > "$TMP/dbg.cst" <<'EOF'
enum Color { Red; Green; Blue; }
struct Point { x as i32; y as i32; }
fn dist2(p as *Point) as i32 {
    let is i32 as dx = p.x;
    let is i32 as dy = p.y;
    return dx + dy;
}
fn work(n as i64) as i64 {
    let is i64 as base = n + 1;
    let is i64 as other = base * 2;
    return other;
}
fn color_id(c as Color) as i32 {
    return cast(i32, c);
}
fn main() as i32 {
    let is Point as pt;
    pt.x = 30;
    pt.y = 12;
    let is Color as c = Color.Green;
    let is i32 as d = dist2(&pt);
    return d + cast(i32, work(5)) + color_id(c);
}
EOF
cat > "$TMP/lib.cst" <<'EOF'
fn libfn(n as i64) as i64 {
    let is i64 as r = n + 1;
    return r;
}
EOF
cat > "$TMP/main.cst" <<'EOF'
extern fn libfn(n as i64) as i64;
fn main() as i32 { return cast(i32, libfn(4)); }
EOF

# ─── ELF DWARF ──────────────────────────────────────────────────────────────
step "ELF DWARF (-g, -O2)"
if cst -g -O2 "$TMP/dbg.cst" -o "$TMP/dbg"; then
    if have llvm-dwarfdump; then
        llvm-dwarfdump --verify "$TMP/dbg" >/dev/null 2>&1 \
            && ok "llvm-dwarfdump --verify clean" || bad "llvm-dwarfdump --verify failed"
        llvm-dwarfdump --debug-info "$TMP/dbg" 2>/dev/null | grep -q 'DW_TAG_enumeration_type' \
            && ok "enum has a DW_TAG_enumeration_type" || bad "no DW_TAG_enumeration_type"
        llvm-dwarfdump --debug-info "$TMP/dbg" 2>/dev/null | grep -q 'DW_TAG_enumerator' \
            && ok "enum carries its enumerators" || bad "no DW_TAG_enumerator"
    else skip "llvm-dwarfdump not installed"; fi
    if have gdb; then
        # pointer parameter: read through it at the entry breakpoint
        gdb -q -batch -ex 'break dist2' -ex run -ex 'print p->x' -ex 'print p->y' "$TMP/dbg" 2>&1 \
            | grep -qE '\$1 = 30' && ok "pointer parameter reads through at entry" \
            || bad "pointer parameter wrong at entry"
        # local at -O2: valid after its assignment (the -g mem2reg skip)
        gdb -q -batch -ex 'break work' -ex run -ex next -ex next -ex 'print base' "$TMP/dbg" 2>&1 \
            | grep -qE '\$1 = 6' && ok "local reads at -O2 (base = 6)" \
            || bad "local wrong at -O2"
        # enum prints by name (a parameter, so the location list applies)
        gdb -q -batch -ex 'break color_id' -ex run -ex 'print c' "$TMP/dbg" 2>&1 \
            | grep -qE 'Green' && ok "enum prints by name (Green)" \
            || bad "enum did not print by name"
    else skip "gdb not installed"; fi
else bad "dbg.cst failed to build (-g -O2)"; fi

# Lines after the linker drops a dead function: separate -c -g objects, the
# library's first function unused. Its rows used to stay at the object's old
# offsets, so used_b reported unused_a's lines.
step "ELF lines after linker dead-code elimination"
cat > "$TMP/dlib.cst" <<'EOF'
fn unused_a(x as i64) as i64 {
    let is i64 as y with mut = x * 3;
    y = y + 7;
    return y;
}

fn used_b(x as i64) as i64 {
    let is i64 as z with mut = x + 1;
    z = z * 2;
    return z;
}
EOF
cat > "$TMP/dmain.cst" <<'EOF'
extern fn used_b(x as i64) as i64;
fn main() as i32 { return cast(i32, used_b(20)); }
EOF
LD="$ROOT/caustic-ld"
if [ -x "$LD" ] && cst -c -g "$TMP/dlib.cst" -o "$TMP/dlib.o" && cst -c -g "$TMP/dmain.cst" -o "$TMP/dmain.o" \
   && "$LD" -g "$TMP/dmain.o" "$TMP/dlib.o" -o "$TMP/dce" >/dev/null 2>&1; then
    if have gdb; then
        gdb -q -batch -ex 'break used_b' -ex run "$TMP/dce" 2>&1 \
            | grep -qE 'used_b .* at .*dlib\.cst:8' && ok "breakpoint in used_b at dlib.cst:8" \
            || bad "used_b's breakpoint line is wrong after dead-code elimination"
    else skip "gdb not installed"; fi
else bad "separate -g objects failed to build or link"; fi

# A module imported with `use`: its functions are built in the same run as the
# root, and used to have no lines (line numbers came only from the root's
# tokens, and the one object named only the root file).
step "ELF lines in an imported module (-g, -O0/-O2)"
cat > "$TMP/ulib.cst" <<'EOF'
fn twice(x as i64) as i64 {
    let is i64 as y = x * 2;
    return y;
}
EOF
cat > "$TMP/umain.cst" <<'EOF'
use "ulib.cst" as ulib;
fn main() as i32 {
    return cast(i32, ulib.twice(21));
}
EOF
for lv in -O0 -O2; do
    if cst -g $lv "$TMP/umain.cst" -o "$TMP/use$lv"; then
        if have gdb; then
            gdb -q -batch -ex 'break ulib.cst:2' -ex run -ex bt "$TMP/use$lv" 2>&1 \
                | grep -qE 'at .*ulib\.cst:2' && ok "$lv breakpoint at ulib.cst:2" \
                || bad "$lv no line in the imported module"
        else skip "gdb not installed"; fi
    else bad "$lv two-module -g build failed"; fi
done

# ─── PE PDB ─────────────────────────────────────────────────────────────────
step "PE PDB"
if have llvm-pdbutil; then
    if cst --target=windows-x86_64 "$TMP/dbg.cst" -o "$TMP/dbg.exe"; then
        for mode in --summary --streams --modules --files --symbols --publics \
                    --globals --section-contribs --fpo --named-streams; do
            if llvm-pdbutil dump "$mode" "$TMP/dbg.pdb" >/dev/null 2>"$TMP/e"; then
                ok "dump $mode exits 0"
            else bad "dump $mode failed: $(head -1 "$TMP/e")"; fi
        done
        llvm-pdbutil dump --types "$TMP/dbg.pdb" 2>/dev/null | grep -q 'LF_ENUM' \
            && ok "enum is an LF_ENUM" || bad "no LF_ENUM in the PDB"
        llvm-pdbutil dump --modules "$TMP/dbg.pdb" 2>/dev/null | grep -q '# files: 1' \
            && ok "module reports its source file" || bad "module reports no files"
    else bad "dbg.cst failed to cross-build (windows)"; fi
    # every instruction carrying a constant of tests/pdb/gen.py must come back,
    # through llvm-pdbutil and llvm-symbolizer, as the file and line that wrote it
    if have python3 && have llvm-symbolizer && have llvm-objdump; then
        ( mkdir -p "$TMP/pl" && cd "$TMP/pl" && python3 "$ROOT/tests/pdb/gen.py"
          for o in -O0 -O1 -O2; do
              for g in -g ""; do
                  rm -f l.exe l.pdb
                  "$CC" -q $g $o --target=windows-x86_64 b.cst -o l.exe && python3 "$ROOT/tests/pdb/check.py" l.exe l.pdb consts.json || exit 1
              done
              "$CC" -q -g $o -c --target=windows-x86_64 b.cst -o b.obj && "$ROOT/caustic-ld" --target=windows-x86_64 b.obj -o c.exe >/dev/null \
                  && python3 "$ROOT/tests/pdb/check.py" c.exe c.pdb consts.json || exit 1
          done ) > "$TMP/pl.out" 2>&1 && ok "PDB lines: every constant on its own file and line (-O0/-O1/-O2, -o and -c)" \
            || bad "PDB lines: $(grep -m1 FAIL "$TMP/pl.out")"
    else skip "PDB lines (python3, llvm-symbolizer, llvm-objdump)"; fi
    # multi-file: one module per object, each with its own source
    if cst --target=windows-x86_64 "$TMP/lib.cst" "$TMP/main.cst" -o "$TMP/multi.exe"; then
        n="$(llvm-pdbutil dump --modules "$TMP/multi.pdb" 2>/dev/null | grep -c 'debug stream:')"
        [ "$n" = 2 ] && ok "multi-file link has 2 modules" || bad "multi-file link has $n modules (want 2)"
        llvm-pdbutil dump --modules "$TMP/multi.pdb" 2>/dev/null | grep -q 'lib.cst' \
            && ok "lib.cst is its own module" || bad "lib.cst missing from the module list"
    else bad "multi-file windows link failed"; fi
else skip "llvm-pdbutil not installed"; fi

# ─── CFI ────────────────────────────────────────────────────────────────────
step "Call frame information"
if have llvm-readobj && cst "$TMP/dbg.cst" -o "$TMP/dbg.elf" 2>/dev/null; then
    n="$(llvm-readobj --unwind "$TMP/dbg.exe" 2>/dev/null | grep -c RuntimeFunction)"
    [ "${n:-0}" -ge 1 ] && ok "PE .pdata has $n RUNTIME_FUNCTION entries" || bad "PE .pdata has no entries"
fi
if have readelf && cst --target=linux-aarch64 "$TMP/dbg.cst" -o "$TMP/dbg.aa" 2>/dev/null; then
    n="$(readelf --debug-dump=frames "$TMP/dbg.aa" 2>/dev/null | grep -c 'FDE cie')"
    [ "${n:-0}" -ge 1 ] && ok "AArch64 .eh_frame has $n FDEs" || bad "AArch64 has no FDEs"
fi
if have g++; then
    cat > "$TMP/cbmid.cst" <<'EOF'
fn call_cb(cb as fn() as i64) as i64 {
    let is i64 as r = call(cb);
    return r;
}
EOF
    cat > "$TMP/exc.cpp" <<'EOF'
#include <cstdio>
extern "C" long call_cb(long (*cb)());
static long thrower() { throw 42; }
int main() {
    try { call_cb(thrower); }
    catch (int e) { printf("caught %d\n", e); return 0; }
    return 1;
}
EOF
    if cst --shared "$TMP/cbmid.cst" -o "$TMP/libcbmid.so" \
       && g++ -O0 -o "$TMP/exc" "$TMP/exc.cpp" -L"$TMP" -lcbmid -Wl,-rpath,"$TMP" 2>/dev/null; then
        out="$(LD_LIBRARY_PATH="$TMP" "$TMP/exc" 2>&1)"
        [ "$out" = "caught 42" ] && ok "C++ exception unwinds through a Caustic .so frame" \
            || bad "exception not caught through the .so: $out"
    else bad "exception fixture failed to build"; fi
else skip "g++ not installed"; fi

echo
if [ "$FAILED" = 0 ]; then echo "${G}debug-info checks OK${N} (present tools all pass)."
else echo "${R}debug-info checks FAILED${N} — a present tool is broken."; exit 1; fi
