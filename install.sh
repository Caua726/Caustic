#!/bin/sh
# Caustic installer — any x86_64 Linux distro (binaries are static, no libc dep).
#
# Default (lean, per-user, no root, no questions):
#   curl -fsSL https://raw.githubusercontent.com/Caua726/Caustic/main/install.sh | sh
#   → caustic + the stdlib source + libcaustic.so into $HOME/.local
#
# Pick everything interactively:
#   curl -fsSL .../install.sh | sh -s -- --custom
#
# WHERE
#   --user                 $HOME/.local  (default)
#   --system               /usr/local    (escalates, see --root)
#   --prefix=DIR           anywhere
#   --root=METHOD          pkexec | sudo | doas | none | auto  (default auto)
#
# WHAT COMPILER            --format=LIST   (comma-separated, first one wins the
#                                           plain `caustic` name; default elf)
#   elf                    native x86_64 Linux ELF
#   cse                    universal caustic-universal.cse.exe — one file that
#                          also runs on Windows and CausticOS, x86_64 and ARM64
#   exe                    Windows PE (from the windows release archive)
#
# WHICH TOOLS              --tools=LIST    (as,ld,mk,lsp | all | none; default none)
#
# WHICH STDLIB             --lib=LIST      (comma-separated; default so)
#   so                     libcaustic.so   — dynamic stdlib for Linux
#   csl                    libcaustic.csl  — universal, loaded by std/csl_loader
#   dll                    libcaustic.dll  — Windows (from the windows archive)
#   none                   no shared stdlib
#   --no-source            skip the stdlib .cst sources (they are what you
#                          compile against; only skip if you know why)
#
# SHELL COMPLETIONS        --completions=LIST  (bash,zsh | auto | none)
#                          default auto = whichever of bash and zsh is installed
#
# FROM SOURCE
#   --from-source [--source-dir=DIR] [--ref=BRANCH]
#
# OTHER
#   --dry-run              print what would happen, touch nothing
#   -y, --yes              never ask anything, whatever the terminal looks like
#                          (implied by --dry-run and by naming any choice above)
set -eu

REPO="Caua726/Caustic"
TARBALL="caustic-x86_64-linux.tar.gz"
WINZIP="caustic-x86_64-windows.zip"
UNIVERSAL="caustic-universal.cse.exe"

ARCH=$(uname -m 2>/dev/null || echo unknown)
[ "$ARCH" = "x86_64" ] || { echo "error: unsupported architecture '$ARCH' (x86_64 only)"; exit 1; }
[ "$(uname -s 2>/dev/null)" = "Linux" ] || { echo "error: this installer targets Linux"; exit 1; }
command -v tar  >/dev/null 2>&1 || { echo "error: 'tar' is required"; exit 1; }
need_curl() { command -v curl >/dev/null 2>&1 || { echo "error: 'curl' is required"; exit 1; }; }
need_unzip() { command -v unzip >/dev/null 2>&1 || { echo "error: 'unzip' is required for --format=exe / --lib=dll"; exit 1; }; }

# `case ,$list, in *,item,*` is the POSIX way to ask "is item in this list".
has() { case ",$2," in *",$1,"*) return 0 ;; *) return 1 ;; esac; }

# --- options ---
MODE="default"
PREFIX=""
ROOT_METHOD="auto"
FORMATS="elf"; SET_FORMAT=0
TOOLS="none";  SET_TOOLS=0
LIBS="so";     SET_LIBS=0
COMPLETIONS="auto"; SET_COMP=0
REINSTALL=""
WITH_SRC=1
DRY=0
ASSUME_YES=0
# 1 once the caller has named any install decision. A run that says what it
# wants has nothing left to be asked about.
SPECIFIED=0
FROM_SRC=0
SOURCE_DIR=""
REF="main"
for arg in "$@"; do
    case "$arg" in
        --custom|--interactive) MODE="custom" ;;
        -y|--yes|--non-interactive) ASSUME_YES=1 ;;
        --system)     PREFIX="/usr/local"; SPECIFIED=1 ;;
        --user)       PREFIX="$HOME/.local"; SPECIFIED=1 ;;
        --prefix=*)   PREFIX="${arg#*=}"; SPECIFIED=1 ;;
        --root=*)     ROOT_METHOD="${arg#*=}"; SPECIFIED=1 ;;
        --format=*)   FORMATS="${arg#*=}"; SET_FORMAT=1; SPECIFIED=1 ;;
        --tools=*)    TOOLS="${arg#*=}"; SET_TOOLS=1; SPECIFIED=1 ;;
        --lib=*|--libs=*) LIBS="${arg#*=}"; SET_LIBS=1; SPECIFIED=1 ;;
        --completions=*) COMPLETIONS="${arg#*=}"; SET_COMP=1; SPECIFIED=1 ;;
        --no-completions) COMPLETIONS="none"; SET_COMP=1; SPECIFIED=1 ;;
        --reinstall|--force) REINSTALL=yes; SPECIFIED=1 ;;
        --no-source)  WITH_SRC=0; SPECIFIED=1 ;;
        --source)     WITH_SRC=1; SPECIFIED=1 ;;
        --dry-run)    DRY=1 ;;
        --from-source) FROM_SRC=1; SPECIFIED=1 ;;
        --source-dir=*) FROM_SRC=1; SOURCE_DIR="${arg#*=}"; SPECIFIED=1 ;;
        --ref=*)      REF="${arg#*=}"; SPECIFIED=1 ;;
        # Kept so older one-liners keep working.
        --with-tools) TOOLS="as,ld"; SPECIFIED=1 ;;
        --with-csl)   LIBS="$LIBS,csl"; SPECIFIED=1 ;;
        --no-so)      LIBS=$(echo "$LIBS" | sed 's/\bso\b//; s/,,/,/g; s/^,//; s/,$//'); [ -n "$LIBS" ] || LIBS="none"; SPECIFIED=1 ;;
        --cse|--universal) FORMATS="cse"; SPECIFIED=1 ;;
        -h|--help)
            sed -n '2,/^set -eu/p' "$0" | sed 's/^# \{0,1\}//; $d'
            exit 0 ;;
        *) echo "warning: ignoring '$arg'" >&2 ;;
    esac
