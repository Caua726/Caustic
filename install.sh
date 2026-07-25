#!/bin/sh
# Caustic installer — any x86_64 Linux distro (binaries are static, no libc dep).
#
# Default (lean, per-user, no sudo, no questions):
#   curl -fsSL https://raw.githubusercontent.com/Caua726/Caustic/main/install.sh | sh
#   → installs `caustic` + the stdlib (source + libcaustic.so) into $HOME/.local
#
# Custom (interactive — pick prefix, tools and stdlib pieces):
#   curl -fsSL .../install.sh | sh -s -- --custom
#
# Build and install the current source instead of the last release:
#   ./install.sh --from-source                 (from inside a checkout)
#   ./install.sh --from-source --source-dir=DIR
#   curl -fsSL .../install.sh | sh -s -- --from-source   (clones first)
#
# Non-interactive flags: --system | --user | --prefix=DIR
#                        --with-tools  (caustic-as, caustic-ld, caustic-mk, caustic-lsp)
#                        --with-csl    (universal libcaustic.csl)  --no-so  --no-source
#                        --cse         install the universal .cse.exe compiler instead
#                                      of the native one (one file: Linux + Windows +
#                                      CausticOS, x86_64 + ARM64)
#                        --from-source [--source-dir=DIR] [--ref=BRANCH]
set -eu

REPO="Caua726/Caustic"
TARBALL="caustic-x86_64-linux.tar.gz"
UNIVERSAL="caustic-universal.cse.exe"

ARCH=$(uname -m 2>/dev/null || echo unknown)
[ "$ARCH" = "x86_64" ] || { echo "error: unsupported architecture '$ARCH' (x86_64 only)"; exit 1; }
[ "$(uname -s 2>/dev/null)" = "Linux" ] || { echo "error: this installer targets Linux"; exit 1; }
command -v tar  >/dev/null 2>&1 || { echo "error: 'tar' is required"; exit 1; }
# curl is only needed when something is downloaded, which --from-source inside a
# checkout with a seed compiler never does.
need_curl() { command -v curl >/dev/null 2>&1 || { echo "error: 'curl' is required"; exit 1; }; }

# --- options ---
MODE="default"
PREFIX=""
WITH_TOOLS=0      # caustic-as / caustic-ld / caustic-mk / caustic-lsp
WITH_CSL=0        # universal libcaustic.csl
WITH_CSE=0        # install the universal .cse.exe compiler instead of the native binary
WITH_SO=1         # libcaustic.so
WITH_SRC=1        # stdlib source (.cst) — required to compile against the stdlib
FROM_SRC=0        # build the checkout instead of downloading the release
SOURCE_DIR=""     # where that checkout is (default: cwd, else clone)
REF="main"
for arg in "$@"; do
    case "$arg" in
        --custom|--interactive) MODE="custom" ;;
        --system)   PREFIX="/usr/local" ;;
        --user)     PREFIX="$HOME/.local" ;;
        --prefix=*) PREFIX="${arg#*=}" ;;
        --with-tools) WITH_TOOLS=1 ;;
        --with-csl)   WITH_CSL=1 ;;
        --cse|--universal) WITH_CSE=1 ;;
        --no-so)      WITH_SO=0 ;;
        --no-source)  WITH_SRC=0 ;;
        --from-source|--source) FROM_SRC=1 ;;
        --source-dir=*) FROM_SRC=1; SOURCE_DIR="${arg#*=}" ;;
        --ref=*)     REF="${arg#*=}" ;;
        -h|--help) echo "usage: install.sh [--custom] [--system|--user|--prefix=DIR] [--with-tools] [--with-csl] [--cse]"
                   echo "       install.sh --from-source [--source-dir=DIR] [--ref=BRANCH]"; exit 0 ;;
        *) echo "warning: ignoring '$arg'" >&2 ;;
    esac
done

# --- interactive (reads /dev/tty so it works through a curl|sh pipe) ---
if [ "$MODE" = "custom" ] && [ -e /dev/tty ]; then
    ask() { printf "%s" "$1" >/dev/tty; read REPLY </dev/tty || REPLY=""; }
    echo "=== Caustic custom install ==="
    ask "Prefix — [1] \$HOME/.local (no sudo)  [2] /usr/local (sudo)  [3] custom  (default 1): "
    case "$REPLY" in 2) PREFIX="/usr/local" ;; 3) ask "  path: "; PREFIX="$REPLY" ;; *) PREFIX="$HOME/.local" ;; esac
    ask "Tools — [1] caustic only  [2] + caustic-as + caustic-ld  [3] everything  (default 1): "
    case "$REPLY" in 2|3) WITH_TOOLS=1 ;; *) WITH_TOOLS=0 ;; esac
    WITH_ALL_TOOLS=0; [ "$REPLY" = 3 ] && WITH_ALL_TOOLS=1
    ask "Compiler — [1] native x86_64 Linux  [2] universal .cse.exe (Linux+Windows+CausticOS, x86_64+ARM64)  (default 1): "
    case "$REPLY" in 2) WITH_CSE=1 ;; *) WITH_CSE=0 ;; esac
    ask "Stdlib — [1] source only  [2] source + libcaustic.so  [3] + libcaustic.csl  (default 2): "
    case "$REPLY" in 1) WITH_SO=0; WITH_CSL=0 ;; 3) WITH_SO=1; WITH_CSL=1 ;; *) WITH_SO=1; WITH_CSL=0 ;; esac
