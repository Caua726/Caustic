#!/bin/sh
# Generated 300-module / 300+7 foreign-symbol consumers, with real ELF/DLL providers.
# Usage from the compiler root: sh tests/capacity/run_tests.sh [compiler]
set -eu
if [ "${1:-}" = --wine ]; then
    shift
    work=$1
    project="$work/project"
    for level in 0 1 2; do
        timeout 30 wine "$project/foreign-pe-$level.exe"
        echo "PE 300+7 foreign values: -O$level passed"
    done
    timeout 30 wine "$project/foreign-pe-separate.exe"
    cd "$project"
    for state in cold hot; do
        if [ "$state" = hot ]; then set -- --force; else set --; fi
        "$work/tools/caustic-mk" build pe --profile probe --target=windows-x86_64 --incremental -q "$@"
        timeout 30 wine "$project/build/windows-x86_64/maker-pe.exe"
        echo "PE maker $state 300+7 foreign values: passed"
    done
    exit 0
fi
root=$(pwd)
cc=${1:-${CAUSTIC_TEST_COMPILER:-./caustic}}
cc=$(command -v "$cc")
case "$cc" in /*) ;; *) cc="$root/$cc" ;; esac
work=$(mktemp -d "${TMPDIR:-/tmp}/caustic-capacity-tests.XXXXXX")
cleanup() { rm -rf "$work"; }
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
mkdir -p "$work/tools" "$work/objects"
ln -s "$cc" "$work/tools/caustic"
export PATH="$work/tools:$PATH" CAUSTIC_DIR="$work/tools" CAUSTIC_LIB="$root"
for name in assembler linker maker; do
    case "$name" in
        assembler) directory=caustic-assembler; binary=caustic-as ;;
        linker) directory=caustic-linker; binary=caustic-ld ;;
        maker) directory=caustic-maker; binary=caustic-mk ;;
    esac
    "$cc" -q -O1 "$directory/main.cst" -o "$work/tools/$binary"
done
"$cc" -q -O1 tests/capacity/generate.cst -o "$work/tools/generate"
"$work/tools/generate" "$work/project"
project="$work/project"
cp tests/capacity/Causticfile "$project/Causticfile"
cp tests/windows/missing_symbol.cst "$project/missing.cst"
for library in a b; do
    gcc -O2 -fPIC -shared "$project/library_$library.c" -o "$project/libcapacity_$library.so"
    x86_64-w64-mingw32-gcc -O2 -shared "$project/library_$library.c" -o "$project/capacity_$library.dll"
done
export LD_LIBRARY_PATH="$project${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
for level in 0 1 2; do
    "$cc" -q "-O$level" "$project/foreign_elf.cst" -o "$project/foreign-elf-$level"
    "$project/foreign-elf-$level"
    "$cc" -q "-O$level" --target=windows-x86_64 "$project/foreign_pe.cst" -o "$project/foreign-pe-$level.exe"
    echo "ELF 300+7 foreign values: -O$level passed"
done
"$cc" -q -O1 -c "$project/foreign_elf.cst" -o "$project/foreign-elf.o"
"$work/tools/caustic-ld" "$project/foreign-elf.o" -o "$project/foreign-elf-separate"
"$project/foreign-elf-separate"
"$cc" -q -O1 --target=windows-x86_64 -c "$project/foreign_pe.cst" -o "$project/foreign-pe.o"
"$work/tools/caustic-ld" --target=windows-x86_64 "$project/foreign-pe.o" -o "$project/foreign-pe-separate.exe"
(
    cd "$project"
    "$cc" -q -O1 -j 4 --module-objects "$work/objects" modules.cst
    "$work/tools/caustic-ld" "$work/objects"/*.o --entry=_modules_cst_main -o "$project/modules-multi"
    "$project/modules-multi"
    echo "300 modules / 301 separately emitted objects: passed"
    for target in modules elf; do
        for state in cold hot; do
            if [ "$state" = hot ]; then set -- --force; else set --; fi
            "$work/tools/caustic-mk" build "$target" --profile probe --incremental -q "$@"
            "$project/maker-$target"
            echo "ELF maker $state $target values: passed"
        done
    done
    for mode in quiet normal; do
        if [ "$mode" = quiet ]; then set -- -q; else set --; fi
        set +e
        "$work/tools/caustic-mk" build missing --profile probe --incremental --force "$@"
        status=$?
        set -e
        [ "$status" -eq 1 ] || { echo "maker $mode missing-symbol link exited $status, expected 1" >&2; exit 1; }
        [ ! -e maker-missing ] && [ ! -e maker-missing.tmp ]
    done
)
sh tests/windows/run_wine.sh sh "$root/tests/capacity/run_tests.sh" --wine "$work"
echo "Generated module and ELF/PE import capacity matrix: passed"
