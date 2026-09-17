#!/usr/bin/env bash
# gen_downloads_page.sh — fill in the downloads page for baker76.com/swos.html.
#
#   scripts/gen_downloads_page.sh              # packaging/web/swos.html -> dist/swos.html
#   scripts/gen_downloads_page.sh --out X.html
#
# The template is packaging/web/swos.html and carries @PLACEHOLDERS@ for the
# build, the date and each archive's size. This fills them from the drop that is
# actually in dist/, so the page can never advertise a build or a size that is
# not the one being published.
#
# scripts/upload_dist.sh runs this immediately before it uploads (every archive
# is present and checked by then) and publishes the result to the site root.
# scripts/build_dist.sh runs it too, so a drop can be looked at locally — but the
# macOS archives come from scripts/build_macos.sh AFTER that, so the sizes in
# dist/swos.html are only final once the upload regenerates it.
#
# A leftover @PLACEHOLDER@ is a failure, not a cosmetic slip: it would be
# published verbatim.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1

TEMPLATE=packaging/web/swos.html
OUT=dist/swos.html

while [ $# -gt 0 ]; do
    case "$1" in
        --out) OUT="$2"; shift 2 ;;
        --template) TEMPLATE="$2"; shift 2 ;;
        -h|--help) sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option $1" >&2; exit 1 ;;
    esac
done

[ -f "$TEMPLATE" ] || { echo "gen_downloads_page: no template at $TEMPLATE" >&2; exit 1; }

# Human sizes, the way a download page writes them: "17 MB", "2.1 MB".
size_of() {
    local f="dist/$1" b
    [ -f "$f" ] || { echo "&mdash;"; return; }
    b=$(stat -c %s "$f" 2>/dev/null || stat -f %z "$f" 2>/dev/null) || { echo "&mdash;"; return; }
    awk -v b="$b" 'BEGIN {
        mb = b / 1048576
        if (mb >= 10) printf "%d MB", mb + 0.5
        else          printf "%.1f MB", mb
    }'
}

# The build the drop was cut from. include/swos_build.h is what the binaries
# themselves carry (scripts/gen_build_info.sh writes it), so the page and the
# game's own main menu cannot disagree; git is the fallback for a bare checkout.
BUILD=""
DATE=""
if [ -f include/swos_build.h ]; then
    BUILD=$(sed -n 's/.*SWOS_BUILD_ID  *"\([^"]*\)".*/\1/p' include/swos_build.h | head -1)
    DATE=$(sed -n 's/.*SWOS_BUILD_DATE  *"\([^"]*\)".*/\1/p' include/swos_build.h | head -1)
fi
[ -n "$BUILD" ] || BUILD=$(git rev-parse --short HEAD 2>/dev/null || echo unknown)
[ -n "$DATE" ]  || DATE=$(date +%Y-%m-%d)

mkdir -p "$(dirname "$OUT")"
sed -e "s|@BUILD@|$BUILD|g" \
    -e "s|@DATE@|$DATE|g" \
    -e "s|@SIZE_SETUP@|$(size_of SWOS-Setup.exe)|g" \
    -e "s|@SIZE_FULL@|$(size_of SWOS-full.zip)|g" \
    -e "s|@SIZE_MACAPP@|$(size_of SWOS-macOS-app.zip)|g" \
    -e "s|@SIZE_AMIGA@|$(size_of SWOS-Amiga.zip)|g" \
    -e "s|@SIZE_BIN@|$(size_of SWOS.zip)|g" \
    -e "s|@SIZE_MACBIN@|$(size_of SWOS-macOS.zip)|g" \
    "$TEMPLATE" > "$OUT" || { echo "gen_downloads_page: could not write $OUT" >&2; exit 1; }

# Nothing may reach the server with a placeholder still in it. (The template's
# own comment names them, so look only at what is left of an @WORD@ form.)
LEFT=$(grep -oE '@[A-Z_]+@' "$OUT" | sort -u)
if [ -n "$LEFT" ]; then
    echo "gen_downloads_page: unfilled placeholder(s) in $OUT:" >&2
    printf '  %s\n' $LEFT >&2
    exit 1
fi

echo "== $OUT  (build $BUILD, $DATE)"