done

# --- may we block on a question? ---
# ONE rule, in one place. Every prompt used to test /dev/tty for itself, which
# made "is this interactive" mean "does a terminal happen to be attached" —
# and that is not the same question. A fully specified run in a terminal
# (`install.sh --user --format=elf`) has already said what it wants, and a
# --dry-run promises to touch nothing, yet both sat waiting on a menu until
# somebody pressed a key. In a Makefile, a container with a controlling
# terminal, or a provisioning script, that is a hang with no output to explain
# it.
#
# So asking requires BOTH a terminal to ask on and a decision that is genuinely
# still open:
#
#   --custom     always ask (and fall back to defaults with no terminal)
#   -y/--yes     never ask
#   --dry-run    never ask — it changes nothing, so there is nothing to confirm
#   any choice named on the command line → never ask
#   otherwise    ask if there is a terminal
#
# /dev/tty can exist and still not be openable (cron, a container, a pipe with
# no controlling terminal), so it is opened rather than tested for.
may_ask() {
    [ "$ASSUME_YES" = 1 ] && return 1
    [ "$DRY" = 1 ] && return 1
    if [ "$MODE" = "custom" ]; then { : </dev/tty; } 2>/dev/null; return $?; fi
    [ "$SPECIFIED" = 1 ] && return 1
    { : </dev/tty; } 2>/dev/null
}

# --- arrow-key menu ---
# Reads raw bytes from /dev/tty so it works through `curl | sh`, where stdin is
# the script itself. Up/Down move, Enter picks; the answer lands in MENU_CHOICE
# as a 1-based index. If the terminal cannot be put in raw mode — no tty, a
# dumb terminal, a CI log — it falls back to typing the number, so the script
# never becomes unusable just because it cannot draw.
MENU_CHOICE=1
menu() {
    _title="$1"; shift
    _n=$#; _sel=1
    printf "%s\n" "$_title" >/dev/tty

    _saved=""
    if [ -z "${NO_TTY_MENU:-}" ]; then _saved=$(stty -g </dev/tty 2>/dev/null || echo ""); fi
    if [ -z "$_saved" ]; then
        _i=1; for _o in "$@"; do printf "  %d) %s\n" "$_i" "$_o" >/dev/tty; _i=$((_i+1)); done
        printf "  choice [1-%d]: " "$_n" >/dev/tty
        read _r </dev/tty || _r=""
        case "$_r" in ''|*[!0-9]*) _r=1 ;; esac
        [ "$_r" -ge 1 ] 2>/dev/null && [ "$_r" -le "$_n" ] 2>/dev/null || _r=1
        MENU_CHOICE=$_r
        return 0
    fi

    stty -echo -icanon min 1 time 0 </dev/tty 2>/dev/null
    _first=1
    while :; do
        [ "$_first" = 1 ] || printf "\033[%dA" "$_n" >/dev/tty
        _first=0
        _i=1
        for _o in "$@"; do
            if [ "$_i" -eq "$_sel" ]; then printf "\033[2K  \033[1;36m> %s\033[0m\n" "$_o" >/dev/tty
            else                            printf "\033[2K    %s\n" "$_o" >/dev/tty; fi
            _i=$((_i+1))
        done
        _b=$(dd bs=1 count=1 2>/dev/null </dev/tty | od -An -tu1 | tr -d ' ')
        case "$_b" in
            27) dd bs=1 count=1 2>/dev/null </dev/tty >/dev/null
                _b2=$(dd bs=1 count=1 2>/dev/null </dev/tty | od -An -tu1 | tr -d ' ')
                case "$_b2" in
                    65) _sel=$((_sel-1)); [ "$_sel" -lt 1 ] && _sel=$_n ;;
                    66) _sel=$((_sel+1)); [ "$_sel" -gt "$_n" ] && _sel=1 ;;
                esac ;;
            107) _sel=$((_sel-1)); [ "$_sel" -lt 1 ] && _sel=$_n ;;   # k
            106) _sel=$((_sel+1)); [ "$_sel" -gt "$_n" ] && _sel=1 ;; # j
            10|13|"") break ;;
        esac
    done
    stty "$_saved" </dev/tty 2>/dev/null
    MENU_CHOICE=$_sel
}

