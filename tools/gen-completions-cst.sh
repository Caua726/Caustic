#!/bin/sh
# tools/gen-completions-cst.sh — turn the completion scripts into Caustic source.
#
# `caustic-mk completions bash|zsh` used to print a script it composed on the
# spot, with the CURRENT project's target names baked in as a literal list. Copy
# that into ~/.zsh/completions and it describes one project forever — in every
# other checkout it completes names that do not exist and hides the ones that
# do. So the subcommand now prints the very scripts in tools/completions/, which
# read their names live, and this generator is what embeds them.
#
# Run it after editing tools/completions/caustic-mk.bash or _caustic-mk:
#
#     tools/gen-completions-cst.sh
#
# tools/completions/selftest.sh fails if the generated file is out of date, so
# forgetting is caught before the commit rather than after the release.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SRC_BASH="$ROOT/tools/completions/caustic-mk.bash"
SRC_ZSH="$ROOT/tools/completions/_caustic-mk"
OUT="$ROOT/caustic-maker/exec/completions_data.cst"

[ -f "$SRC_BASH" ] || { echo "error: missing $SRC_BASH" >&2; exit 1; }
[ -f "$SRC_ZSH" ]  || { echo "error: missing $SRC_ZSH" >&2; exit 1; }

# One c.print per line: a backslash and a double quote are the only two bytes a
# Caustic string literal cares about, and the trailing \n is the line ending.
emit_lines() {
    while IFS= read -r ln || [ -n "$ln" ]; do
        esc=$(printf '%s' "$ln" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g')
        printf '    c.print("%s\\n");\n' "$esc"
    done < "$1"
}

SUM=$(cat "$SRC_BASH" "$SRC_ZSH" | sha256sum | cut -d' ' -f1)

{
    echo "// GENERATED FILE — do not edit by hand."
    echo "// Produced by tools/gen-completions-cst.sh from:"
    echo "//   tools/completions/caustic-mk.bash"
    echo "//   tools/completions/_caustic-mk"
    echo "// Re-run that script after changing either of them; the completion"
    echo "// selftest compares this checksum and fails when they drift apart."
    echo "// SOURCE-SHA256 $SUM"
    echo ""
    echo 'use "../core/common.cst" as c;'
    echo ""
    echo "fn emit_bash() as void {"
    emit_lines "$SRC_BASH"
    echo "}"
    echo ""
    echo "fn emit_zsh() as void {"
    emit_lines "$SRC_ZSH"
    echo "}"
} > "$OUT.tmp"
mv "$OUT.tmp" "$OUT"

echo "wrote $OUT ($(wc -l < "$OUT") lines, source sha256 ${SUM%"${SUM#????????}"}…)"
