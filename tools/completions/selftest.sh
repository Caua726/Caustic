#!/usr/bin/env bash
#
# tools/completions/selftest.sh — proves the shell completions still answer what
# the toolchain actually accepts.
#
# The bash half runs the completion functions for real: COMP_WORDS/COMP_CWORD
# are built exactly the way readline builds them, INCLUDING the split at '='
# that COMP_WORDBREAKS forces ("--target=lin" arrives as three words). That
# split is not a detail — it is what silently broke every "--opt=value"
# completion before, and a test that hands the function one tidy word would not
# have noticed.
#
# The zsh half checks that every #compdef file parses under zsh, and — when a
# zsh with the zpty module is available — drives a real completion widget and
# compares what a user would see.
#
# Run:  tools/completions/selftest.sh          (from anywhere)
set -u

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../.." && pwd)

if [ -t 1 ]; then G=$'\e[32m'; R=$'\e[31m'; Y=$'\e[33m'; D=$'\e[2m'; N=$'\e[0m'
else G=; R=; Y=; D=; N=; fi
PASS=0; FAIL=0
ok()   { PASS=$((PASS+1)); [ -n "${VERBOSE:-}" ] && printf "  ${G}✓${N} %s\n" "$*"; return 0; }
bad()  { FAIL=$((FAIL+1)); printf "  ${R}✗${N} %s\n" "$*"; }
note() { printf "  ${Y}∼${N} %s\n" "$*"; }
head_() { printf "${D}%s${N}\n" "$*"; }

# ── the bash half ────────────────────────────────────────────────────────────

