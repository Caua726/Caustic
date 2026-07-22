#!/usr/bin/env bash
#
# tools/prerelease.sh — pre-release readiness check (run BEFORE cutting a release).
#
# The one thing that is easy to forget when tagging a release is bumping the
# version: ship v0.0.17's binary tagged "v0.1.0" and every `caustic --version`
# in the wild lies. This script is the reminder. Run it before `git tag` /
# `gh release create`; it FAILS (blocking) if the version has not moved since
# the last release, so the dev is told to bump it instead of finding out after.
#
# Checks:
#   1. src/version.cst and Causticfile agree on the version string.
#   2. The version is NEWER than the latest release tag (not the same, not older).
#   3. A tag v<version> does not already exist (would collide on `git tag`).
#
# It reads only the working tree + git metadata — it builds nothing and mutates
# nothing. The self-host fixpoint / test gate is a separate step
# (tools/precommit.sh); this checks release *bookkeeping*, not correctness.
#
# Usage:  tools/prerelease.sh
# Skip:   PRERELEASE_SKIP=1 tools/prerelease.sh   (exit 0, no checks)

set -u
ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$ROOT" || exit 1

# ---- pretty output (mirrors tools/precommit.sh) ----------------------------
if [ -t 1 ]; then B=$'\e[1m'; G=$'\e[32m'; R=$'\e[31m'; Y=$'\e[33m'; D=$'\e[2m'; N=$'\e[0m'
else B=; G=; R=; Y=; D=; N=; fi
step() { printf "%s\n" "${B}▸ $*${N}"; }
ok()   { printf "  ${G}✓${N} %s\n" "$*"; }
info() { printf "  ${D}%s${N}\n" "$*"; }
die()  { printf "  ${R}✗ %s${N}\n" "$*"; printf "\n${R}${B}pre-release NOT ready${N} — %s\n" "${1:-fix the above before releasing}"; exit 1; }

if [ "${PRERELEASE_SKIP:-0}" = "1" ]; then echo "${Y}pre-release check skipped (PRERELEASE_SKIP=1)${N}"; exit 0; fi

# ---- read the version from its two sources of truth ------------------------
step "Version bookkeeping"

VER_SRC="$(grep -oE '"[0-9]+\.[0-9]+\.[0-9]+[^"]*"' src/version.cst 2>/dev/null | head -1 | tr -d '"')"
VER_MK="$(grep -E '^[[:space:]]*version[[:space:]]' Causticfile 2>/dev/null | grep -oE '"[^"]+"' | head -1 | tr -d '"')"

[ -n "$VER_SRC" ] || die "could not read a version from src/version.cst"
[ -n "$VER_MK" ]  || die "could not read a version from Causticfile"

# 1. the two sources must agree
if [ "$VER_SRC" != "$VER_MK" ]; then
    printf "  ${R}✗${N} version mismatch: src/version.cst=${B}%s${N} vs Causticfile=${B}%s${N}\n" "$VER_SRC" "$VER_MK"
    die "make src/version.cst and Causticfile agree on the version"
fi
VER="$VER_SRC"
ok "src/version.cst and Causticfile agree: ${B}$VER${N}"

# ---- compare against the latest release tag --------------------------------
LAST_TAG="$(git tag --sort=-v:refname 2>/dev/null | grep -E '^v?[0-9]' | head -1)"

if [ -z "$LAST_TAG" ]; then
    ok "no prior release tag — this would be the first release (v$VER)"
    LAST_VER=""
else
    LAST_VER="${LAST_TAG#v}"
    info "latest release tag: $LAST_TAG"
fi

# 2. version must be strictly newer than the last release
if [ -n "$LAST_VER" ]; then
    if [ "$VER" = "$LAST_VER" ]; then
        printf "  ${R}✗${N} version is still ${B}%s${N} — unchanged since the last release (%s)\n" "$VER" "$LAST_TAG"
        printf "      ${Y}Bump it before releasing:${N}\n"
        printf "        • src/version.cst  →  let is *u8 as VERSION with imut = \"X.Y.Z\";\n"
        printf "        • Causticfile      →  version \"X.Y.Z\"\n"
        die "bump the version — you are about to re-release $LAST_TAG"
    fi
    # strict-newer via version sort: the max of {last, current} must be current, and they differ
    NEWEST="$(printf '%s\n%s\n' "$LAST_VER" "$VER" | sort -V | tail -1)"
    if [ "$NEWEST" != "$VER" ]; then
        printf "  ${R}✗${N} version ${B}%s${N} is OLDER than the last release ${B}%s${N}\n" "$VER" "$LAST_VER"
        die "the new version must be greater than $LAST_TAG"
    fi
    ok "version ${B}$VER${N} is newer than the last release ($LAST_TAG)"
fi

# 3. the tag we are about to create must not already exist
if git rev-parse "v$VER" >/dev/null 2>&1; then
    printf "  ${R}✗${N} tag ${B}v%s${N} already exists\n" "$VER"
    die "delete the existing tag or bump to an unused version"
fi
ok "tag ${B}v$VER${N} is free"

# ---- context: what would ship ----------------------------------------------
if [ -n "$LAST_TAG" ]; then
    NCOMMITS="$(git rev-list --count "$LAST_TAG"..HEAD 2>/dev/null || echo '?')"
    step "Release scope"
    info "$NCOMMITS commits since $LAST_TAG"
    info "changelog:  git log --oneline $LAST_TAG..HEAD"
fi

printf "\n${G}${B}pre-release ready${N} — version ${B}v$VER${N}.\n"
printf "${D}Release flow:${N}\n"
printf "${D}  1. tools/precommit.sh       — self-host fixpoint gate (-O0/-O1/-O2)${N}\n"
printf "${D}  2. tools/release-build.sh   — ship binary = gen4 -O2 self-host fixpoint (whole${N}\n"
printf "${D}                                toolchain rebuilt 4x), then packs the tarball${N}\n"
printf "${D}  3. git tag v$VER && gh release create v$VER caustic-x86_64-linux.tar.gz${N}\n"
exit 0
