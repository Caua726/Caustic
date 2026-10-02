#!/bin/sh
# Caustic updater — reinstalls the latest release with the choices you made the
# first time. An install built from source is rebuilt from source instead, from
# the branch it came from.
#
#   ./update.sh                       update the install at the default prefix
#   ./update.sh --prefix=DIR          update the install at DIR
#   ./update.sh --check               report versions, change nothing
#   ./update.sh --dry-run             show the install it would run
#
# It reads the manifest install.sh wrote and replays the same flags, so the
# flavour you picked — native or universal compiler, which tools, which shared
# stdlib — survives the update instead of silently reverting to the defaults.
# Anything you pass here overrides the recorded value for this run.
set -eu

REPO="Caua726/Caustic"
PREFIX=""
PASSTHRU=""
CHECK=0
for arg in "$@"; do
    case "$arg" in
        --prefix=*) PREFIX="${arg#*=}"; PASSTHRU="$PASSTHRU $arg" ;;
        --user)     PREFIX="$HOME/.local" ;;
        --system)   PREFIX="/usr/local" ;;
        --check)    CHECK=1 ;;
        -h|--help)  sed -n '2,/^set -eu/p' "$0" | sed 's/^# \{0,1\}//; $d'; exit 0 ;;
        *) PASSTHRU="$PASSTHRU $arg" ;;
    esac
done

if [ -z "$PREFIX" ]; then
    for p in "$HOME/.local" "/usr/local"; do
        [ -f "$p/lib/caustic/install-manifest" ] && { PREFIX="$p"; break; }
    done
    [ -n "$PREFIX" ] || { echo "error: no caustic install found (use --prefix=DIR, or install.sh for a first install)"; exit 1; }
fi

MANIFEST="$PREFIX/lib/caustic/install-manifest"
[ -f "$MANIFEST" ] || { echo "error: no manifest at $MANIFEST — run install.sh instead"; exit 1; }
get() { sed -n "s/^$1=//p" "$MANIFEST"; }

M_FORMAT=$(get format); M_TOOLS=$(get tools); M_LIB=$(get lib)
M_SOURCE=$(get source); M_ROOT=$(get root); M_COMP=$(get completions)
# An install that predates these lines came from a release.
M_BUILD=$(get build); M_REF=$(get ref); M_COMMIT=$(get commit)
[ -n "$M_REF" ] || M_REF="main"

HAVE=""
[ -x "$PREFIX/bin/caustic" ] && HAVE=$("$PREFIX/bin/caustic" --version 2>/dev/null | head -1 | awk '{print $2}')
LATEST=""
CURRENT=0
AHEAD=0
if [ "$M_BUILD" = "source" ]; then
    # A build from source is current when it was built from the commit its
    # branch points at now. Comparing it with the latest release would call
    # every build past that release out of date, and offer to downgrade it.
    REMOTE=""
    command -v git >/dev/null 2>&1 && REMOTE=$(GIT_TERMINAL_PROMPT=0 \
        git ls-remote "https://github.com/$REPO.git" "refs/heads/$M_REF" 2>/dev/null | cut -f1)
    if [ -n "$REMOTE" ]; then
        LATEST="$M_REF @ $(echo "$REMOTE" | cut -c1-7)"
        [ "$REMOTE" = "$M_COMMIT" ] && CURRENT=1
    fi
    [ -n "$M_COMMIT" ] && HAVE="${HAVE:-unknown} ($M_REF @ $(echo "$M_COMMIT" | cut -c1-7))"
else
    if command -v curl >/dev/null 2>&1; then
        # The redirect target of /releases/latest ends in the tag, which is the
        # cheapest way to ask "what is current" without an API token.
        LATEST=$(curl -fsSLI -o /dev/null -w '%{url_effective}' \
                 "https://github.com/$REPO/releases/latest" 2>/dev/null | sed 's|.*/tag/v\{0,1\}||')
    fi
    [ -n "$HAVE" ] && [ -n "$LATEST" ] && [ "$HAVE" = "$LATEST" ] && CURRENT=1
    # Newer than the latest release can only be a build from source made before
    # the manifest recorded that. "Updating" it would install an older compiler.
    if [ "$CURRENT" = 0 ] && [ -n "$HAVE" ] && [ -n "$LATEST" ] \
       && [ "$(printf '%s\n%s\n' "$LATEST" "$HAVE" | sort -V 2>/dev/null | tail -1)" = "$HAVE" ]; then
        AHEAD=1
    fi
fi

echo "caustic update"
echo "  prefix:    $PREFIX"
echo "  installed: ${HAVE:-unknown}"
echo "  latest:    ${LATEST:-unknown}"
echo "  replaying: format=$M_FORMAT tools=$M_TOOLS lib=$M_LIB source=$M_SOURCE root=$M_ROOT completions=${M_COMP:-auto}$([ "$M_BUILD" = "source" ] && echo " from-source ref=$M_REF")"

if [ "$AHEAD" = 1 ]; then
    echo "$HAVE is newer than the latest release ($LATEST) — not replacing it with an older one."
    echo "It was built from source; rebuild it from main with:  install.sh --from-source"
    [ "$CHECK" = 1 ] && exit 0
    exit 1
fi
if [ "$CHECK" = 1 ]; then
    if [ "$CURRENT" = 1 ]; then
        echo "up to date"
    elif [ -z "$LATEST" ]; then
        echo "could not find out what is current — nothing to compare against"
    else
        echo "an update is available — run without --check to install it"
    fi
    exit 0
fi
if [ "$CURRENT" = 1 ]; then
    echo "already at $HAVE — reinstalling anyway"
fi

# Prefer the install.sh sitting next to this script; fall back to fetching it,
# which is what the piped one-liner needs.
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
INSTALLER="$DIR/install.sh"
TMP=""
if [ ! -f "$INSTALLER" ]; then
    command -v curl >/dev/null 2>&1 || { echo "error: 'curl' is required to fetch install.sh"; exit 1; }
    TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT INT TERM
    curl -fsSL "https://raw.githubusercontent.com/$REPO/main/install.sh" -o "$TMP/install.sh" \
        || { echo "error: could not fetch install.sh"; exit 1; }
    INSTALLER="$TMP/install.sh"
fi

SRCFLAG=""
[ "$M_SOURCE" = "0" ] && SRCFLAG="--no-source"
# An install that predates shell completions has no `completions=` line. Leaving
# the flag off then means "auto", so the update adds them — which is the point of
# updating; passing an empty value would instead mean "none" forever.
COMPFLAG=""
[ -n "$M_COMP" ] && COMPFLAG="--completions=$M_COMP"
BUILDFLAG=""
[ "$M_BUILD" = "source" ] && BUILDFLAG="--from-source --ref=$M_REF"
# shellcheck disable=SC2086
sh "$INSTALLER" --prefix="$PREFIX" --format="$M_FORMAT" --tools="$M_TOOLS" \
                --lib="$M_LIB" --root="$M_ROOT" $SRCFLAG $COMPFLAG $BUILDFLAG $PASSTHRU