# --- interactive (reads /dev/tty so it works through a curl|sh pipe) ---
# A function rather than a block: the "change them" branch of the already-
# installed prompt further down needs to ask the same questions, and that
# prompt cannot run until the prefix is known.
ask() { printf "%s" "$1" >/dev/tty; read REPLY </dev/tty || REPLY=""; }
ASKED=0
interactive_setup() {
    ASKED=1
    echo "=== Caustic ==="

    menu "Install from" "the latest release (fast)" "source — clones and builds (needs git)"
    if [ "$MENU_CHOICE" = 2 ]; then
        FROM_SRC=1
        printf "  branch or tag (default main): " >/dev/tty
        read REPLY </dev/tty || REPLY=""
        [ -n "$REPLY" ] && REF="$REPLY"
    else FROM_SRC=0; fi

    menu "Where" "\$HOME/.local — just me, no root" "/usr/local — everyone, needs root" "somewhere else"
    case "$MENU_CHOICE" in
        2) PREFIX="/usr/local" ;;
        3) printf "  path: " >/dev/tty; read REPLY </dev/tty || REPLY=""; PREFIX="$REPLY" ;;
        *) PREFIX="$HOME/.local" ;;
    esac

    if [ "${PREFIX#$HOME}" = "$PREFIX" ]; then
        menu "Become root with" "whatever is available" "pkexec" "sudo" "doas"
        case "$MENU_CHOICE" in 2) ROOT_METHOD="pkexec" ;; 3) ROOT_METHOD="sudo" ;; 4) ROOT_METHOD="doas" ;; *) ROOT_METHOD="auto" ;; esac
    fi

    menu "Compiler" \
         "native Linux binary" \
         "universal — one file for Linux, Windows and CausticOS, x86_64 and ARM64" \
         "Windows .exe" \
         "native + universal"
    case "$MENU_CHOICE" in 2) FORMATS="cse" ;; 3) FORMATS="exe" ;; 4) FORMATS="elf,cse" ;; *) FORMATS="elf" ;; esac

    menu "Tools alongside the compiler" "none" "assembler + linker" "everything (as, ld, mk, lsp)"
    case "$MENU_CHOICE" in 2) TOOLS="as,ld" ;; 3) TOOLS="all" ;; *) TOOLS="none" ;; esac

    menu "Shared standard library" "libcaustic.so" "+ libcaustic.csl (universal)" "all three, with the Windows .dll" "none"
    case "$MENU_CHOICE" in 2) LIBS="so,csl" ;; 3) LIBS="so,csl,dll" ;; 4) LIBS="none" ;; *) LIBS="so" ;; esac

    menu "Standard library sources (.cst) — you compile against these" "install them" "skip them"
    case "$MENU_CHOICE" in 2) WITH_SRC=0 ;; *) WITH_SRC=1 ;; esac

    menu "Shell completions (TAB expands targets, triples, flags)" \
         "for whichever of bash and zsh is installed" "bash only" "zsh only" "none"
    case "$MENU_CHOICE" in 2) COMPLETIONS="bash" ;; 3) COMPLETIONS="zsh" ;; 4) COMPLETIONS="none" ;; *) COMPLETIONS="auto" ;; esac
}

if [ "$MODE" = "custom" ] && may_ask; then
    interactive_setup
fi

# Normalise the tool list so the copy loop below reads one shape.
[ "$TOOLS" = "all" ] && TOOLS="as,ld,mk,lsp"
[ "$TOOLS" = "none" ] && TOOLS=""
[ "$LIBS" = "none" ] && LIBS=""
# `auto` means the shells this machine actually has. Installing a zsh function
# on a box with no zsh is harmless but dishonest in the plan we print.
# Ends in `return 0` deliberately: a function whose last command is a failing
# test returns non-zero, and under `set -e` that ends the install.
normalise_completions() {
    if [ "$COMPLETIONS" = "auto" ]; then
        COMPLETIONS=""
        command -v bash >/dev/null 2>&1 && COMPLETIONS="bash"
        command -v zsh  >/dev/null 2>&1 && COMPLETIONS="${COMPLETIONS:+$COMPLETIONS,}zsh"
    fi
    [ "$COMPLETIONS" = "none" ] && COMPLETIONS=""
    return 0
}
normalise_completions

