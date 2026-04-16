#!/bin/bash
# Caustic Test Suite
# Tests the entire compiler toolchain: unit tests, bootstrap, assembler, linker, full toolchain.
# Run from repo root: bash tests/run_tests.sh

set -o pipefail

# --- Config ---
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# Use local binaries if present, otherwise system-installed
CAUSTIC="$REPO_ROOT/caustic"
if [ ! -f "$CAUSTIC" ]; then CAUSTIC="$(command -v caustic)"; fi
CAUSTIC_AS="$REPO_ROOT/caustic-as"
if [ ! -f "$CAUSTIC_AS" ]; then CAUSTIC_AS="$(command -v caustic-as)"; fi
CAUSTIC_LD="$REPO_ROOT/caustic-ld"
if [ ! -f "$CAUSTIC_LD" ]; then CAUSTIC_LD="$(command -v caustic-ld)"; fi

COMPILER_SRC="$REPO_ROOT/src/main.cst"
ASSEMBLER_SRC="$REPO_ROOT/caustic-assembler/main.cst"
LINKER_SRC="$REPO_ROOT/caustic-linker/main.cst"

TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

TOTAL_PASS=0
TOTAL_FAIL=0
PHASE_FAILED=0

# --- Colors ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BOLD='\033[1m'
NC='\033[0m'

# --- Helpers ---
pass() {
    echo -e "  ${GREEN}PASS${NC} $1"
    TOTAL_PASS=$((TOTAL_PASS + 1))
}

fail() {
    echo -e "  ${RED}FAIL${NC} $1${2:+ ($2)}"
    TOTAL_FAIL=$((TOTAL_FAIL + 1))
    PHASE_FAILED=1
}

phase_header() {
    echo ""
    echo -e "${BOLD}--- $1 ---${NC}"
}

step_ok() {
    echo -e "  $1... ${GREEN}ok${NC}"
}

step_fail() {
    echo -e "  $1... ${RED}FAILED${NC}${2:+ ($2)}"
    TOTAL_FAIL=$((TOTAL_FAIL + 1))
    PHASE_FAILED=1
}

check_file() {
    if [ ! -f "$1" ]; then
        echo -e "  ${RED}ERROR${NC}: Required file not found: $1"
        return 1
    fi
    return 0
}

# ============================================================
echo -e "${BOLD}=== Caustic Test Suite ===${NC}"

# Verify toolchain exists
for tool in "$CAUSTIC" "$CAUSTIC_AS" "$CAUSTIC_LD"; do
    if [ ! -x "$tool" ]; then
        echo -e "${RED}ERROR${NC}: Toolchain binary not found or not executable: $tool"
        echo "Build the toolchain first."
        exit 1
    fi
done

if [ ! -f "$COMPILER_SRC" ]; then
    echo -e "${RED}ERROR${NC}: Compiler source not found: $COMPILER_SRC"
    exit 1
fi

# ============================================================
# Phase 1: Unit Tests
# ============================================================
phase_header "Unit Tests"

UNIT_PASS=0
UNIT_FAIL=0
UNIT_SKIP=0

# Multi-file test: section_main needs section_driver_a and section_driver_b
MULTI_FILE_TESTS="section_main"

