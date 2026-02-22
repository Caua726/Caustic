#!/bin/sh
set -e

REPO="caua-oliveira/caustic"
TARBALL="caustic-x86_64-linux.tar.gz"

# --- detect arch ---
ARCH=$(uname -m)
if [ "$ARCH" != "x86_64" ]; then
    echo "error: unsupported architecture '$ARCH' (only x86_64)"
    exit 1
fi

OS=$(uname -s)
if [ "$OS" != "Linux" ]; then
    echo "error: unsupported OS '$OS' (only Linux)"
    exit 1
fi

# --- detect prefix ---
if [ "$(id -u)" -eq 0 ]; then
    PREFIX="/usr/local"
else
    PREFIX="$HOME/.local"
fi

BIN_DIR="$PREFIX/bin"
LIB_DIR="$PREFIX/lib/caustic/std"

echo "installing caustic to $PREFIX ..."

# --- download latest release ---
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

echo "downloading latest release ..."
curl -sL "https://github.com/$REPO/releases/latest/download/$TARBALL" -o "$TMPDIR/$TARBALL"

# --- extract ---
echo "extracting ..."
tar xzf "$TMPDIR/$TARBALL" -C "$TMPDIR"

# --- install ---
mkdir -p "$BIN_DIR"
mkdir -p "$LIB_DIR"

cp "$TMPDIR/caustic-x86_64-linux/bin/caustic"    "$BIN_DIR/caustic"
cp "$TMPDIR/caustic-x86_64-linux/bin/caustic-as"  "$BIN_DIR/caustic-as"
cp "$TMPDIR/caustic-x86_64-linux/bin/caustic-ld"  "$BIN_DIR/caustic-ld"
cp "$TMPDIR/caustic-x86_64-linux/lib/caustic/std/"*.cst "$LIB_DIR/"

chmod +x "$BIN_DIR/caustic" "$BIN_DIR/caustic-as" "$BIN_DIR/caustic-ld"

# --- verify ---
if command -v caustic >/dev/null 2>&1; then
    echo "caustic installed successfully!"
else
    echo "installed to $BIN_DIR — make sure it's in your PATH"
    echo "  export PATH=\"$BIN_DIR:\$PATH\""
fi
