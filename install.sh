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
# Non-interactive flags: --system | --user | --prefix=DIR
#                        --with-tools  (caustic-as, caustic-ld, caustic-mk, caustic-lsp)
#                        --with-csl    (universal libcaustic.csl)  --no-so  --no-source
set -eu

REPO="Caua726/Caustic"
TARBALL="caustic-x86_64-linux.tar.gz"

ARCH=$(uname -m 2>/dev/null || echo unknown)
[ "$ARCH" = "x86_64" ] || { echo "error: unsupported architecture '$ARCH' (x86_64 only)"; exit 1; }
[ "$(uname -s 2>/dev/null)" = "Linux" ] || { echo "error: this installer targets Linux"; exit 1; }
command -v curl >/dev/null 2>&1 || { echo "error: 'curl' is required"; exit 1; }
command -v tar  >/dev/null 2>&1 || { echo "error: 'tar' is required"; exit 1; }

# --- options ---
MODE="default"
PREFIX=""
WITH_TOOLS=0      # caustic-as / caustic-ld / caustic-mk / caustic-lsp
WITH_CSL=0        # universal libcaustic.csl
WITH_SO=1         # libcaustic.so
WITH_SRC=1        # stdlib source (.cst) — required to compile against the stdlib
for arg in "$@"; do
    case "$arg" in
        --custom|--interactive) MODE="custom" ;;
        --system)   PREFIX="/usr/local" ;;
        --user)     PREFIX="$HOME/.local" ;;
        --prefix=*) PREFIX="${arg#*=}" ;;
        --with-tools) WITH_TOOLS=1 ;;
        --with-csl)   WITH_CSL=1 ;;
        --no-so)      WITH_SO=0 ;;
        --no-source)  WITH_SRC=0 ;;
        -h|--help) echo "usage: install.sh [--custom] [--system|--user|--prefix=DIR] [--with-tools] [--with-csl]"; exit 0 ;;
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

# --- download + extract ---
TMPDIR=$(mktemp -d); trap 'rm -rf "$TMPDIR"' EXIT INT TERM
echo "downloading latest release ..."
curl -fsSL "https://github.com/$REPO/releases/latest/download/$TARBALL" -o "$TMPDIR/$TARBALL" \
    || { echo "error: download failed"; exit 1; }
tar xzf "$TMPDIR/$TARBALL" -C "$TMPDIR"
SRC="$TMPDIR/caustic-x86_64-linux"

echo "installing to $PREFIX ..."
$SUDO mkdir -p "$BIN_DIR" "$STD_DIR"

# caustic (always — it has the assembler + linker built in)
$SUDO cp "$SRC/bin/caustic" "$BIN_DIR/"; $SUDO chmod +x "$BIN_DIR/caustic"
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

# stdlib pieces
[ "$WITH_SRC" = 1 ] && $SUDO cp -R "$SRC"/lib/caustic/std/. "$STD_DIR/"
[ "$WITH_SO" = 1 ]  && [ -f "$SRC/lib/caustic/libcaustic.so" ]  && $SUDO cp "$SRC/lib/caustic/libcaustic.so"  "$LIB_DIR/"
[ "$WITH_CSL" = 1 ] && [ -f "$SRC/lib/caustic/libcaustic.csl" ] && $SUDO cp "$SRC/lib/caustic/libcaustic.csl" "$LIB_DIR/"

echo "caustic installed → $BIN_DIR/caustic"
command -v caustic >/dev/null 2>&1 || echo "  (add to PATH:  export PATH=\"$BIN_DIR:\$PATH\")"
