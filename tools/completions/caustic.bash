# caustic — bash completion.  Flags mirror src/main.cst:parse_args.
#
# install:  <prefix>/share/bash-completion/completions/caustic   (loaded on demand)
#     or:   . tools/completions/caustic.bash                     (from ~/.bashrc)

# ── shared helpers ───────────────────────────────────────────────────────────
# Defined identically in caustic.bash, caustic-as.bash, caustic-ld.bash and
# caustic-mk.bash. The scripts share one shell namespace when they are sourced
# together, where the last definition of a name is the one that runs — so these
# blocks must stay byte-identical (tools/completions/selftest.sh checks it).
#
# COMP_WORDBREAKS contains '=', so bash hands "--target=lin" to us as the three
# words "--target" "=" "lin". Every long option that takes its value after an
# '=' was therefore invisible to a `case "$cur" in --target=*)` test — it never
# matched, and TAB fell through to plain file completion.
#
# __caustic_comp_init sets, for the caller:
#   cur   the text readline will replace — it does NOT carry the "--opt="
#         prefix when bash split the word, because readline only replaces what
#         follows the '='
#   prev  the previous word
#   copt  "--target" while completing that option's value, "" otherwise
#   cpfx  the prefix every reply must carry: "" in the split case, "--target="
#         when the word arrived whole (a shell whose COMP_WORDBREAKS has no '=')
__caustic_comp_init() {
    cur=${COMP_WORDS[COMP_CWORD]}
    prev=${COMP_WORDS[COMP_CWORD-1]}
    copt=""; cpfx=""
    if [[ $cur == "=" && $prev == -?* ]]; then
        copt=$prev; cur=""
    elif [[ $prev == "=" && $COMP_CWORD -ge 2 && ${COMP_WORDS[COMP_CWORD-2]} == -?* ]]; then
        copt=${COMP_WORDS[COMP_CWORD-2]}
    elif [[ $cur == -?*=* ]]; then
        copt=${cur%%=*}; cpfx="$copt="; cur=${cur#*=}
    fi
}

# Word-list reply, carrying whatever prefix the split demanded.
__caustic_comp_words() {
    COMPREPLY=( $(compgen -W "$1" -P "$cpfx" -- "$cur") )
    # A lone "--target=" is not a finished word: keep the cursor against it.
    if [ ${#COMPREPLY[@]} -eq 1 ] && [[ ${COMPREPLY[0]} == *= ]]; then
        compopt -o nospace 2>/dev/null
    fi
}

# File reply. $1 = a glob to keep ('*.cst'), empty = every file. Splitting on
# newlines alone is what lets a path with spaces in it survive.
__caustic_comp_files() {
    local IFS=$'\n'
    if [ -n "${1:-}" ]; then
        COMPREPLY=( $(compgen -f -X "!$1" -- "$cur") $(compgen -d -- "$cur") )
    else
        COMPREPLY=( $(compgen -f -- "$cur") )
    fi
    [ -n "$cpfx" ] && COMPREPLY=( "${COMPREPLY[@]/#/$cpfx}" )
    compopt -o filenames 2>/dev/null
}

__caustic_comp_dirs() {
    local IFS=$'\n'
    COMPREPLY=( $(compgen -d -- "$cur") )
    [ -n "$cpfx" ] && COMPREPLY=( "${COMPREPLY[@]/#/$cpfx}" )
    compopt -o filenames 2>/dev/null
}

# The directory holding the binary being completed, so a completion can look
# beside it for the stdlib and the shared libraries that ship with it.
__caustic_comp_selfdir() {
    local p=$1
    case "$p" in
        */*) ;;
        *) p=$(command -v "$p" 2>/dev/null) || return 1 ;;
    esac
    [ -n "$p" ] || return 1
    printf '%s\n' "${p%/*}"
}

# Every triple load_target (src/codegen/target.cst) accepts, canonical + aliases.
# caustic / caustic-x86_64 / caustic-aarch64 / caustic-x86_64-aarch64 are the
# CSE family: one container that runs on Linux, Windows and CausticOS.
__caustic_comp_triples="linux-x86_64 linux-aarch64 windows-x86_64 windows-aarch64
caustic caustic-x86_64 caustic-aarch64 caustic-x86_64-aarch64
wasm32-wasi wasm32 wasm64-wasi wasm64"
# ── end shared helpers ───────────────────────────────────────────────────────

# The --target= already on the line, whether bash split it at the '=' or not.
_caustic_line_target() {
    local i
    for (( i = 1; i < COMP_CWORD; i++ )); do
        case "${COMP_WORDS[i]}" in
            --target=?*) printf '%s\n' "${COMP_WORDS[i]#--target=}"; return ;;
            --target) [ "${COMP_WORDS[i+1]}" = "=" ] && printf '%s\n' "${COMP_WORDS[i+2]}"; return ;;
        esac
    done
}

_caustic_line_has() {
    local i
    for (( i = 1; i < COMP_CWORD; i++ )); do
        [ "${COMP_WORDS[i]}" = "$1" ] && return 0
    done
    return 1
}

# -l<name> resolves to lib<name>.so; offer the ones actually installed next to
# the compiler on top of the stdlib's own name.
_caustic_libs() {
    local d f n out="-lcaustic" self
    self=$(__caustic_comp_selfdir "${COMP_WORDS[0]}")
    for d in ${self:+"$self/../lib/caustic"} "$HOME/.local/lib/caustic" \
             /usr/local/lib/caustic /usr/lib/caustic; do
        [ -d "$d" ] || continue
        for f in "$d"/lib*.so; do
            [ -e "$f" ] || continue
            n=${f##*/lib}; n=${n%.so}
            [ "$n" = caustic ] || out="$out -l$n"
        done
    done
    printf '%s\n' "$out"
}

_caustic() {
    local cur prev copt cpfx
    __caustic_comp_init

    local flags="-c -o -j --parallel --max-ram --shared --emit-interface --emit-csti
--emit-tokens --emit-ast --emit-ir --profile --cache
--module-objects --module-only --emit-deps --csti-dir
-O0 -O1 -O2 --path -q --quiet --no-asm-cache
-debuglexer -debugparser -debugir --freestanding --allow-unsupported
--target= --mode= --entry= --base= --app-version= --stack-size= --extension="
    # Both are read straight off argv[1] and stop the compiler there, so they
    # are only ever the first word.
    [ "$COMP_CWORD" -eq 1 ] && flags="$flags --version --help"

    # A value that follows an '='.
    case "$copt" in
        --target)      __caustic_comp_words "$__caustic_comp_triples"; return ;;
        # CSE flavours: pure = CausticOS only, compat = both OS paths chosen at
        # run time, bundle = a polyglot that every loader accepts.
        --mode)        __caustic_comp_words "pure compat bundle"; return ;;
        --entry)       __caustic_comp_words "main"; return ;;
        --base)        __caustic_comp_words "0x400000 0x100000"; return ;;
        --stack-size)  __caustic_comp_words "65536 131072 262144 1048576 8388608"; return ;;
        --extension)   __caustic_comp_words "exe cse"; return ;;
        --app-version) return ;;                       # free-form
        -?*)           return ;;                       # some other --opt=, nothing to offer
    esac

    # A value that follows a space.
    case "$prev" in
        -o)
            # The name you almost always want is the input's basename carrying
            # the extension this target produces — hello.cst becomes hello on
            # Linux, hello.exe for windows-*, hello.wasm for wasm*. A CSE build
            # appends .cse / .cse.exe itself, so the bare name is right there.
            local w base="" t sfx=""
            for (( w = 1; w < COMP_CWORD; w++ )); do
                case "${COMP_WORDS[w]}" in
                    *.cst) base="${COMP_WORDS[w]##*/}"; base="${base%.cst}"; break ;;
                esac
            done
            t=$(_caustic_line_target)
            if _caustic_line_has --shared; then
                case "$t" in
                    windows-*) base="lib$base"; sfx=".dll" ;;
                    caustic*)  base="lib$base"; sfx=".csl" ;;
                    wasm*)     sfx=".wasm" ;;
                    *)         base="lib$base"; sfx=".so" ;;
                esac
            else
                case "$t" in
                    windows-*) sfx=".exe" ;;
                    wasm*)     sfx=".wasm" ;;
                esac
            fi
            __caustic_comp_files
            if [ -n "$base" ] && [[ $base$sfx == "$cur"* ]]; then
                COMPREPLY=( "$base$sfx" "${COMPREPLY[@]}" )
            fi
            return ;;
        --emit-deps)                                __caustic_comp_files; return ;;
        --cache|--csti-dir|--path|--module-objects) __caustic_comp_dirs;  return ;;
        -j|--parallel)                              __caustic_comp_words "1 2 4 8 16 32"; return ;;
        --max-ram)                                  __caustic_comp_words "0 256 512 1024 2048 4096 8192"; return ;;
    esac

    case "$cur" in
        # -lcaustic is the one library name the compiler itself acts on (it
        # externalizes the stdlib); any other is passed through to the linker.
        -l*) __caustic_comp_words "$(_caustic_libs)"; return ;;
        -*)  __caustic_comp_words "$flags"; return ;;
    esac

    __caustic_comp_files '*.cst'
}

# The toolchain ships under several names: a plain ELF, a Windows .exe, and
# the CSE flavours (.cse / .cse.exe) that carry every OS and architecture in
# one file. All of them are the same program and take the same arguments.
complete -F _caustic caustic
complete -F _caustic caustic-elf
complete -F _caustic caustic.exe
complete -F _caustic caustic.cse
complete -F _caustic caustic.cse.exe
complete -F _caustic caustic-universal.cse
complete -F _caustic caustic-universal.cse.exe
