#!/bin/sh
# Run a command in an owned Wine prefix and a responsive, bounded Xvfb session.
set -eu
[ "$#" -gt 0 ] || { echo "usage: sh tests/windows/run_wine.sh command [args]" >&2; exit 2; }
work=$(mktemp -d "${TMPDIR:-/tmp}/caustic-wine-tests.XXXXXX")
export WINEPREFIX="$work/wine" WINEARCH=win64 WINEDEBUG=-all WINEDLLOVERRIDES=winedbg.exe=d
unset WAYLAND_DISPLAY
xvfb_pid=""
cleanup() {
    status=$?
    trap - EXIT HUP INT TERM
    wineserver -k 2>/dev/null || true
    retain=0
    if ! timeout 10 wineserver -w 2>/dev/null; then
        echo "Wine runner: shutdown did not finish; retaining owned prefix $work" >&2
        retain=1
        status=1
    fi
    if [ -n "$xvfb_pid" ]; then
        kill "$xvfb_pid" 2>/dev/null || true
        wait "$xvfb_pid" 2>/dev/null || true
    fi
    if [ "$retain" -eq 0 ]; then rm -rf "$work"; fi
    exit "$status"
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
: > "$work/display"
Xvfb -displayfd 3 -screen 0 1280x1024x24 -nolisten tcp -noreset 3>"$work/display" >"$work/xvfb.log" 2>&1 &
xvfb_pid=$!
if ! timeout 5 sh -c '
    while kill -0 "$1" 2>/dev/null; do
        if IFS= read -r display < "$2"; then
            export DISPLAY=":$display"
            if xdpyinfo >/dev/null 2>&1; then exit 0; fi
        fi
        sleep 0.05
    done
    exit 1
' wine-readiness "$xvfb_pid" "$work/display"; then
    cat "$work/xvfb.log" >&2
    echo "Wine runner: X server did not answer within 5s" >&2
    exit 1
fi
IFS= read -r display < "$work/display"
export DISPLAY=":$display"
"$@"
