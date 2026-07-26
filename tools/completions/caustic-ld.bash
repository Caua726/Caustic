# caustic-ld — bash completion.  Flags mirror caustic-linker/main.cst:main.
#
# install:  <prefix>/share/bash-completion/completions/caustic-ld  (loaded on demand)
#     or:   . tools/completions/caustic-ld.bash                    (from ~/.bashrc)

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

# -l<name> resolves to lib<name>.so in the loader's library directories; offer
# what is actually installed there on top of the two everyone reaches for.
_caustic_ld_libs() {
    local d f n out="-lcaustic -lc -lm" self
    self=$(__caustic_comp_selfdir "${COMP_WORDS[0]}")
    for d in ${self:+"$self/../lib/caustic"} "$HOME/.local/lib/caustic" \
             /usr/local/lib/caustic /usr/lib/caustic; do
        [ -d "$d" ] || continue
        for f in "$d"/lib*.so; do
            [ -e "$f" ] || continue
            n=${f##*/lib}; n=${n%.so}
            case " $out " in *" -l$n "*) ;; *) out="$out -l$n" ;; esac
        done
    done
    printf '%s\n' "$out"
}

_caustic_ld() {
    local cur prev copt cpfx
    __caustic_comp_init

    local flags="-o -v -h --help --entry= --base= --target= --mode= --extension=
--strip --map --keep-empty --freestanding --shared
--cse-combine --cse-fat --image= --pe= --elf= --elf-arm64= --pe-arm64= --cst="

    # A value that follows an '='.
    case "$copt" in
        --target)    __caustic_comp_words "$__caustic_comp_triples"; return ;;
        --mode)      __caustic_comp_words "pure compat bundle"; return ;;
        --entry)     __caustic_comp_words "main"; return ;;
        --base)      __caustic_comp_words "0x400000 0x100000"; return ;;
        --extension) __caustic_comp_words "exe cse"; return ;;
        # Finished executables and images fed to --cse-combine / --cse-fat. A
        # pure CSE image is named "<n>.cse" and a polyglot one "<n>.cse.exe", so
        # the glob has to reach past the extension — '*.cse' alone would hide
        # every combined image. An ELF carries no extension, hence no filter.
        --image|--cst)     __caustic_comp_files '*.cse*'; return ;;
        --pe|--pe-arm64)   __caustic_comp_files '*.exe';  return ;;
        --elf|--elf-arm64) __caustic_comp_files;          return ;;
        -?*)         return ;;
    esac

    case "$prev" in
        -o) __caustic_comp_files; return ;;
    esac

    case "$cur" in
        -l*) __caustic_comp_words "$(_caustic_ld_libs)"; return ;;
        -*)  __caustic_comp_words "$flags"; return ;;
    esac

    # Objects — .o from the ELF path, .obj from the PE one.
    local IFS=$'\n'
    COMPREPLY=( $(compgen -f -X '!*.o' -- "$cur") $(compgen -f -X '!*.obj' -- "$cur")
                $(compgen -d -- "$cur") )
    compopt -o filenames 2>/dev/null
}

# The toolchain ships under several names: a plain ELF, a Windows .exe, and
# the CSE flavours (.cse / .cse.exe) that carry every OS and architecture in
# one file. All of them are the same program and take the same arguments.
complete -F _caustic_ld caustic-ld
complete -F _caustic_ld caustic-ld.exe
complete -F _caustic_ld caustic-ld.cse
complete -F _caustic_ld caustic-ld.cse.exe
complete -F _caustic_ld caustic-ld-universal.cse
complete -F _caustic_ld caustic-ld-universal.cse.exe