# The first format named owns the plain `caustic` name.
PRIMARY=$(echo "$FORMATS" | cut -d, -f1)
for f in $(echo "$FORMATS" | tr ',' ' '); do
    case "$f" in elf|exe|cse) ;; *) echo "error: unknown --format '$f' (elf|exe|cse)"; exit 1 ;; esac
done
for l in $(echo "$LIBS" | tr ',' ' '); do
    case "$l" in so|csl|dll) ;; *) echo "error: unknown --lib '$l' (so|csl|dll|none)"; exit 1 ;; esac
done
for s in $(echo "$COMPLETIONS" | tr ',' ' '); do
    case "$s" in bash|zsh) ;; *) echo "error: unknown --completions '$s' (bash|zsh|auto|none)"; exit 1 ;; esac
done

# --- prefix + privilege escalation ---
[ -z "$PREFIX" ] && PREFIX="$HOME/.local"
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    parent="$PREFIX"
    while [ ! -e "$parent" ] && [ "$parent" != "/" ]; do parent=$(dirname "$parent"); done
    if [ ! -w "$parent" ]; then
        case "$ROOT_METHOD" in
            none) echo "error: $PREFIX needs root and --root=none was given"; exit 1 ;;
            auto) for m in pkexec sudo doas; do
                      command -v "$m" >/dev/null 2>&1 && { SUDO="$m"; break; }
                  done
                  [ -n "$SUDO" ] || { echo "error: $PREFIX needs root and no pkexec/sudo/doas found (use --user)"; exit 1; } ;;
            *)    command -v "$ROOT_METHOD" >/dev/null 2>&1 \
                      || { echo "error: --root=$ROOT_METHOD not found on PATH"; exit 1; }
                  SUDO="$ROOT_METHOD" ;;
        esac
        echo "note: $PREFIX needs root — using $SUDO"
    fi
fi
BIN_DIR="$PREFIX/bin"; LIB_DIR="$PREFIX/lib/caustic"; STD_DIR="$LIB_DIR/std"

# --- already installed? ---
# Re-running the one-liner used to overwrite an existing install with the
# defaults, quietly discarding whichever compiler and libraries had been chosen.
# Now the recorded choices carry over unless this run names its own, and an
# interactive session is asked first.
EXISTING="$LIB_DIR/install-manifest"
if [ -f "$EXISTING" ]; then
    HAVE=""
    [ -x "$BIN_DIR/caustic" ] && HAVE=$("$BIN_DIR/caustic" --version 2>/dev/null | head -1 | awk '{print $2}')
    E_FORMAT=$(sed -n 's/^format=//p' "$EXISTING")
    E_TOOLS=$(sed -n 's/^tools=//p' "$EXISTING")
    E_LIBS=$(sed -n 's/^lib=//p' "$EXISTING")
    E_COMP=$(sed -n 's/^completions=//p' "$EXISTING")
    echo "caustic ${HAVE:-(unknown version)} is already installed at $PREFIX"
    echo "  compiler: $E_FORMAT   tools: $E_TOOLS   stdlib: $E_LIBS"

    # Anything this run did not name keeps what is already there.
    [ "$SET_FORMAT" = 0 ] && [ -n "$E_FORMAT" ] && FORMATS="$E_FORMAT"
    [ "$SET_TOOLS" = 0 ]  && [ -n "$E_TOOLS" ]  && TOOLS="$E_TOOLS"
    [ "$SET_LIBS" = 0 ]   && [ -n "$E_LIBS" ]   && LIBS="$E_LIBS"
    [ "$SET_COMP" = 0 ]   && [ -n "$E_COMP" ]   && { COMPLETIONS="$E_COMP"; normalise_completions; }
    [ "$TOOLS" = "none" ] && TOOLS=""
    [ "$LIBS" = "none" ] && LIBS=""

    if [ -z "$REINSTALL" ] && [ "$MODE" != "custom" ] && may_ask; then
        menu "Already installed — what now?" "reinstall with the same choices" "reinstall, choosing again" "cancel"
        case "$MENU_CHOICE" in
            2) MODE="custom" ;;
            3) echo "nothing done"; exit 0 ;;
        esac
    fi

    # "change them": ask the same questions the --custom flag asks. The prefix
    # is among them, so the directories derived from it are recomputed after.
    if [ "$MODE" = "custom" ] && [ "$ASKED" = 0 ]; then
        interactive_setup
        [ "$TOOLS" = "all" ] && TOOLS="as,ld,mk,lsp"
        [ "$TOOLS" = "none" ] && TOOLS=""
        [ "$LIBS" = "none" ] && LIBS=""
        normalise_completions
        PRIMARY=$(echo "$FORMATS" | cut -d, -f1)
        BIN_DIR="$PREFIX/bin"; LIB_DIR="$PREFIX/lib/caustic"; STD_DIR="$LIB_DIR/std"
    fi