# Split a command line the way readline does before calling a completion
# function: on whitespace, and again at every '=' (which is in the default
# COMP_WORDBREAKS), with the '=' kept as a word of its own. Verified against a
# real interactive bash — see the comment above.
_split_line() {
    local line=$1 w rest
    SPLIT=()
    for w in $line; do
        rest=$w
        while [[ $rest == *=* ]]; do
            SPLIT+=( "${rest%%=*}" "=" )
            rest=${rest#*=}
        done
        [ -n "$rest" ] && SPLIT+=( "$rest" )
    done
    # A line ending in a space means the cursor sits on a new, empty word.
    [[ $line == *" " ]] && SPLIT+=( "" )
}

# complete <function> <line>   → one candidate per line on stdout
complete_line() {
    local fn=$1 line=$2
    local -a SPLIT
    _split_line "$line"
    COMP_WORDS=( "${SPLIT[@]}" )
    COMP_CWORD=$(( ${#SPLIT[@]} - 1 ))
    COMP_LINE=$line
    COMP_POINT=${#line}
    COMPREPLY=()
    # bash passes: $1 command, $2 current word, $3 previous word.
    "$fn" "${COMP_WORDS[0]}" "${COMP_WORDS[COMP_CWORD]}" "${COMP_WORDS[COMP_CWORD-1]:-}" 2>/dev/null
    [ ${#COMPREPLY[@]} -eq 0 ] || printf '%s\n' "${COMPREPLY[@]}"
}

# has <fn> <line> <candidate>...  — every candidate must be offered
has() {
    local fn=$1 line=$2; shift 2
    local out c
    out=$(complete_line "$fn" "$line")
    for c in "$@"; do
        if ! printf '%s\n' "$out" | grep -qxF -- "$c"; then
            bad "\`$line\` should offer '$c'"
            printf "${D}      got: %s${N}\n" "$(printf '%s ' $out)"
            return 1
        fi
    done
    ok "\`$line\` offers $*"
}

# hasnt <fn> <line> <candidate>...  — none of them may be offered
hasnt() {
    local fn=$1 line=$2; shift 2
    local out c
    out=$(complete_line "$fn" "$line")
    for c in "$@"; do
        if printf '%s\n' "$out" | grep -qxF -- "$c"; then
            bad "\`$line\` must NOT offer '$c'"
            return 1
        fi
    done
    ok "\`$line\` withholds $*"
}

# empty <fn> <line> — nothing at all
empty() {
    local out
    out=$(complete_line "$1" "$2")
    if [ -n "$out" ]; then
        bad "\`$2\` should offer nothing, got: $(printf '%s ' $out)"
        return 1
    fi
    ok "\`$2\` offers nothing"
}

head_ "bash: parsing"
for f in "$DIR"/*.bash; do
    if bash -n "$f"; then ok "${f##*/} parses"; else bad "${f##*/} is not valid bash"; fi
done

# The four scripts define the same helper names, and a shell that sources all of
# them keeps only the last definition of each. If they ever drift, whichever
# file loses the race starts completing with someone else's semantics.
head_ "bash: the shared helper block is identical everywhere"
sum=""
for f in "$DIR"/caustic.bash "$DIR"/caustic-as.bash "$DIR"/caustic-ld.bash "$DIR"/caustic-mk.bash; do
    s=$(awk '/^# ── shared helpers/,/^# ── end shared helpers/' "$f" | md5sum | cut -d' ' -f1)
    if [ -z "$sum" ]; then sum=$s
    elif [ "$s" != "$sum" ]; then bad "${f##*/} carries a different shared helper block"; s=""; fi
    [ -n "$s" ] && ok "${f##*/} shared block matches"
done

# `caustic-mk completions <shell>` prints these very scripts, embedded by
# tools/gen-completions-cst.sh. Editing one without re-running the generator
# would ship a binary that hands out a stale completion — the failure this
# checksum exists to make loud.
head_ "caustic-mk: the embedded copy is in step with the scripts"
GEN="$ROOT/caustic-maker/exec/completions_data.cst"
if [ -f "$GEN" ]; then
    want=$(cat "$DIR/caustic-mk.bash" "$DIR/_caustic-mk" | sha256sum | cut -d' ' -f1)
    have=$(sed -n 's|^// SOURCE-SHA256 ||p' "$GEN")
    if [ "$want" = "$have" ]; then
        ok "completions_data.cst matches the scripts"
    else
        bad "completions_data.cst is stale — run tools/gen-completions-cst.sh"
    fi
else
    note "no caustic-maker/exec/completions_data.cst — embedded-copy check skipped"
fi
if [ -x "$ROOT/caustic-mk" ]; then
    for sh in bash zsh; do
        f="$DIR/caustic-mk.bash"; [ "$sh" = zsh ] && f="$DIR/_caustic-mk"
        if "$ROOT/caustic-mk" completions "$sh" 2>/dev/null | diff -q - "$f" >/dev/null; then
            ok "\`caustic-mk completions $sh\` prints the shipped script"
        else
            bad "\`caustic-mk completions $sh\` differs from ${f##*/} (rebuild caustic-mk?)"
        fi
    done
fi

# A sandbox with one file of every kind the toolchain cares about, plus a
# manifest, so file-type filters and the Causticfile reader are tested against
# something real instead of whatever the source tree happens to contain.
FIX=$(mktemp -d "${TMPDIR:-/tmp}/caustic-comp.XXXXXX")
trap 'rm -rf "$FIX"' EXIT
: > "$FIX/hello.cst"; : > "$FIX/other.cst"
: > "$FIX/hello.cst.s"; : > "$FIX/hello.cst.s.o"; : > "$FIX/win.obj"
: > "$FIX/pure.cse"; : > "$FIX/poly.cse.exe"; : > "$FIX/win.exe"
: > "$FIX/notes.txt"
mkdir -p "$FIX/build"
cat > "$FIX/Causticfile" <<'EOF'
name "fixture"
set MODE "release"
set TRIPLE "linux-x86_64"
target "app"  { src "hello.cst" out "app" }
target "tool" { src "other.cst" out "tool" }
profile "release" { flags "-O2" }
profile "debug"   { flags "-O0" }
script "test" { "echo hi" }
script "dist" { "echo dist" }
EOF
cd "$FIX" || exit 1

. "$DIR/caustic.bash"
. "$DIR/caustic-as.bash"
. "$DIR/caustic-ld.bash"
. "$DIR/caustic-lsp.bash"
. "$DIR/caustic-mk.bash"

head_ "caustic"
has  _caustic 'caustic ' hello.cst other.cst
hasnt _caustic 'caustic ' notes.txt hello.cst.s
has  _caustic 'caustic -' -c -o -O1 -O2 --shared --target= --emit-csti --module-objects
has  _caustic 'caustic --version' --version                 # only as argv[1]
hasnt _caustic 'caustic hello.cst --vers' --version
# The '=' split: this is the case that used to fall through to file completion.
has  _caustic 'caustic --target=' linux-x86_64 windows-x86_64 caustic-x86_64 wasm32-wasi
has  _caustic 'caustic --target=w' windows-x86_64 windows-aarch64 wasm32-wasi wasm64-wasi
hasnt _caustic 'caustic --target=w' linux-x86_64 hello.cst
has  _caustic 'caustic --mode=' pure compat bundle
has  _caustic 'caustic --mode=c' compat
has  _caustic 'caustic --extension=' exe cse
has  _caustic 'caustic --stack-size=' 65536 1048576
has  _caustic 'caustic -O' -O0 -O1 -O2
has  _caustic 'caustic -l' -lcaustic
# -o suggests the output name this target actually produces.
has  _caustic 'caustic hello.cst -o ' hello
has  _caustic 'caustic --target=windows-x86_64 hello.cst -o ' hello.exe
has  _caustic 'caustic --target=wasm32-wasi hello.cst -o ' hello.wasm
has  _caustic 'caustic --shared hello.cst -o ' libhello.so
has  _caustic 'caustic --target=windows-x86_64 --shared hello.cst -o ' libhello.dll
has  _caustic 'caustic --target=caustic-x86_64 --shared hello.cst -o ' libhello.csl
# --cache takes a directory: the fixture's only one, and no file.
has   _caustic 'caustic --cache ' build
hasnt _caustic 'caustic --cache ' hello.cst notes.txt

head_ "caustic-as"
has  _caustic_as 'caustic-as ' hello.cst.s
hasnt _caustic_as 'caustic-as ' hello.cst notes.txt
has  _caustic_as 'caustic-as -' --profile --target=
has  _caustic_as 'caustic-as --target=li' linux-x86_64 linux-aarch64

head_ "caustic-ld"
has  _caustic_ld 'caustic-ld ' hello.cst.s.o win.obj
hasnt _caustic_ld 'caustic-ld ' hello.cst notes.txt
has  _caustic_ld 'caustic-ld -' -o --strip --shared --cse-combine --cse-fat --image= --entry=
has  _caustic_ld 'caustic-ld --target=' linux-x86_64 caustic-x86_64-aarch64
# A pure CSE image is <n>.cse, a polyglot one <n>.cse.exe — both are --image=.
has  _caustic_ld 'caustic-ld --cse-fat --image=' pure.cse poly.cse.exe
has  _caustic_ld 'caustic-ld --cse-combine --pe=' win.exe poly.cse.exe
hasnt _caustic_ld 'caustic-ld --cse-fat --image=' notes.txt
has  _caustic_ld 'caustic-ld -l' -lcaustic -lc -lm

head_ "caustic-lsp"
empty _caustic_lsp 'caustic-lsp '
empty _caustic_lsp 'caustic-lsp -'

head_ "caustic-mk (names read from the Causticfile)"
has  _caustic_mk 'caustic-mk ' build run test list info why graph watch install clean init doctor completions
has  _caustic_mk 'caustic-mk -' -h --help -V --version
has  _caustic_mk 'caustic-mk build ' all app tool
hasnt _caustic_mk 'caustic-mk build ' test dist
has  _caustic_mk 'caustic-mk run ' app tool test dist
has  _caustic_mk 'caustic-mk info ' app tool
has  _caustic_mk 'caustic-mk why ' app tool
has  _caustic_mk 'caustic-mk completions ' bash zsh
has  _caustic_mk 'caustic-mk build -' -j --incremental --force --target= --profile --define
has  _caustic_mk 'caustic-mk clean -' --cache
has  _caustic_mk 'caustic-mk graph -' --dot
has  _caustic_mk 'caustic-mk watch -' --interval
hasnt _caustic_mk 'caustic-mk build -' --cache --dot --interval
has  _caustic_mk 'caustic-mk build --profile ' release debug
has  _caustic_mk 'caustic-mk build --target=' linux-x86_64 caustic-x86_64
# A flag's value is not the command's positional: the target list must survive.
has  _caustic_mk 'caustic-mk build --profile release ' app tool
has  _caustic_mk 'caustic-mk build -j 4 ' app tool
# One name only — a second positional is an error, so offer nothing.
empty _caustic_mk 'caustic-mk build app '
# run/test forward everything after the name to the child.
has  _caustic_mk 'caustic-mk run app ' hello.cst notes.txt
has  _caustic_mk 'caustic-mk test ' hello.cst notes.txt
has  _caustic_mk 'caustic-mk run app -- ' hello.cst
# --define: names first, then that key's current value.
has  _caustic_mk 'caustic-mk build -D ' MODE= TRIPLE=
has  _caustic_mk 'caustic-mk build --define ' MODE= TRIPLE=
has  _caustic_mk 'caustic-mk build -D MODE=' release
has  _caustic_mk 'caustic-mk build --define=TRIPLE=' linux-x86_64
empty _caustic_mk 'caustic-mk list '
empty _caustic_mk 'caustic-mk doctor '

# The same answers, but sourced from a live `caustic-mk list` rather than from
# the manifest reader — the two must agree.
if [ -x "$ROOT/caustic-mk" ]; then
    head_ "caustic-mk (names read from a live \`caustic-mk list\`)"
    PATH="$ROOT:$PATH"
    has _caustic_mk 'caustic-mk build ' all app tool
    has _caustic_mk 'caustic-mk run ' app tool test dist
    has _caustic_mk 'caustic-mk build --profile ' release debug
else
    note "no built caustic-mk at $ROOT — live-list check skipped"
fi

# ── the zsh half ─────────────────────────────────────────────────────────────
cd "$ROOT" || exit 1
if command -v zsh >/dev/null 2>&1; then
    head_ "zsh: parsing"
    for f in "$DIR"/_*; do
        case "$f" in *.zwc) continue ;; esac
        if zsh -n "$f" 2>/dev/null; then ok "${f##*/} parses"; else bad "${f##*/} is not valid zsh"; fi
    done

    head_ "zsh: real completion through zpty"
    if [ -n "${SKIP_ZPTY:-}" ]; then
        note "SKIP_ZPTY set — functional zsh check skipped"
    elif ! zsh -fc 'zmodload zsh/zpty' 2>/dev/null; then
        note "zsh has no zpty module — functional zsh check skipped"
    else
        ZOUT="$FIX/zpty.out"
        ( cd "$FIX" && FPDIR="$DIR" zsh -f "$DIR/zpty-probe.zsh" ) >"$ZOUT" 2>/dev/null
        if ! grep -q '<<<CASE' "$ZOUT" 2>/dev/null; then
            note "zpty probe produced no output — skipped"
        else
            # Everything between "<<<CASE <line>" and ">>>END" is what zsh
            # listed for that line.
            zhas() {
                local line=$1; shift
                local body c
                body=$(awk -v pat="<<<CASE $line" '
                    index($0, pat) == 1 && length($0) == length(pat) { f = 1; next }
                    /^>>>END$/ { if (f) exit }
                    f { print }' "$ZOUT")
                if [ -z "$body" ]; then bad "zsh: no listing captured for \`$line\`"; return 1; fi
                for c in "$@"; do
                    if ! printf '%s\n' "$body" | grep -qE "(^|[[:space:]])$(printf '%s' "$c" | sed 's/[.[\*^$]/\\&/g')([[:space:]]|$)"; then
                        bad "zsh: \`$line\` should offer '$c'"
                        return 1
                    fi
                done
                ok "zsh: \`$line\` offers $*"
            }
            zhas 'caustic --target=' linux-x86_64 windows-x86_64 caustic-x86_64 wasm32-wasi
            zhas 'caustic -' --shared --target --emit-csti -O2
            zhas 'caustic --mode=' pure compat bundle
            zhas 'caustic-as --target=' linux-aarch64
            zhas 'caustic-ld -' --cse-fat --cse-combine --strip
            zhas 'caustic-mk ' build run test completions doctor
            zhas 'caustic-mk build ' all app tool
            zhas 'caustic-mk run ' app tool dist
            zhas 'caustic-mk completions ' bash zsh
        fi
    fi
else
    note "zsh not installed — zsh checks skipped"
fi

printf "\n"
if [ "$FAIL" -eq 0 ]; then
    printf "${G}completions OK${N} — %d checks passed\n" "$PASS"
    exit 0
fi
printf "${R}completions FAILED${N} — %d passed, %d failed\n" "$PASS" "$FAIL"
exit 1
