# caustic-mk — bash completion.  Commands/flags mirror caustic-maker/main.cst.
#
# Target, script and profile names come LIVE from `caustic-mk list` — and, when
# no usable binary is around, straight out of the nearest Causticfile — so
# completion always describes the project you are standing in, never a snapshot
# baked in when the script was written.
#
# install:  <prefix>/share/bash-completion/completions/caustic-mk  (loaded on demand)
#     or:   . tools/completions/caustic-mk.bash                    (from ~/.bashrc)

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

# The nearest Causticfile, searched upward the way caustic-mk looks for it.
_caustic_mk_file() {
    local d="$PWD" i
    for (( i = 0; i < 32; i++ )); do
        [ -f "$d/Causticfile" ] && { printf '%s\n' "$d/Causticfile"; return 0; }
        [ "$d" = "/" ] && return 1
        d="${d%/*}"; [ -z "$d" ] && d=/
    done
    return 1
}

# $1 = targets | scripts | profiles, read out of the manifest itself. This is
# the fallback for a checkout with no built binary yet, and the answer whenever
# `caustic-mk list` cannot run.
_caustic_mk_from_file() {
    local f kw=$1
    f=$(_caustic_mk_file) || return 1
    case "$kw" in
        targets)  kw=target  ;;
        scripts)  kw=script  ;;
        profiles) kw=profile ;;
    esac
    sed -n "s/^[[:space:]]*$kw[[:space:]]\{1,\}\"\([^\"]*\)\".*/\1/p" "$f"
}

# $1 = targets | scripts | profiles. Asks the very binary being completed — so
# `./caustic-mk build <TAB>` inside a checkout describes that checkout, not
# whichever caustic-mk happens to be first on PATH.
_caustic_mk_names() {
    local bin=${_caustic_mk_bin:-caustic-mk} out=""
    if command -v "$bin" >/dev/null 2>&1; then
        out=$("$bin" list 2>/dev/null | awk -v what="$1" '
            /^targets \(/           { intargets = 1; next }
            intargets && /^  +[^ ]/ { if (what == "targets") print $1; next }
            /^[^ ]/                 { intargets = 0 }
            what == "scripts"  && /^scripts:/  { for (i = 2; i <= NF; i++) print $i }
            what == "profiles" && /^profiles:/ { for (i = 2; i <= NF; i++) print $i }
        ')
    fi
    [ -n "$out" ] || out=$(_caustic_mk_from_file "$1")
    printf '%s\n' "$out"
}

# `set NAME "..."` keys from the manifest, each with the '=' already on it —
# --define wants NAME=VALUE.
_caustic_mk_vars() {
    local f
    f=$(_caustic_mk_file) || return 1
    sed -n 's/^[[:space:]]*set[[:space:]]\{1,\}\([A-Za-z_][A-Za-z0-9_]*\).*/\1=/p' "$f"
}

# What that key is set to today, offered as the starting point for an override.
_caustic_mk_var_value() {
    local f
    f=$(_caustic_mk_file) || return 1
    sed -n "s/^[[:space:]]*set[[:space:]]\{1,\}$1[[:space:]]\{1,\}\"\([^\"]*\)\".*/\1/p" "$f"
}

# "-D NAME=<value>" and "--define=NAME=<value>" both arrive shredded by the '='
# in COMP_WORDBREAKS. Work out which NAME's value is being typed, if any.
_caustic_mk_define_name() {
    local i n
    if [ "$cur" = "=" ]; then
        n=$prev; i=$((COMP_CWORD-2))
    elif [ "$prev" = "=" ]; then
        n=${COMP_WORDS[COMP_CWORD-2]}; i=$((COMP_CWORD-3))
    else
        return 1
    fi
    [ "$i" -ge 0 ] || return 1
    [ "${COMP_WORDS[i]}" = "=" ] && i=$((i-1))
    [ "$i" -ge 0 ] || return 1
    case "${COMP_WORDS[i]}" in
        -D|--define) printf '%s\n' "$n"; return 0 ;;
    esac
    return 1
}

# How many positionals precede the cursor. Flags that swallow the next word
# have to be skipped, or `caustic-mk run --profile release <TAB>` reads
# "release" as the name of the script to run. Answer lands in $REPLY.
_caustic_mk_positionals() {
    local i w skip=0
    REPLY=0
    for (( i = 2; i < COMP_CWORD; i++ )); do
        w=${COMP_WORDS[i]}
        if [ "$skip" = 1 ]; then skip=0; continue; fi
        case "$w" in
            -j|--parallel|--profile|--prefix|--interval|--define|-D) skip=1 ;;
            =) ;;                       # the '=' of an --opt=value bash split
            -*) ;;
            *)  # the value half of "--opt = value" is not a positional either
                [ "${COMP_WORDS[i-1]}" = "=" ] || REPLY=$((REPLY+1)) ;;
        esac
    done
}