fi

# pkexec drops the environment and runs from /, so a relative path or a $HOME
# reference would resolve differently than intended. Everything below is passed
# absolute, which is why this matters only here.
run() { if [ "$DRY" = 1 ]; then echo "  would: $*"; else $SUDO "$@"; fi; }

# Every installed path is recorded so uninstall removes exactly what was put
# there and nothing else — no globbing over a shared prefix like /usr/local.
MANIFEST_FILES=""
track() { MANIFEST_FILES="$MANIFEST_FILES$1
"; }

echo "install plan"
echo "  prefix:   $PREFIX${SUDO:+  (via $SUDO)}"
echo "  compiler: $FORMATS   (\`caustic\` → $PRIMARY)"
echo "  tools:    ${TOOLS:-none}"
echo "  stdlib:   ${LIBS:-no shared lib}$([ "$WITH_SRC" = 1 ] && echo ' + sources')"
echo "  complete: ${COMPLETIONS:-none}"
[ "$DRY" = 1 ] && echo "  (dry run — nothing will be written)"

# A dry run answers "what would you do", so it must not reach the network or
# unpack anything — it prints the plan above and stops.
if [ "$DRY" = 1 ]; then
    echo "dry run — no download, nothing written"
    exit 0
fi

# --- obtain the trees to install ---
TMPDIR=$(mktemp -d); trap 'rm -rf "$TMPDIR"' EXIT INT TERM
is_checkout() { [ -f "$1/Causticfile" ] && [ -f "$1/src/main.cst" ]; }

# The Windows archive is only fetched when something actually needs it.
WINSRC=""
fetch_windows() {
    [ -n "$WINSRC" ] && return 0
    need_curl; need_unzip
    echo "downloading $WINZIP ..."
    curl -fsSL "https://github.com/$REPO/releases/latest/download/$WINZIP" -o "$TMPDIR/$WINZIP" \
        || { echo "error: could not download $WINZIP"; exit 1; }
    unzip -q -o "$TMPDIR/$WINZIP" -d "$TMPDIR"
    WINSRC="$TMPDIR/caustic-x86_64-windows"
}

if [ "$FROM_SRC" = 1 ]; then
    if [ -n "$SOURCE_DIR" ]; then
        is_checkout "$SOURCE_DIR" || { echo "error: $SOURCE_DIR is not a Caustic checkout"; exit 1; }
    elif is_checkout "$PWD"; then
        SOURCE_DIR="$PWD"
    else
        command -v git >/dev/null 2>&1 || { echo "error: 'git' is required to clone (or use --source-dir=DIR)"; exit 1; }
        echo "cloning $REPO ($REF) ..."
        git clone --depth 1 --branch "$REF" --recurse-submodules \
            "https://github.com/$REPO.git" "$TMPDIR/src" >/dev/null 2>&1 \
            || { echo "error: clone failed"; exit 1; }
        SOURCE_DIR="$TMPDIR/src"
    fi
    echo "building from $SOURCE_DIR ..."

    # Caustic compiles itself, so building it needs a compiler to start from.
    # Prefer one already in the checkout, then one on PATH, and fall back to the
    # released binaries — which is what the bootstrap is for.
    SEED_BIN=""
    if [ -x "$SOURCE_DIR/caustic" ] && [ -x "$SOURCE_DIR/caustic-mk" ]; then
        SEED_BIN="$SOURCE_DIR"
    elif command -v caustic >/dev/null 2>&1 && command -v caustic-mk >/dev/null 2>&1; then
        SEED_BIN=$(dirname "$(command -v caustic-mk)")
    else
        echo "  no seed compiler — fetching the release to bootstrap with ..."
        need_curl
        curl -fsSL "https://github.com/$REPO/releases/latest/download/$TARBALL" -o "$TMPDIR/$TARBALL" \
            || { echo "error: download failed"; exit 1; }
        tar xzf "$TMPDIR/$TARBALL" -C "$TMPDIR"
        SEED_BIN="$TMPDIR/caustic-x86_64-linux/bin"
    fi
    PATH="$SEED_BIN:$PATH"; export PATH

    # Output goes to a log rather than the terminal — packaging libcaustic warns
    # about the Windows symbols a Linux .so cannot resolve, every time, and that
    # is expected noise. The log is printed if a step actually fails.
    LOG="$TMPDIR/build.log"
    ( cd "$SOURCE_DIR" \
      && for t in caustic caustic-as caustic-ld caustic-mk; do
             echo "  building $t"
             caustic-mk build "$t" >>"$LOG" 2>&1 || exit 1
         done \
      && caustic-mk build caustic-lsp >>"$LOG" 2>&1 || true ) \
      || { echo "error: build failed"; tail -20 "$LOG"; exit 1; }
    echo "  packaging"
    ( cd "$SOURCE_DIR" && caustic-mk run dist >>"$LOG" 2>&1 ) \
      || { echo "error: packaging failed"; tail -20 "$LOG"; exit 1; }
    if has exe "$FORMATS" || has dll "$LIBS"; then
        echo "  packaging windows"
        ( cd "$SOURCE_DIR" && caustic-mk run dist-windows >>"$LOG" 2>&1 ) \
          || { echo "error: windows packaging failed"; tail -20 "$LOG"; exit 1; }
        rm -rf "$TMPDIR/winpkg"; mkdir -p "$TMPDIR/winpkg"
        unzip -q -o "$SOURCE_DIR/$WINZIP" -d "$TMPDIR/winpkg"
        WINSRC="$TMPDIR/winpkg/caustic-x86_64-windows"
    fi

    [ -f "$SOURCE_DIR/$TARBALL" ] || { echo "error: build produced no $TARBALL"; exit 1; }
    rm -rf "$TMPDIR/pkg"; mkdir -p "$TMPDIR/pkg"
    tar xzf "$SOURCE_DIR/$TARBALL" -C "$TMPDIR/pkg"
    SRC="$TMPDIR/pkg/caustic-x86_64-linux"

    # The universal image is not in the linux tarball — `dist` does not build it,
    # so without this the install step below falls through to downloading the
    # released one, and --from-source would hand back a binary that is not from
    # this source. --mode=bundle links one body per OS/architecture and welds
    # them, so it is the slowest thing here; only build it when asked for.
    if has cse "$FORMATS"; then
        echo "  building $UNIVERSAL"
        ( cd "$SOURCE_DIR" \
          && caustic -O2 --target=caustic --mode=bundle src/main.cst -o caustic-universal >>"$LOG" 2>&1 ) \
          || { echo "error: universal build failed"; tail -20 "$LOG"; exit 1; }
        [ -f "$SOURCE_DIR/$UNIVERSAL" ] \
          || { echo "error: the universal build produced no $UNIVERSAL"; exit 1; }
        cp "$SOURCE_DIR/$UNIVERSAL" "$SRC/bin/$UNIVERSAL"
    fi
else
    need_curl
    echo "downloading latest release ..."
    curl -fsSL "https://github.com/$REPO/releases/latest/download/$TARBALL" -o "$TMPDIR/$TARBALL" \
        || { echo "error: download failed"; exit 1; }
    tar xzf "$TMPDIR/$TARBALL" -C "$TMPDIR"
    SRC="$TMPDIR/caustic-x86_64-linux"
fi

echo "installing to $PREFIX ..."
run mkdir -p "$BIN_DIR" "$STD_DIR"

# --- compiler(s) ---
# Each format is installed under its own name; the primary one additionally
# answers to plain `caustic`, so the command you type does not change with the
# flavour you picked.
install_format() {
    case "$1" in
        elf)
            [ -f "$SRC/bin/caustic" ] || { echo "error: the release carries no native caustic"; exit 1; }
            run cp "$SRC/bin/caustic" "$BIN_DIR/caustic-elf"; track "$BIN_DIR/caustic-elf"
            run chmod +x "$BIN_DIR/caustic-elf"
            NAME="caustic-elf" ;;
        exe)
            fetch_windows
            [ -f "$WINSRC/bin/caustic.exe" ] || { echo "error: the windows archive carries no caustic.exe"; exit 1; }
            run cp "$WINSRC/bin/caustic.exe" "$BIN_DIR/caustic.exe"; track "$BIN_DIR/caustic.exe"
            NAME="caustic.exe" ;;
        cse)
            # One file carrying a native body per OS and architecture. On Linux a
            # shell stub picks the right ELF, which is why it needs the exec bit;
            # the .exe in the name is what lets the same file run on Windows,
            # whose loader reads the MZ at offset 0.
            if [ -f "$SRC/bin/$UNIVERSAL" ]; then
                run cp "$SRC/bin/$UNIVERSAL" "$BIN_DIR/$UNIVERSAL"; track "$BIN_DIR/$UNIVERSAL"
            else
                need_curl
                echo "downloading $UNIVERSAL ..."
                curl -fsSL "https://github.com/$REPO/releases/latest/download/$UNIVERSAL" \
                     -o "$TMPDIR/$UNIVERSAL" \
                    || { echo "error: could not download $UNIVERSAL"; exit 1; }
                run cp "$TMPDIR/$UNIVERSAL" "$BIN_DIR/$UNIVERSAL"; track "$BIN_DIR/$UNIVERSAL"
            fi
            run chmod +x "$BIN_DIR/$UNIVERSAL"
            # The image must start with MZ for the Windows loader, and on Linux
            # that same magic is what wine and mono claim in binfmt_misc. Where
            # either is installed the kernel hands the file to wine BEFORE the
            # shell ever sees the stub, so running the image by name silently
            # executes its Windows body instead of the ELF one — it compiles,
            # and quietly produces a broken binary. Going through sh explicitly
            # is what keeps the right body running, so the invocable name is a
            # launcher and the image itself is only ever data. $0 is absolute
            # here: the kernel got a resolved path to run the #! line with.
            printf '#!/bin/sh\nexec sh "$(dirname -- "$0")/%s" "$@"\n' "$UNIVERSAL" \
                > "$TMPDIR/caustic-cse"
            chmod +x "$TMPDIR/caustic-cse"
            run cp "$TMPDIR/caustic-cse" "$BIN_DIR/caustic-cse"; track "$BIN_DIR/caustic-cse"
            run chmod +x "$BIN_DIR/caustic-cse"
            NAME="caustic-cse" ;;
    esac
}
for f in $(echo "$FORMATS" | tr ',' ' '); do
    install_format "$f"
    [ "$f" = "$PRIMARY" ] && PRIMARY_NAME="$NAME"
