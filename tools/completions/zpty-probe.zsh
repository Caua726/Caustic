#!/usr/bin/env zsh
#
# tools/completions/zpty-probe.zsh — drives a REAL zsh completion widget.
#
# Nothing else can check a #compdef file honestly: _arguments only runs inside a
# completion widget, so a test has to put a zsh on a pty, load the functions the
# way compinit does, press TAB and read back what the user would have seen.
#
# Called by selftest.sh with $FPDIR pointing at the directory holding the _*
# functions and the cwd set to a fixture project. Prints, per line tried:
#
#     <<<CASE <the line>
#     ... exactly what zsh listed ...
#     >>>END
emulate -L zsh

zmodload zsh/zpty 2>/dev/null || exit 1

local fpdir=${FPDIR:-${0:A:h}}
local zdir
zdir=$(mktemp -d "${TMPDIR:-/tmp}/caustic-zpty.XXXXXX") || exit 1
trap "rm -rf ${(q)zdir}" EXIT

# One TAB per line. Each costs a pty round trip, so keep the list short.
local -a cases=(
    'caustic --target='
    'caustic -'
    'caustic --mode='
    'caustic-as --target='
    'caustic-ld -'
    'caustic-ld --image='
    'caustic-mk '
    'caustic-mk build '
    'caustic-mk run '
    'caustic-mk build --profile '
    'caustic-mk build -D '
    'caustic-mk completions '
)

local DRAINED

# Read until the pty has been quiet for a moment. Waiting on a marker instead
# deadlocks the interesting case: a long match list fills the pty buffer, zsh
# blocks writing it, and the ^C that was supposed to end the case kills the
# listing halfway through. Draining continuously keeps the pipe moving.
_drain() {
    local chunk
    integer idle=0 quiet=${1:-15}
    DRAINED=""
    while (( idle < quiet )); do
        chunk=""
        if zpty -r -t zc chunk 2>/dev/null; then
            DRAINED+=$chunk
            idle=0
        else
            (( idle++ ))
            sleep 0.02
        fi
    done
}

zpty -b zc zsh -f || exit 1

# The prompt is assembled from two pieces so the marker never appears in the
# text typed below.
zpty -w zc "M1=CAUSTIC; M2=READY; PROMPT=\"\$M1\$M2%% \"; RPROMPT=''"
_drain
# A wide terminal: at 80 columns zsh wraps the match list and a candidate can
# end up split across two lines, where no grep would find it. Bracketed paste
# would wrap every reply in escape codes.
zpty -w zc "unset zle_bracketed_paste; stty cols 400 rows 200 2>/dev/null; LINES=200; COLUMNS=400"
_drain
zpty -w zc "fpath=(${(q)fpdir} \$fpath); autoload -Uz compinit; compinit -u -d ${(q)zdir}/zcompdump"
_drain 40
# Never page, never beep, and never try to correct the deliberately half-typed
# words below.
zpty -w zc "unsetopt correct correct_all list_beep; LISTMAX=100000; zstyle -d ':completion:*' list-prompt; zstyle -d ':completion:*' select-prompt"
_drain

local line
for line in $cases; do
    _drain 5                       # anything still pending is the last prompt
    zpty -w -n zc "$line"$'\t'
    _drain 25                      # the listing, read as it is produced
    print -r -- "<<<CASE $line"
    # Strip the escape sequences the line editor paints with; what is left is
    # the match list as a user would read it.
    print -r -- $DRAINED | sed -e $'s/\033\\[[0-9;?]*[a-zA-Z]//g' -e $'s/\033[()][A-Z0-9]//g' -e 's/\r/\n/g'
    print -r -- ">>>END"
    zpty -w -n zc $'\003'          # abandon the line, back to a clean prompt
done

zpty -d zc
