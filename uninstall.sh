#!/bin/sh
# Caustic uninstaller — removes exactly what install.sh put down.
#
#   ./uninstall.sh                    remove the install at the default prefix
#   ./uninstall.sh --prefix=DIR       remove the install at DIR
#   ./uninstall.sh --dry-run          list what would go, remove nothing
#   ./uninstall.sh --root=METHOD      pkexec | sudo | doas | none | auto
#
# It reads the manifest install.sh wrote, so it deletes the files it created and
# nothing else — important when the prefix is shared, like /usr/local.
set -eu

PREFIX=""
DRY=0
ROOT_METHOD="auto"
for arg in "$@"; do
    case "$arg" in
        --prefix=*) PREFIX="${arg#*=}" ;;
        --user)     PREFIX="$HOME/.local" ;;
        --system)   PREFIX="/usr/local" ;;
        --root=*)   ROOT_METHOD="${arg#*=}" ;;
        --dry-run)  DRY=1 ;;
        -h|--help)  sed -n '2,/^set -eu/p' "$0" | sed 's/^# \{0,1\}//; $d'; exit 0 ;;
        *) echo "warning: ignoring '$arg'" >&2 ;;
    esac
done

# Without an explicit prefix, look where install.sh puts things by default.
if [ -z "$PREFIX" ]; then
    for p in "$HOME/.local" "/usr/local"; do
        [ -f "$p/lib/caustic/install-manifest" ] && { PREFIX="$p"; break; }
    done
    [ -n "$PREFIX" ] || { echo "error: no caustic install found in \$HOME/.local or /usr/local (use --prefix=DIR)"; exit 1; }
fi

MANIFEST="$PREFIX/lib/caustic/install-manifest"
[ -f "$MANIFEST" ] || { echo "error: no manifest at $MANIFEST — nothing to uninstall (or it predates manifests)"; exit 1; }

SUDO=""
if [ "$(id -u)" -ne 0 ] && [ ! -w "$PREFIX" ]; then
    case "$ROOT_METHOD" in
        none) echo "error: $PREFIX needs root and --root=none was given"; exit 1 ;;
        auto) for m in pkexec sudo doas; do command -v "$m" >/dev/null 2>&1 && { SUDO="$m"; break; }; done
              [ -n "$SUDO" ] || { echo "error: $PREFIX needs root and no pkexec/sudo/doas found"; exit 1; } ;;
        *)    command -v "$ROOT_METHOD" >/dev/null 2>&1 || { echo "error: --root=$ROOT_METHOD not found"; exit 1; }
              SUDO="$ROOT_METHOD" ;;
    esac
    echo "note: $PREFIX needs root — using $SUDO"
fi
# One escalation for the whole uninstall, not one per file. pkexec does not keep
# an authorization between calls the way sudo does, so running each step
# through it asked for the password once per command — a dozen times for a
# system install. When root is needed the steps are written to a script and
# run by a single escalation at the end (run_root_now); without root they run
# as they come, as before.
ROOT_SCRIPT=""
q() { printf "'%s'" "$(printf '%s' "$1" | sed "s/'/'\\\\''/g")"; }
run() {
    if [ "$DRY" = 1 ]; then echo "  would: $*"; return 0; fi
    if [ -z "$SUDO" ]; then "$@"; return; fi
    _line=""
    for _a in "$@"; do _line="$_line $(q "$_a")"; done
    printf '%s\n' "$_line" >> "$ROOT_SCRIPT"
}
run_root_now() {
    [ "$DRY" = 1 ] && return 0
    [ -n "$SUDO" ] || return 0
    [ -s "$ROOT_SCRIPT" ] || return 0
    $SUDO sh -e "$ROOT_SCRIPT"
}
if [ -n "$SUDO" ] && [ "$DRY" != 1 ]; then
    ROOT_SCRIPT=$(mktemp); trap 'rm -f "$ROOT_SCRIPT"' EXIT INT TERM
fi

STDDIR=$(sed -n 's/^stddir=//p' "$MANIFEST")
echo "removing the caustic install at $PREFIX"

# Files, from the manifest's `files:` section to the end.
sed -n '/^files:/,$p' "$MANIFEST" | sed '1d' | while IFS= read -r f; do
    [ -n "$f" ] || continue
    if [ -e "$f" ] || [ -L "$f" ]; then run rm -f "$f"; else echo "  gone already: $f"; fi
done

# The stdlib sources are a directory install.sh copied wholesale.
[ -n "$STDDIR" ] && [ -d "$STDDIR" ] && run rm -rf "$STDDIR"
run rm -f "$MANIFEST"

# Only remove the directories if they are now empty — a shared prefix keeps
# whatever else lives there. The completion directories come first: they are
# the deepest, and rmdir only takes an empty one. "Now" means after the files
# are gone, so with the removals queued for one escalation the check is queued
# with them.
DIRS="$PREFIX/share/bash-completion/completions
$PREFIX/share/bash-completion
$PREFIX/share/zsh/site-functions
$PREFIX/share/zsh
$PREFIX/share/caustic
$PREFIX/share
$PREFIX/lib/caustic
$PREFIX/bin
$PREFIX/lib"
if [ -n "$SUDO" ] && [ "$DRY" != 1 ]; then
    printf '%s\n' "$DIRS" | while IFS= read -r d; do
        printf '[ -d %s ] && [ -z "$(ls -A %s 2>/dev/null)" ] && rmdir %s || true\n' "$(q "$d")" "$(q "$d")" "$(q "$d")" >> "$ROOT_SCRIPT"
    done
    run_root_now || { echo "error: removing $PREFIX failed"; exit 1; }
else
    printf '%s\n' "$DIRS" | while IFS= read -r d; do
        [ -d "$d" ] || continue
        if [ -z "$(ls -A "$d" 2>/dev/null)" ]; then run rmdir "$d"; fi
    done
fi

if [ "$DRY" = 1 ]; then
    echo "dry run complete — nothing was removed"
else
    echo "caustic removed from $PREFIX"
    case ":$PATH:" in
        *":$PREFIX/bin:"*) echo "  (PATH still mentions $PREFIX/bin — drop it from your shell profile)" ;;
    esac
fi