done
# A Windows PE cannot be the thing `caustic` runs on Linux, so it keeps its own
# name and nothing is linked to it.
if [ "$PRIMARY" != "exe" ]; then
    run ln -sf "$PRIMARY_NAME" "$BIN_DIR/caustic"; track "$BIN_DIR/caustic"
fi

# --- tools ---
for t in $(echo "$TOOLS" | tr ',' ' '); do
    [ -z "$t" ] && continue
    if [ -f "$SRC/bin/caustic-$t" ]; then
        run cp "$SRC/bin/caustic-$t" "$BIN_DIR/caustic-$t"; track "$BIN_DIR/caustic-$t"
        run chmod +x "$BIN_DIR/caustic-$t"
    else
        echo "  note: caustic-$t is not in this release, skipped"
    fi
done

# --- stdlib ---
[ "$WITH_SRC" = 1 ] && run cp -R "$SRC"/lib/caustic/std/. "$STD_DIR/"
for l in $(echo "$LIBS" | tr ',' ' '); do
    [ -z "$l" ] && continue
    case "$l" in
        so|csl)
            if [ -f "$SRC/lib/caustic/libcaustic.$l" ]; then
                run cp "$SRC/lib/caustic/libcaustic.$l" "$LIB_DIR/"; track "$LIB_DIR/libcaustic.$l"
            else
                echo "  note: libcaustic.$l is not in this release, skipped"
            fi ;;
        dll)
            fetch_windows
            if [ -f "$WINSRC/lib/caustic/libcaustic.dll" ]; then
                run cp "$WINSRC/lib/caustic/libcaustic.dll" "$LIB_DIR/"; track "$LIB_DIR/libcaustic.dll"
            else
                echo "  note: libcaustic.dll is not in the windows archive, skipped"
            fi ;;
    esac