_caustic_mk() {
    local cur prev copt cpfx cmd i dn REPLY
    local _caustic_mk_bin=$1
    __caustic_comp_init

    local commands="build run test list info why graph watch install clean init doctor completions help version"
    # main.cst reads argv[1] for these; as a later word _parse_opts rejects them.
    local global_flags="-h --help -V --version"
    local flags="-j --parallel --incremental --continue --force --no-deps
-n --dry-run -q --quiet -v --verbose --time
--profile --prefix --define -D --target="

    if [ "$COMP_CWORD" -eq 1 ]; then
        case "$cur" in
            -*) __caustic_comp_words "$global_flags" ;;
            *)  __caustic_comp_words "$commands" ;;
        esac
        return
    fi

    cmd=${COMP_WORDS[1]}
    # Flags that belong to one command each.
    case "$cmd" in
        clean) flags="$flags --cache" ;;
        graph) flags="$flags --dot" ;;
        watch) flags="$flags --interval" ;;
    esac

    # An explicit `--` hands the rest to the child process.
    for (( i = 2; i < COMP_CWORD; i++ )); do
        if [ "${COMP_WORDS[i]}" = "--" ]; then __caustic_comp_files; return; fi
    done

    # ...and for run/test so does each word after the name: those arguments are
    # the script's $1..$n, or the target binary's own argv, and no flag of ours
    # is parsed past that point.
    _caustic_mk_positionals
    if [ "$REPLY" -ge 1 ] && { [ "$cmd" = run ] || [ "$cmd" = test ]; }; then
        __caustic_comp_files
        return
    fi

    if dn=$(_caustic_mk_define_name); then
        # On "NAME=" the word under the cursor is the '=' itself; readline will
        # replace what comes after it, which is nothing yet.
        [ "$cur" = "=" ] && cur=""
        __caustic_comp_words "$(_caustic_mk_var_value "$dn")"
        return
    fi

    case "$copt" in
        --target)
            __caustic_comp_words "$__caustic_comp_triples"; return ;;
        --define)
            __caustic_comp_words "$(_caustic_mk_vars)"
            compopt -o nospace 2>/dev/null
            return ;;
        -?*) return ;;
    esac

    case "$prev" in
        --profile) __caustic_comp_words "$(_caustic_mk_names profiles)"; return ;;
        --prefix)  __caustic_comp_dirs; return ;;
        -j|--parallel) __caustic_comp_words "1 2 4 8 16 32"; return ;;
        --interval)    __caustic_comp_words "50 100 200 400 1000 2000"; return ;;
        --define|-D)
            # NAME= with no trailing space: the value still has to be typed.
            __caustic_comp_words "$(_caustic_mk_vars)"
            compopt -o nospace 2>/dev/null
            return ;;
    esac

    case "$cur" in
        -*) __caustic_comp_words "$flags"; return ;;
    esac

    # Every one of these takes a single name, so once it is typed there is
    # nothing left to offer (a second positional is an error).
    case "$cmd" in
        build|run|info|why|graph|watch|install|completions)
            [ "$REPLY" -eq 0 ] || { COMPREPLY=(); return; } ;;
    esac
    case "$cmd" in
        build)
            # `all` builds every target; a target named all would win over it.
            __caustic_comp_words "all $(_caustic_mk_names targets)" ;;
        run)
            __caustic_comp_words "$(_caustic_mk_names targets) $(_caustic_mk_names scripts)" ;;
        test)
            __caustic_comp_files ;;
        info|why|graph|watch|install)
            __caustic_comp_words "$(_caustic_mk_names targets)" ;;
        completions)
            __caustic_comp_words "bash zsh" ;;
        *)
            # list, clean, init, doctor, help and version take no positional.
            COMPREPLY=() ;;
    esac
}

# The toolchain ships under several names: a plain ELF, a Windows .exe, and
# the CSE flavours (.cse / .cse.exe) that carry every OS and architecture in
# one file. All of them are the same program and take the same arguments.
complete -F _caustic_mk caustic-mk
complete -F _caustic_mk caustic-mk.exe
complete -F _caustic_mk caustic-mk.cse
complete -F _caustic_mk caustic-mk.cse.exe
complete -F _caustic_mk caustic-mk-universal.cse
complete -F _caustic_mk caustic-mk-universal.cse.exe
