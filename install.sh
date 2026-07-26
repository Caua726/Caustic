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
# FROM SOURCE
#   --from-source [--source-dir=DIR] [--ref=BRANCH]
#
# OTHER
#   --dry-run              print what would happen, touch nothing
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
REINSTALL=""
WITH_SRC=1
DRY=0
FROM_SRC=0
SOURCE_DIR=""
REF="main"
for arg in "$@"; do
    case "$arg" in
        --custom|--interactive) MODE="custom" ;;
        --system)     PREFIX="/usr/local" ;;
        --user)       PREFIX="$HOME/.local" ;;
        --prefix=*)   PREFIX="${arg#*=}" ;;
        --root=*)     ROOT_METHOD="${arg#*=}" ;;
        --format=*)   FORMATS="${arg#*=}"; SET_FORMAT=1 ;;
        --tools=*)    TOOLS="${arg#*=}"; SET_TOOLS=1 ;;
        --lib=*|--libs=*) LIBS="${arg#*=}"; SET_LIBS=1 ;;
        --reinstall|--force) REINSTALL=yes ;;
        --no-source)  WITH_SRC=0 ;;
        --source)     WITH_SRC=1 ;;
        --dry-run)    DRY=1 ;;
        --from-source) FROM_SRC=1 ;;
        --source-dir=*) FROM_SRC=1; SOURCE_DIR="${arg#*=}" ;;
        --ref=*)      REF="${arg#*=}" ;;
        # Kept so older one-liners keep working.
        --with-tools) TOOLS="as,ld" ;;
        --with-csl)   LIBS="$LIBS,csl" ;;
        --no-so)      LIBS=$(echo "$LIBS" | sed 's/\bso\b//; s/,,/,/g; s/^,//; s/,$//'); [ -n "$LIBS" ] || LIBS="none" ;;
        --cse|--universal) FORMATS="cse" ;;
        -h|--help)
            sed -n '2,/^set -eu/p' "$0" | sed 's/^# \{0,1\}//; $d'
            exit 0 ;;
        *) echo "warning: ignoring '$arg'" >&2 ;;
    esac
done

# --- interactive (reads /dev/tty so it works through a curl|sh pipe) ---
if [ "$MODE" = "custom" ] && [ -e /dev/tty ]; then
    ask() { printf "%s" "$1" >/dev/tty; read REPLY </dev/tty || REPLY=""; }
    echo "=== Caustic custom install ==="
    ask "Source — [1] latest release (fast)  [2] build from source (needs git; slower)  (default 1): "
    case "$REPLY" in 2) FROM_SRC=1 ;; *) FROM_SRC=0 ;; esac
    if [ "$FROM_SRC" = 1 ]; then
        ask "  branch or tag (default main): "
        [ -n "$REPLY" ] && REF="$REPLY"
    fi

    ask "Prefix — [1] \$HOME/.local  [2] /usr/local  [3] custom  (default 1): "
    case "$REPLY" in 2) PREFIX="/usr/local" ;; 3) ask "  path: "; PREFIX="$REPLY" ;; *) PREFIX="$HOME/.local" ;; esac

    if [ "$PREFIX" = "/usr/local" ] || [ "${PREFIX#$HOME}" = "$PREFIX" ]; then
        ask "Escalate with — [1] auto  [2] pkexec  [3] sudo  [4] doas  (default 1): "
        case "$REPLY" in 2) ROOT_METHOD="pkexec" ;; 3) ROOT_METHOD="sudo" ;; 4) ROOT_METHOD="doas" ;; *) ROOT_METHOD="auto" ;; esac
    fi

    ask "Compiler — [1] elf (native Linux)  [2] cse (universal: Linux+Windows+CausticOS, x86_64+ARM64)  [3] exe (Windows PE)  [4] elf + cse  (default 1): "
    case "$REPLY" in 2) FORMATS="cse" ;; 3) FORMATS="exe" ;; 4) FORMATS="elf,cse" ;; *) FORMATS="elf" ;; esac

    ask "Tools — [1] none  [2] as + ld  [3] everything (as, ld, mk, lsp)  (default 1): "
    case "$REPLY" in 2) TOOLS="as,ld" ;; 3) TOOLS="all" ;; *) TOOLS="none" ;; esac

    ask "Shared stdlib — [1] libcaustic.so  [2] + libcaustic.csl  [3] .so + .csl + .dll  [4] none  (default 1): "
    case "$REPLY" in 2) LIBS="so,csl" ;; 3) LIBS="so,csl,dll" ;; 4) LIBS="none" ;; *) LIBS="so" ;; esac

    ask "Install the stdlib sources (.cst)? — required to compile anything [Y/n]: "
    case "$REPLY" in [Nn]*) WITH_SRC=0 ;; *) WITH_SRC=1 ;; esac
fi

# Normalise the tool list so the copy loop below reads one shape.
[ "$TOOLS" = "all" ] && TOOLS="as,ld,mk,lsp"
[ "$TOOLS" = "none" ] && TOOLS=""
[ "$LIBS" = "none" ] && LIBS=""

# The first format named owns the plain `caustic` name.
PRIMARY=$(echo "$FORMATS" | cut -d, -f1)
for f in $(echo "$FORMATS" | tr ',' ' '); do
    case "$f" in elf|exe|cse) ;; *) echo "error: unknown --format '$f' (elf|exe|cse)"; exit 1 ;; esac
done
for l in $(echo "$LIBS" | tr ',' ' '); do
    case "$l" in so|csl|dll) ;; *) echo "error: unknown --lib '$l' (so|csl|dll|none)"; exit 1 ;; esac
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
    echo "caustic ${HAVE:-(unknown version)} is already installed at $PREFIX"
    echo "  compiler: $E_FORMAT   tools: $E_TOOLS   stdlib: $E_LIBS"

    # Anything this run did not name keeps what is already there.
    [ "$SET_FORMAT" = 0 ] && [ -n "$E_FORMAT" ] && FORMATS="$E_FORMAT"
    [ "$SET_TOOLS" = 0 ]  && [ -n "$E_TOOLS" ]  && TOOLS="$E_TOOLS"
    [ "$SET_LIBS" = 0 ]   && [ -n "$E_LIBS" ]   && LIBS="$E_LIBS"
    [ "$TOOLS" = "none" ] && TOOLS=""
    [ "$LIBS" = "none" ] && LIBS=""

    # /dev/tty can exist and still not be openable (cron, a container, a pipe
    # with no controlling terminal), so try it rather than test for it.
    if [ -z "$REINSTALL" ] && [ "$MODE" != "custom" ] && { : </dev/tty; } 2>/dev/null; then
        printf "Reinstall it, keeping these choices? [Y/n] " >/dev/tty
        read REPLY </dev/tty || REPLY=""
        case "$REPLY" in [Nn]*) echo "nothing done — use update.sh to move to the latest release"; exit 0 ;; esac
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
            NAME="$UNIVERSAL" ;;
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