done

# --- shell completions ---
# One file per tool, in the directory each shell already searches:
#   bash  <prefix>/share/bash-completion/completions/<command>
#   zsh   <prefix>/share/zsh/site-functions/_<command>
# With --user that is $XDG_DATA_HOME/bash-completion/completions, which
# bash-completion loads on demand with no configuration at all.
BASH_COMP_DIR="$PREFIX/share/bash-completion/completions"
ZSH_COMP_DIR="$PREFIX/share/zsh/site-functions"
COMPSRC="$SRC/share/caustic/completions"
INSTALLED_COMP=""
if [ -n "$COMPLETIONS" ]; then
    if [ ! -d "$COMPSRC" ]; then
        echo "  note: this release carries no completion scripts, skipped"
        COMPLETIONS=""
    else
        # Only for the commands that were actually installed.
        COMP_NAMES="caustic"
        for t in $(echo "$TOOLS" | tr ',' ' '); do
            [ -z "$t" ] && continue
            [ -f "$BIN_DIR/caustic-$t" ] && COMP_NAMES="$COMP_NAMES caustic-$t"
        done
        # The compiler also answers to the name of the flavour that was
        # installed. bash-completion looks its file up by the exact command
        # name, so those need a link; zsh reads every name off the #compdef
        # line in the file itself and needs none.
        COMP_ALIASES=""
        for f in $(echo "$FORMATS" | tr ',' ' '); do
            case "$f" in
                elf) COMP_ALIASES="$COMP_ALIASES caustic-elf" ;;
                exe) COMP_ALIASES="$COMP_ALIASES caustic.exe" ;;
                cse) COMP_ALIASES="$COMP_ALIASES $UNIVERSAL" ;;
            esac
        done
    fi
