#!/usr/bin/env bash
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# SPDX-FileCopyrightText: 2026 PeriBluGaming
#
# Apply this title's local psxrecomp patches to the pinned submodule checkout.
#
# Usage: scripts/apply_framework_patches.sh [--check] [--root DIR]
#   --check  Verify only and change nothing. Exit 0 when the patches are
#            applied exactly, 2 when psxrecomp is the clean base revision,
#            1 on any other state.
#   --root   Title repository root (default: the parent of scripts/).
#
# patches/psxrecomp/BASE names the psxrecomp revision the patches are made
# against; patches/psxrecomp/NNNN-*.patch apply in file-name order. The
# framework working tree must be either the clean base revision (the series
# is then applied) or exactly the base plus the whole series (nothing to do).
# Any other state -- another revision, a partly applied series, foreign
# edits, extra or staged files, changed nested submodules, a damaged patch --
# stops with an explanation and changes nothing. If the applied result does
# not match the expected one, this run takes its own changes back. Only the
# submodule's working tree is written: its index and HEAD and the
# superproject's gitlink stay as they are. Runs on bash 3.2 (macOS), Linux and
# Git for Windows / MSYS2.
set -euo pipefail
export LC_ALL=C

die() { printf 'apply_framework_patches: error: %s\n' "$*" >&2; exit 1; }
note() { printf 'apply_framework_patches: %s\n' "$*"; }

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CHECK=0
while [ $# -gt 0 ]; do
  case "$1" in
    --check) CHECK=1; shift ;;
    --root)
      [ $# -ge 2 ] || die "--root needs a directory"
      ROOT="$(cd "$2" && pwd)" || die "no such directory: $2"
      shift 2 ;;
    -h|--help) sed -n '5,23p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) die "unknown argument: $1 (see --help)" ;;
  esac
done

FW="$ROOT/psxrecomp"
PATCH_DIR="$ROOT/patches/psxrecomp"

command -v git >/dev/null 2>&1 || die "git not found"
[ -e "$FW/.git" ] ||
  die "$FW is not a git checkout; run: git submodule update --init --recursive"
[ -f "$PATCH_DIR/BASE" ] || die "missing $PATCH_DIR/BASE"

BASE="$(grep -v '^[[:space:]]*#' "$PATCH_DIR/BASE" | tr -d '[:space:]')"
printf '%s' "$BASE" | grep -Eq '^[0-9a-f]{40}$' ||
  die "$PATCH_DIR/BASE must hold one full 40-character revision"

HEAD="$(git -C "$FW" rev-parse HEAD 2>/dev/null)" || die "cannot read the psxrecomp revision"
[ "$HEAD" = "$BASE" ] ||
  die "psxrecomp is at $HEAD but the patches are made against $BASE. Restore the pin (git submodule update --init --recursive) or revalidate the patches for the new revision."

PATCHES=()
for p in "$PATCH_DIR"/[0-9][0-9][0-9][0-9]-*.patch; do
  [ -f "$p" ] && PATCHES+=("$p")