fi

# --- prefix default + sudo ---
[ -z "$PREFIX" ] && PREFIX="$HOME/.local"
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    parent="$PREFIX"
    while [ ! -e "$parent" ] && [ "$parent" != "/" ]; do parent=$(dirname "$parent"); done
    if [ ! -w "$parent" ]; then
        command -v sudo >/dev/null 2>&1 && { SUDO="sudo"; echo "note: $PREFIX needs root — using sudo"; } \
            || { echo "error: $PREFIX needs root and sudo is unavailable (use --user)"; exit 1; }
    fi
fi
BIN_DIR="$PREFIX/bin"; LIB_DIR="$PREFIX/lib/caustic"; STD_DIR="$LIB_DIR/std"

# --- obtain the tree to install ---
TMPDIR=$(mktemp -d); trap 'rm -rf "$TMPDIR"' EXIT INT TERM

is_checkout() { [ -f "$1/Causticfile" ] && [ -f "$1/src/main.cst" ]; }

if [ "$FROM_SRC" = 1 ]; then
    # Locate the checkout: an explicit --source-dir, the current directory if it
    # is one, or a fresh clone.
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
    # Prefer one already in the checkout, then one on PATH, and fall back to
    # the released binaries — which is what the bootstrap is for.
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

    # Build the toolchain, then let the Causticfile's own dist script stage it.
    # caustic-lsp is optional: it is Linux-only and not needed to compile.
    #
    # Output goes to a log rather than the terminal — packaging libcaustic warns
    # about the Windows symbols that a Linux .so cannot resolve, every time, and
    # that is expected noise. The log is printed if a step actually fails.
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
$SUDO mkdir -p "$BIN_DIR" "$STD_DIR"

# caustic (always — it has the assembler + linker built in)
# The universal build replaces this one; installing both would leave the native
# binary shadowed by a symlink a moment later.
if [ "$WITH_CSE" = 0 ]; then
    $SUDO cp "$SRC/bin/caustic" "$BIN_DIR/"; $SUDO chmod +x "$BIN_DIR/caustic"
fi
if [ "$WITH_TOOLS" = 1 ]; then
    for t in caustic-as caustic-ld; do
        [ -f "$SRC/bin/$t" ] && { $SUDO cp "$SRC/bin/$t" "$BIN_DIR/"; $SUDO chmod +x "$BIN_DIR/$t"; }
    done
    if [ "${WITH_ALL_TOOLS:-0}" = 1 ]; then
        for t in caustic-mk caustic-lsp; do
            [ -f "$SRC/bin/$t" ] && { $SUDO cp "$SRC/bin/$t" "$BIN_DIR/"; $SUDO chmod +x "$BIN_DIR/$t"; }
        done
    fi
fi

# Universal compiler. It is one file carrying a native body per OS/architecture;
# on Linux a shell stub picks the right ELF, which is why it needs the exec bit
# and keeps its .cse.exe name — the .exe is what lets the same file run on
# Windows, whose loader reads the MZ at offset 0.
if [ "$WITH_CSE" = 1 ]; then
    if [ -f "$SRC/bin/$UNIVERSAL" ]; then
        $SUDO cp "$SRC/bin/$UNIVERSAL" "$BIN_DIR/$UNIVERSAL"
    else
        need_curl
        curl -fsSL "https://github.com/$REPO/releases/latest/download/$UNIVERSAL" \
             -o "$TMPDIR/$UNIVERSAL" \
            || { echo "error: could not download $UNIVERSAL"; exit 1; }
        $SUDO cp "$TMPDIR/$UNIVERSAL" "$BIN_DIR/$UNIVERSAL"
    fi
    $SUDO chmod +x "$BIN_DIR/$UNIVERSAL"
    # `caustic` stays the name you type; the universal build answers to it.
    $SUDO ln -sf "$UNIVERSAL" "$BIN_DIR/caustic"
fi

# stdlib pieces
[ "$WITH_SRC" = 1 ] && $SUDO cp -R "$SRC"/lib/caustic/std/. "$STD_DIR/"
[ "$WITH_SO" = 1 ]  && [ -f "$SRC/lib/caustic/libcaustic.so" ]  && $SUDO cp "$SRC/lib/caustic/libcaustic.so"  "$LIB_DIR/"
[ "$WITH_CSL" = 1 ] && [ -f "$SRC/lib/caustic/libcaustic.csl" ] && $SUDO cp "$SRC/lib/caustic/libcaustic.csl" "$LIB_DIR/"

if [ "$WITH_CSE" = 1 ]; then
    echo "caustic installed → $BIN_DIR/caustic → $UNIVERSAL (universal)"
else
    echo "caustic installed → $BIN_DIR/caustic"
fi
command -v caustic >/dev/null 2>&1 || echo "  (add to PATH:  export PATH=\"$BIN_DIR:\$PATH\")"