fi
if [ -n "$COMPLETIONS" ] && has bash "$COMPLETIONS"; then
    run mkdir -p "$BASH_COMP_DIR"
    for n in $COMP_NAMES; do
        [ -f "$COMPSRC/$n.bash" ] || continue
        run cp "$COMPSRC/$n.bash" "$BASH_COMP_DIR/$n"; track "$BASH_COMP_DIR/$n"
    done
    for a in $COMP_ALIASES; do
        [ "$a" = "caustic" ] && continue
        run ln -sf caustic "$BASH_COMP_DIR/$a"; track "$BASH_COMP_DIR/$a"
    done
    INSTALLED_COMP="bash"
fi
if [ -n "$COMPLETIONS" ] && has zsh "$COMPLETIONS"; then
    run mkdir -p "$ZSH_COMP_DIR"
    for n in $COMP_NAMES; do
        [ -f "$COMPSRC/_$n" ] || continue
        run cp "$COMPSRC/_$n" "$ZSH_COMP_DIR/_$n"; track "$ZSH_COMP_DIR/_$n"
    done
    INSTALLED_COMP="${INSTALLED_COMP:+$INSTALLED_COMP,}zsh"
fi

# --- manifest ---
# update.sh replays these choices; uninstall.sh removes these paths. Written
# last so a failed install leaves no manifest claiming success.
MANIFEST="$LIB_DIR/install-manifest"
{
    echo "# caustic install manifest — written by install.sh, read by update.sh and uninstall.sh"
    echo "prefix=$PREFIX"
    echo "format=$FORMATS"
    echo "tools=${TOOLS:-none}"
    echo "lib=${LIBS:-none}"
    echo "completions=${COMPLETIONS:-none}"
    echo "source=$WITH_SRC"
    echo "root=$ROOT_METHOD"
    [ "$WITH_SRC" = 1 ] && echo "stddir=$STD_DIR"
    echo "files:"
    printf "%s" "$MANIFEST_FILES"
} > "$TMPDIR/manifest"
run cp "$TMPDIR/manifest" "$MANIFEST"
if [ "$PRIMARY" = "exe" ]; then
    echo "caustic installed → $BIN_DIR/caustic.exe  (Windows PE; run it under wine or on Windows)"
else
    echo "caustic installed → $BIN_DIR/caustic → $PRIMARY_NAME"
fi
command -v caustic >/dev/null 2>&1 || echo "  (add to PATH:  export PATH=\"$BIN_DIR:\$PATH\")"

# A completion file in a directory the shell never reads is the same as no
# completion at all, and the failure is silent — so say what is missing rather
# than leaving TAB quietly doing nothing.
if [ -n "$INSTALLED_COMP" ]; then
    echo "completions installed → $INSTALLED_COMP"
    if has bash "$INSTALLED_COMP"; then
        if [ ! -r /usr/share/bash-completion/bash_completion ] \
           && [ ! -r /etc/bash_completion ] && [ ! -r /usr/local/share/bash-completion/bash_completion ]; then
            echo "  bash: the bash-completion package is not installed, so nothing loads them."
            echo "        Install it, or add to ~/.bashrc:"
            echo "          for f in \"$BASH_COMP_DIR\"/*; do [ -r \"\$f\" ] && . \"\$f\"; done"
        else
            case ":${XDG_DATA_DIRS:-/usr/local/share:/usr/share}:$HOME/.local/share:" in
                *":$PREFIX/share:"*) ;;
                *) echo "  bash: $PREFIX/share is outside XDG_DATA_DIRS, so bash-completion will not"
                   echo "        find them. Add to ~/.bashrc:"
                   echo "          export XDG_DATA_DIRS=\"$PREFIX/share:\${XDG_DATA_DIRS:-/usr/local/share:/usr/share}\"" ;;
            esac
        fi
    fi
    if has zsh "$INSTALLED_COMP"; then
        # Ask zsh itself rather than guessing: distributions disagree about
        # which site-functions directories are compiled into the default fpath.
        if ! zsh -fc 'print -l $fpath' 2>/dev/null | grep -qx "$ZSH_COMP_DIR"; then
            echo "  zsh: $ZSH_COMP_DIR is not on your \$fpath. Add to ~/.zshrc, BEFORE compinit:"
            echo "         fpath=($ZSH_COMP_DIR \$fpath)"
            echo "       then:  rm -f ~/.zcompdump*; compinit"
        fi
    fi
fi
