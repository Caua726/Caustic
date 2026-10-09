#!/bin/sh
# Permanent PE stack probes, target guards, mutable pointers and loader rebasing.
# Usage: sh tests/windows/run_tests.sh [compiler]
# --inside reuses an explicitly owned Wine/X session (mutation workers).
set -eu
if [ "${1:-}" != "--inside" ]; then
    exec sh "$(dirname "$0")/run_wine.sh" sh "$0" --inside "${1:-./caustic}"
fi
shift
cc=${1:-./caustic}
export LC_ALL=C
mkdir -p build/pe-tests
x86_64-w64-mingw32-gcc -std=c11 -O2 -Wall -Wextra -Werror \
    tests/windows/rebased_string_consumer.c -o build/pe-tests/rebased-string-consumer.exe
for level in 0 1 2; do
    "$cc" -q "-O$level" tests/logical_fold_test.cst -o "build/pe-tests/logical-$level"
    "build/pe-tests/logical-$level"
    "$cc" -q "-O$level" tests/windows/target_imports.cst -o "build/pe-tests/target-$level"
    "build/pe-tests/target-$level"
    readelf -d "build/pe-tests/target-$level" > "build/pe-tests/elf-$level.txt"
    needed=0
    while IFS= read -r line; do
        case "$line" in
            *'(NEEDED)'*)
                case "$line" in *'[libc.so.6]'*) needed=$((needed + 1));; *) echo "Wrong ELF import: $line" >&2; exit 1;; esac;;
        esac
    done < "build/pe-tests/elf-$level.txt"
    [ "$needed" -eq 1 ]
    "$cc" -q "-O$level" --target=windows-x86_64 tests/windows/target_imports.cst -o "build/pe-tests/target-$level.exe"
    objdump -p "build/pe-tests/target-$level.exe" > "build/pe-tests/pe-$level.txt"
    needed=0
    while IFS= read -r line; do
        case "$line" in
            *'DLL Name:'*)
                case "$line" in *'user32.dll'|*'kernel32.dll') needed=$((needed + 1));; *) echo "Wrong PE import: $line" >&2; exit 1;; esac;;
        esac
    done < "build/pe-tests/pe-$level.txt"
    [ "$needed" -eq 2 ]
    timeout 30 wine "build/pe-tests/target-$level.exe"
    "$cc" -q "-O$level" --target=windows-x86_64 tests/windows/stack_probe.cst -o "build/pe-tests/probe-$level.exe"
    timeout 30 wine "build/pe-tests/probe-$level.exe"
    "$cc" -q "-O$level" --target=windows-x86_64 tests/string_global_test.cst -o "build/pe-tests/strings-$level.exe"
    timeout 30 wine "build/pe-tests/strings-$level.exe"
    "$cc" -q "-O$level" --target=windows-x86_64 --shared tests/string_global_test.cst -o "build/pe-tests/strings-$level.dll"
    timeout 30 wine build/pe-tests/rebased-string-consumer.exe "build/pe-tests/strings-$level.dll"
    echo "PE target guards and 8192-byte recursive probe: -O$level passed"
done
"$cc" -q caustic-linker/main.cst -o build/pe-tests/linker
"$cc" -q --target=windows-x86_64 -c tests/windows/stack_probe.cst -o build/pe-tests/probe.o
build/pe-tests/linker --target=windows-x86_64 build/pe-tests/probe.o -o build/pe-tests/probe-separate.exe
timeout 30 wine build/pe-tests/probe-separate.exe
"$cc" -q --target=windows-x86_64 -O1 -c tests/string_global_test.cst -o build/pe-tests/strings.o
build/pe-tests/linker --target=windows-x86_64 build/pe-tests/strings.o -o build/pe-tests/strings-separate.exe
timeout 30 wine build/pe-tests/strings-separate.exe
"$cc" -q caustic-assembler/main.cst -o build/pe-tests/assembler
build/pe-tests/assembler tests/windows/section_bounds.s
build/pe-tests/linker --target=windows-x86_64 tests/windows/section_bounds.s.o -o build/pe-tests/section-bounds.exe
timeout 30 wine build/pe-tests/section-bounds.exe
# Rejected links must remove a prior successful executable, not just avoid a new one.
for target in linux-x86_64 windows-x86_64; do
    for fixture in missing_symbol missing_import_slot; do
        for mode in normal quiet; do
            "$cc" -q "--target=$target" tests/string_global_test.cst -o build/pe-tests/missing.exe
            if [ "$mode" = quiet ]; then set -- -q; else set --; fi
            set +e
            "$cc" "$@" "--target=$target" "tests/windows/$fixture.cst" -o build/pe-tests/missing.exe
            status=$?
            set -e
            [ "$status" -eq 1 ] || { echo "$target/$mode unresolved link exited $status, expected 1" >&2; exit 1; }
            [ ! -e build/pe-tests/missing.exe ] || { echo "$target/$mode retained a stale executable" >&2; exit 1; }
        done
        "$cc" -q "--target=$target" -c "tests/windows/$fixture.cst" -o build/pe-tests/missing.o
        "$cc" -q "--target=$target" tests/string_global_test.cst -o build/pe-tests/missing.exe
        set +e
        build/pe-tests/linker "--target=$target" build/pe-tests/missing.o -o build/pe-tests/missing.exe
        status=$?
        set -e
        [ "$status" -eq 1 ] || { echo "$target standalone unresolved link exited $status, expected 1" >&2; exit 1; }
        [ ! -e build/pe-tests/missing.exe ] || { echo "$target standalone retained a stale executable" >&2; exit 1; }
    done
done
echo "PE separate link and ELF/PE unresolved-symbol rejection: passed"