for testfile in "$REPO_ROOT"/tests/*.cst; do
    [ -f "$testfile" ] || continue
    basename=$(basename "$testfile" .cst)

    # Skip helper files that aren't standalone tests (no main function)
    if [ "$basename" = "section_driver_a" ] || [ "$basename" = "section_driver_b" ]; then
        continue
    fi

    outbin="$TMPDIR/caustic_test_${basename}"

    # Check if this is a multi-file test
    if [ "$basename" = "section_main" ]; then
        # Multi-file: compile each with -c, assemble, then link together
        driver_a="$REPO_ROOT/tests/section_driver_a.cst"
        driver_b="$REPO_ROOT/tests/section_driver_b.cst"

        compile_ok=1
        for src in "$driver_a" "$driver_b" "$testfile"; do
            "$CAUSTIC" -c "$src" > "$TMPDIR/compile_out" 2>&1
            if [ $? -ne 0 ]; then
                compile_ok=0
                break
            fi
            sfile="${src}.s"
            "$CAUSTIC_AS" "$sfile" > "$TMPDIR/compile_out" 2>&1
            if [ $? -ne 0 ]; then
                compile_ok=0
                break
            fi
            # Move .s and .o into TMPDIR to avoid polluting repo
            mv "${sfile}" "$TMPDIR/" 2>/dev/null
            mv "${sfile}.o" "$TMPDIR/" 2>/dev/null
        done

        if [ $compile_ok -eq 1 ]; then
            "$CAUSTIC_LD" \
                "$TMPDIR/$(basename "$driver_a").s.o" \
                "$TMPDIR/$(basename "$driver_b").s.o" \
                "$TMPDIR/$(basename "$testfile").s.o" \
                -o "$outbin" > "$TMPDIR/compile_out" 2>&1
            if [ $? -ne 0 ]; then
                compile_ok=0
            fi
        fi

        if [ $compile_ok -eq 0 ]; then
            fail "$basename" "compile/link error"
            UNIT_FAIL=$((UNIT_FAIL + 1))
            continue
        fi
    else
        # Single-file test: compile with -o (full pipeline)
        if ! "$CAUSTIC" "$testfile" -o "$outbin" > "$TMPDIR/compile_out" 2>&1; then
            fail "$basename" "compile error"
            UNIT_FAIL=$((UNIT_FAIL + 1))
            # Clean up any intermediate files the compiler may have left
            rm -f "${testfile}.s" "${testfile}.s.o" 2>/dev/null
            continue
        fi
        # Clean up intermediate files that -o may leave behind
        rm -f "${testfile}.s" "${testfile}.s.o" 2>/dev/null
    fi

    # Run the test binary
    "$outbin" > "$TMPDIR/test_stdout" 2>&1
    exit_code=$?

    if [ $exit_code -eq 0 ]; then
        pass "$basename"
        UNIT_PASS=$((UNIT_PASS + 1))
    else
        fail "$basename" "exit $exit_code"
        UNIT_FAIL=$((UNIT_FAIL + 1))
    fi
done

UNIT_TOTAL=$((UNIT_PASS + UNIT_FAIL))
echo -e "  Unit: ${UNIT_PASS}/${UNIT_TOTAL} passed"

if [ $UNIT_FAIL -ne 0 ]; then
    PHASE_FAILED=1
fi

# ============================================================
# Phase 2: Bootstrap Compiler (4 generations)
# ============================================================
phase_header "Bootstrap: Compiler"

bootstrap_ok=1

for gen in 1 2 3 4; do
    if [ $gen -eq 1 ]; then
        compiler="$CAUSTIC"
    else
        prev=$((gen - 1))
        compiler="$TMPDIR/caustic_gen${prev}"
    fi

    outbin="$TMPDIR/caustic_gen${gen}"
    label="Gen $gen"

    # Compile
    if ! "$compiler" "$COMPILER_SRC" > "$TMPDIR/bootstrap_out" 2>&1; then
        step_fail "$label" "compile failed"
        bootstrap_ok=0
        break
    fi

    # Assemble
    sfile="${COMPILER_SRC}.s"
    if ! "$CAUSTIC_AS" "$sfile" > "$TMPDIR/bootstrap_out" 2>&1; then
        step_fail "$label" "assemble failed"
        rm -f "$sfile" 2>/dev/null
        bootstrap_ok=0
        break
    fi

    # Link
    ofile="${sfile}.o"
    if ! "$CAUSTIC_LD" "$ofile" -o "$outbin" > "$TMPDIR/bootstrap_out" 2>&1; then
        step_fail "$label" "link failed"
        rm -f "$sfile" "$ofile" 2>/dev/null
        bootstrap_ok=0
        break
    fi

    # Clean up intermediates from repo
    rm -f "$sfile" "$ofile" 2>/dev/null

    step_ok "$label"
done

# Fixed-point check: gen3 == gen4
if [ $bootstrap_ok -eq 1 ]; then
    hash3=$(sha256sum "$TMPDIR/caustic_gen3" | awk '{print $1}')
    hash4=$(sha256sum "$TMPDIR/caustic_gen4" | awk '{print $1}')
    if [ "$hash3" = "$hash4" ]; then
        step_ok "Fixed point (gen3 == gen4)"
    else
        step_fail "Fixed point" "gen3 != gen4"
        bootstrap_ok=0
    fi
fi

if [ $bootstrap_ok -ne 1 ]; then
    PHASE_FAILED=1
    echo -e "  ${RED}Bootstrap compiler FAILED, skipping remaining phases.${NC}"
    echo ""
    echo -e "${BOLD}=== Results: ${RED}FAILED${NC} ${BOLD}===${NC}"
    echo "  Passed: $TOTAL_PASS"
    echo "  Failed: $TOTAL_FAIL"
    exit 1
fi

# Use gen4 compiler for remaining phases
GEN4="$TMPDIR/caustic_gen4"

# ============================================================
# Phase 3: Bootstrap Assembler
# ============================================================
phase_header "Bootstrap: Assembler"

as_ok=1

# Build assembler with gen4 compiler
if ! "$GEN4" "$ASSEMBLER_SRC" > "$TMPDIR/as_build_out" 2>&1; then
    step_fail "Compile" "gen4 failed to compile assembler"
    as_ok=0
fi

if [ $as_ok -eq 1 ]; then
    sfile="${ASSEMBLER_SRC}.s"
    if ! "$CAUSTIC_AS" "$sfile" > "$TMPDIR/as_build_out" 2>&1; then
        step_fail "Assemble" "caustic-as failed"
        as_ok=0
    fi

    if [ $as_ok -eq 1 ]; then
        ofile="${sfile}.o"
        if ! "$CAUSTIC_LD" "$ofile" -o "$TMPDIR/new_as" > "$TMPDIR/as_build_out" 2>&1; then
            step_fail "Link" "caustic-ld failed"
            as_ok=0
        fi
        rm -f "$ofile" 2>/dev/null
    fi
    rm -f "$sfile" 2>/dev/null
fi

if [ $as_ok -eq 1 ]; then
    step_ok "Build"

    # Test: compile a simple test, assemble with new_as, link, run
    # Use a simple test without custom sections to isolate assembler testing
    echo 'fn main() as i32 { return 0; }' > "$TMPDIR/as_test.cst"
    if ! "$GEN4" "$TMPDIR/as_test.cst" > "$TMPDIR/as_test_out" 2>&1; then
        step_fail "Test" "compile test file failed"
        as_ok=0
    fi

    if [ $as_ok -eq 1 ]; then
        if ! "$TMPDIR/new_as" "$TMPDIR/as_test.cst.s" > "$TMPDIR/as_test_out" 2>&1; then
            step_fail "Test" "new assembler failed to assemble"
            as_ok=0
        fi

        if [ $as_ok -eq 1 ]; then
            if ! "$CAUSTIC_LD" "$TMPDIR/as_test.cst.s.o" -o "$TMPDIR/as_test_bin" > "$TMPDIR/as_test_out" 2>&1; then
                step_fail "Test" "link failed"
                as_ok=0
            fi
        fi
    fi

    if [ $as_ok -eq 1 ]; then
        "$TMPDIR/as_test_bin" > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            step_ok "Test"
        else
            step_fail "Test" "binary exited non-zero"
            as_ok=0
        fi
    fi
fi

if [ $as_ok -ne 1 ]; then
    PHASE_FAILED=1
fi

# ============================================================
# Phase 4: Bootstrap Linker
# ============================================================
phase_header "Bootstrap: Linker"

ld_ok=1

# Build linker with gen4 compiler
if ! "$GEN4" "$LINKER_SRC" > "$TMPDIR/ld_build_out" 2>&1; then
    step_fail "Compile" "gen4 failed to compile linker"
    ld_ok=0
fi

if [ $ld_ok -eq 1 ]; then
    sfile="${LINKER_SRC}.s"
    if ! "$CAUSTIC_AS" "$sfile" > "$TMPDIR/ld_build_out" 2>&1; then
        step_fail "Assemble" "caustic-as failed"
        ld_ok=0
    fi

    if [ $ld_ok -eq 1 ]; then
        ofile="${sfile}.o"
        if ! "$CAUSTIC_LD" "$ofile" -o "$TMPDIR/new_ld" > "$TMPDIR/ld_build_out" 2>&1; then
            step_fail "Link" "caustic-ld failed"
            ld_ok=0
        fi
        rm -f "$ofile" 2>/dev/null
    fi
    rm -f "$sfile" 2>/dev/null
fi

if [ $ld_ok -eq 1 ]; then
    step_ok "Build"

    # Test: compile + assemble a test, link with new_ld, run
    test_src="$REPO_ROOT/tests/section_test.cst"
    test_ok=1

    if ! "$GEN4" "$test_src" > "$TMPDIR/ld_test_out" 2>&1; then
        step_fail "Test" "compile test file failed"
        test_ok=0
    fi

    if [ $test_ok -eq 1 ]; then
        test_s="${test_src}.s"
        # Use new_as if available, otherwise fall back to system caustic-as
        assembler="$CAUSTIC_AS"
        if [ -x "$TMPDIR/new_as" ]; then
            assembler="$TMPDIR/new_as"
        fi
        if ! "$assembler" "$test_s" > "$TMPDIR/ld_test_out" 2>&1; then
            step_fail "Test" "assemble failed"
            test_ok=0
        fi

        if [ $test_ok -eq 1 ]; then
            test_o="${test_s}.o"
            if ! "$TMPDIR/new_ld" "$test_o" -o "$TMPDIR/ld_test_bin" > "$TMPDIR/ld_test_out" 2>&1; then
                step_fail "Test" "new linker failed"
                test_ok=0
            fi
            rm -f "$test_o" 2>/dev/null
        fi
        rm -f "$test_s" 2>/dev/null
    fi

    if [ $test_ok -eq 1 ]; then
        "$TMPDIR/ld_test_bin" > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            step_ok "Test"
        else
            step_fail "Test" "binary exited non-zero"
            ld_ok=0
        fi
    else
        ld_ok=0
    fi
fi

if [ $ld_ok -ne 1 ]; then
    PHASE_FAILED=1
fi

# ============================================================
# Phase 5: Full Toolchain Test
# ============================================================
phase_header "Full Toolchain"

full_ok=1

# Rebuild the compiler using all-new tools:
# 1. Compile with gen4 compiler -> .s
# 2. Assemble with new_as -> .o
# 3. Link with new_ld -> binary
# 4. Use the result to compile and run a test

if [ ! -x "$TMPDIR/new_as" ] || [ ! -x "$TMPDIR/new_ld" ]; then
    step_fail "Build" "new_as or new_ld not available (previous phase failed)"
    full_ok=0
fi

if [ $full_ok -eq 1 ]; then
    # Step 1: Compile compiler source with gen4
    if ! "$GEN4" "$COMPILER_SRC" > "$TMPDIR/full_build_out" 2>&1; then
        step_fail "Build" "gen4 compile failed"
        full_ok=0
    fi
fi

if [ $full_ok -eq 1 ]; then
    # Step 2: Assemble with new_as
    sfile="${COMPILER_SRC}.s"
    if ! "$TMPDIR/new_as" "$sfile" > "$TMPDIR/full_build_out" 2>&1; then
        step_fail "Build" "new_as assemble failed"
        rm -f "$sfile" 2>/dev/null
        full_ok=0
    fi
fi

if [ $full_ok -eq 1 ]; then
    # Step 3: Link with new_ld
    ofile="${sfile}.o"
    if ! "$TMPDIR/new_ld" "$ofile" -o "$TMPDIR/caustic_full" > "$TMPDIR/full_build_out" 2>&1; then
        step_fail "Build" "new_ld link failed"
        rm -f "$sfile" "$ofile" 2>/dev/null
        full_ok=0
    fi
    rm -f "$sfile" "$ofile" 2>/dev/null
fi

if [ $full_ok -eq 1 ]; then
    step_ok "Build"

    # Step 4: Use fully-rebuilt compiler to compile and run a test
    test_src="$REPO_ROOT/tests/section_test.cst"

    if ! "$TMPDIR/caustic_full" "$test_src" -o "$TMPDIR/full_test_bin" > "$TMPDIR/full_test_out" 2>&1; then
        step_fail "Test" "full-toolchain compiler failed to compile test"
        # Clean up intermediates
        rm -f "${test_src}.s" "${test_src}.s.o" 2>/dev/null
        full_ok=0
    else
        # Clean up intermediates
        rm -f "${test_src}.s" "${test_src}.s.o" 2>/dev/null

        "$TMPDIR/full_test_bin" > /dev/null 2>&1
        if [ $? -eq 0 ]; then
            step_ok "Test"
        else
            step_fail "Test" "binary exited non-zero"
            full_ok=0
        fi
    fi
fi

if [ $full_ok -ne 1 ]; then
    PHASE_FAILED=1
fi

# ============================================================
# Final Summary
# ============================================================
echo ""
if [ $PHASE_FAILED -eq 0 ]; then
    echo -e "${BOLD}=== Results: ${GREEN}ALL PASSED${NC} ${BOLD}===${NC}"
else
    echo -e "${BOLD}=== Results: ${RED}FAILED${NC} ${BOLD}===${NC}"
fi
echo "  Unit tests: ${UNIT_PASS}/${UNIT_TOTAL} passed"
echo "  Bootstrap compiler: $([ $bootstrap_ok -eq 1 ] && echo 'ok' || echo 'FAILED')"
echo "  Bootstrap assembler: $([ $as_ok -eq 1 ] && echo 'ok' || echo 'FAILED')"
echo "  Bootstrap linker: $([ $ld_ok -eq 1 ] && echo 'ok' || echo 'FAILED')"
echo "  Full toolchain: $([ $full_ok -eq 1 ] && echo 'ok' || echo 'FAILED')"

if [ $PHASE_FAILED -ne 0 ]; then
    exit 1
fi
exit 0
