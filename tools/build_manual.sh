#!/bin/sh
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
#
# Builds the PDF user manual from the Markdown chapters in docs/user/ (docs/dev/manual-pdf.md).
#
#   tools/build_manual.sh                    # build/manual/RadioFoundry-manual-<version>.pdf
#   tools/build_manual.sh --version v26.11   # the version printed on the title page and in the file name
#   tools/build_manual.sh --typ-only         # stop after the typst source (no typst needed)
#   tools/build_manual.sh --out FILE.pdf
#
# Needs pandoc (3.1.12 or later) and typst (0.12 or later) on PATH, or $PANDOC and $TYPST. Nothing else: no LaTeX,
# no fonts beyond the ones typst carries. Chapters are the files docs/user/NN-*.md in name order; docs/user/README.md
# is the GitHub index and is not part of the PDF. Set RFOUNDRY_REPO_URL (for example
# https://github.com/OWNER/REPO/blob/v26.11) to turn links into the rest of the repository into web links; without
# it they print as plain text.
set -eu

cd "$(dirname "$0")/.."
ROOT=$(pwd)
PANDOC=${PANDOC:-pandoc}
TYPST=${TYPST:-typst}
VERSION=""
OUT=""
TYP_ONLY=0
LANG_CODE=""

while [ $# -gt 0 ]; do
	case "$1" in
	--version) VERSION=$2; shift 2 ;;
	--out) OUT=$2; shift 2 ;;
	--lang) LANG_CODE=$2; shift 2 ;;
	--typ-only) TYP_ONLY=1; shift ;;
	-h|--help) sed -n '5,16p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
	*) echo "build_manual.sh: unknown argument $1" >&2; exit 2 ;;
	esac
done

if [ -z "$VERSION" ]; then
	VERSION=$(git describe --tags --always --dirty 2> /dev/null || echo unknown)
fi

need() {
	if ! command -v "$1" > /dev/null 2>&1; then
		echo "build_manual.sh: $1 not found (docs/dev/manual-pdf.md says how to install it)" >&2
		exit 2
	fi
}
need "$PANDOC"
[ "$TYP_ONLY" = 1 ] || need "$TYPST"

BUILD="$ROOT/build/manual"
mkdir -p "$BUILD"
[ -n "$OUT" ] || OUT="$BUILD/RadioFoundry-manual-$VERSION.pdf"
TYP="$BUILD/manual.typ"

# Chapters in order. A language the site translates docs into keeps them under docs/i18n/<code>/user/, mirroring the
# English tree chapter for chapter; a chapter with no translation is skipped rather than built from the English, so
# the PDF a language gets is one in that language throughout.
SRC_DIR="docs/user"
if [ -n "$LANG_CODE" ]; then
	SRC_DIR="docs/i18n/$LANG_CODE/user"
	if [ ! -d "$SRC_DIR" ]; then
		echo "build_manual.sh: no chapters in $SRC_DIR" >&2
		exit 2
	fi
fi
CHAPTERS=$(cd "$SRC_DIR" && ls [0-9][0-9][0-9]*-*.md 2>/dev/null || ls [0-9][0-9]*[a-z]-*.md [0-9][0-9]-*.md 2>/dev/null | sort -u)
if [ -z "$CHAPTERS" ]; then
	echo "build_manual.sh: no chapters in $SRC_DIR" >&2
	exit 2
fi

DATE=$(date -u +%Y-%m-%d)
TITLE="RadioFoundry User Manual"
MD_LANG="en"
MD_REGION="US"
if [ -n "$LANG_CODE" ]; then
	MD_LANG=$LANG_CODE
	case "$LANG_CODE" in
	zh) TITLE="RadioFoundry 用户手册"; MD_REGION="CN" ;;
	es) TITLE="Manual de usuario de RadioFoundry"; MD_REGION="ES" ;;
	it) TITLE="Manuale utente di RadioFoundry"; MD_REGION="IT" ;;
	fr) TITLE="Manuel d'utilisation de RadioFoundry"; MD_REGION="FR" ;;
	esac
fi
# shellcheck disable=SC2086 # the chapter list is split on purpose
(cd "$SRC_DIR" && RFOUNDRY_MANUAL_DIR="$SRC_DIR" RFOUNDRY_REPO_ROOT="$ROOT" "$PANDOC" \
	--from=gfm --file-scope \
	--to=typst --standalone \
	--template="$ROOT/tools/manual/manual.typ" \
	--lua-filter="$ROOT/tools/manual/manual.lua" \
	--toc --toc-depth=2 \
	--metadata title="$TITLE" \
	--metadata subtitle="Version $VERSION" \
	--metadata lang="$MD_LANG" \
	--metadata region="$MD_REGION" \
	--metadata date="$DATE" \
	--output="$TYP" \
	$CHAPTERS)
echo "manual: $TYP"

if [ "$TYP_ONLY" = 1 ]; then
	exit 0
fi
# Typst carries Latin faces only. A translated manual needs a face for its script: RFOUNDRY_MANUAL_FONTS names a
# directory of them (the manual workflow fetches one), and the template falls back to it through `fallback` below
FONT_ARGS=""
if [ -n "${RFOUNDRY_MANUAL_FONTS:-}" ] && [ -d "$RFOUNDRY_MANUAL_FONTS" ]; then
	FONT_ARGS="--font-path $RFOUNDRY_MANUAL_FONTS"
fi
# shellcheck disable=SC2086 # one optional flag, split on purpose
"$TYPST" compile --root "$ROOT" $FONT_ARGS "$TYP" "$OUT"
echo "manual: $OUT"