done
[ ${#PATCHES[@]} -gt 0 ] || die "no patches found in $PATCH_DIR"

# A patch checked out with CRLF line endings (core.autocrlf without this
# repository's .gitattributes) matches no context line, so stop early.
CR="$(printf '\r')"
for p in "${PATCHES[@]}"; do
  if grep -q "$CR" "$p"; then
    die "$(basename "$p") has CR (CRLF) line endings and cannot apply. The repository's .gitattributes keeps patches byte-exact; check it is present and re-check the patches out: git checkout -- patches/psxrecomp"
  fi
done

sha256() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
  else shasum -a 256 "$1" | cut -d' ' -f1
  fi
}

WORK="$(mktemp -d 2>/dev/null || mktemp -d -t fwpatch)"
trap 'rm -rf "$WORK"' EXIT
WORK="$(cd "$WORK" && pwd -P)"
EXPECTED="$WORK/expected"
PATHS="$WORK/paths"
mkdir -p "$EXPECTED"

# The useful part of a failed git apply: its error and fatal lines. Whitespace
# warnings come first and would hide the cause; without such lines, its last
# lines.
apply_errors() {
  local found
  found="$(grep -E '^(error|fatal):' "$WORK/apply.err" | head -n 5 | tr '\n' ' ' || true)"
  [ -n "$found" ] || found="$(tail -n 3 "$WORK/apply.err" | tr '\n' ' ')"
  [ -n "$found" ] || found="(git apply printed no diagnostics)"
  printf '%s' "${found% }"
}

# Every path the series touches (the post-image name of each file diff).
: >"$PATHS"
for p in "${PATCHES[@]}"; do
  grep '^diff --git ' "$p" | while read -r _ _ a b extra; do
    [ -z "${extra:-}" ] && [ "${a#a/}" != "$a" ] && [ "${b#b/}" != "$b" ] ||
      die "unsupported file header in $(basename "$p"): diff --git $a $b ${extra:-}"
    printf '%s\n' "${b#b/}"
  done >>"$PATHS"
done
sort -u "$PATHS" -o "$PATHS"
[ -s "$PATHS" ] || die "the patches touch no files"

# Expected result: the base version of every touched file with the whole
# series applied, built outside any repository so nothing in psxrecomp is
# read from its working tree or written.
while read -r path; do
  if git -C "$FW" cat-file -e "$BASE:$path" 2>/dev/null; then
    mkdir -p "$EXPECTED/$(dirname "$path")"
    git -C "$FW" show "$BASE:$path" >"$EXPECTED/$path"
  fi
done <"$PATHS"
( cd "$EXPECTED" && GIT_CEILING_DIRECTORIES="$WORK" git apply "${PATCHES[@]}" ) 2>"$WORK/apply.err" ||
  die "the patch series does not apply to $BASE (damaged or outdated patch): $(apply_errors)"

changed_paths() {
  git -C "$FW" status --porcelain --untracked-files=all --ignore-submodules=none |
    sed -e 's/^...//' -e 's/.* -> //'
}

changed_list() { changed_paths | tr '\n' ';'; }

# Paths that differ from the expected patched state, one per line. Both sides
# are hashed through psxrecomp's own clean filters for the path, so a checkout
# with converted line endings (core.autocrlf on Windows, which also applies to
# the git apply that built the expected files) compares by content as git
# stores it; any change to that content still differs.
mismatches() {
  local path want have
  while read -r path; do
    if [ -f "$EXPECTED/$path" ]; then
      if [ ! -f "$FW/$path" ]; then
        printf 'missing: %s\n' "$path"; continue
      fi
      want="$(git -C "$FW" hash-object --path="$path" "$EXPECTED/$path")"
      have="$(git -C "$FW" hash-object "$path")"
      [ "$want" = "$have" ] || printf 'differs: %s\n' "$path"
    elif [ -e "$FW/$path" ]; then
      printf 'should be deleted: %s\n' "$path"
    fi
  done <"$PATHS"
  changed_paths | grep -vxF -f "$PATHS" | sed 's/^/foreign change: /' || true
}

git -C "$FW" diff --cached --quiet ||
  die "psxrecomp has staged changes; the patches only touch the working tree. Unstage them first."

for p in "${PATCHES[@]}"; do
  note "patch $(basename "$p") sha256 $(sha256 "$p")"
done

if [ -z "$(changed_paths)" ]; then
  if [ "$CHECK" -eq 1 ]; then
    note "psxrecomp $BASE is clean; the patches are not applied"
    exit 2
  fi
  git -C "$FW" apply --check "${PATCHES[@]}" 2>"$WORK/apply.err" ||
    die "the patches do not apply to psxrecomp: $(apply_errors)"
  # psxrecomp is clean here (no staged, changed or untracked files), so every
  # change from now on is this run's own.
  RESTORE="Restore psxrecomp (git -C psxrecomp checkout -- . and remove the untracked files) and run this script again."
  if ! git -C "$FW" apply "${PATCHES[@]}" 2>"$WORK/apply.err"; then
    err="$(apply_errors)"
    [ -n "$(changed_paths)" ] ||
      die "applying the patches failed: $err. psxrecomp is unchanged."
    die "applying the patches failed part-way: $err. psxrecomp was clean before this run, so these changes are its own: $(changed_list) $RESTORE"
  fi
  left="$(mismatches)"
  if [ -n "$left" ]; then
    left="$(printf '%s' "$left" | tr '\n' ';')"
    # Take back exactly the series this run applied, last patch first.
    REVERSED=()
    for p in "${PATCHES[@]}"; do REVERSED=("$p" ${REVERSED[@]+"${REVERSED[@]}"}); done
    if ! git -C "$FW" apply -R "${REVERSED[@]}" 2>"$WORK/apply.err"; then
      die "the applied result differs from the expected one ($left) and reverting this run's patches failed: $(apply_errors). psxrecomp still has: $(changed_list) $RESTORE"
    fi
    rest="$(changed_list)"
    [ -z "$rest" ] ||
      die "the applied result differs from the expected one ($left); the patches were reverted, but psxrecomp still has: $rest $RESTORE"
    die "the applied result differs from the expected one ($left; filter or line-ending settings?). This run's patches were reverted; psxrecomp is the clean base again."
  fi
  note "applied ${#PATCHES[@]} patch(es) to psxrecomp $BASE"
  exit 0
fi

left="$(mismatches)"
if [ -z "$left" ]; then
  note "psxrecomp $BASE already carries exactly these patches; nothing to do"
  exit 0
fi
printf '%s\n' "$left" | sed 's/^/  /' >&2
die "psxrecomp has changes that are neither the clean base nor exactly the patched state (partly applied, edited or extra files). Nothing was changed. Restore psxrecomp (git -C psxrecomp checkout -- . and remove untracked files) and run this script again."
