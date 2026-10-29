#!/bin/bash
# Renders the application icons from gfx/fatrat_logo.svg, the single source for all of them:
#
#   gfx/icons/hicolor/scalable/apps/fatrat.svg   the logo centred on a square canvas
#   gfx/icons/hicolor/NxN/apps/fatrat.png        installed into share/icons
#   gfx/fatrat.png                               window and tray icon compiled into the binary
#
# Rerun after changing the logo and commit the results. Needs rsvg-convert (librsvg).
set -euo pipefail

cd "$(dirname "$0")"
SRC=fatrat_logo.svg
OUT=icons/hicolor
SIZES="16 22 24 32 48 64 128 256 512"

# Widen the portrait viewBox into a square one with the logo in the middle
read -r W H < <(sed -n 's/^<svg [^>]*viewBox="0 0 \([0-9.]*\) \([0-9.]*\)".*/\1 \2/p' "$SRC")
if [ -z "${W:-}" ]; then
	echo "Cannot read the viewBox of $SRC" >&2
	exit 1
fi
SIDE=$(awk -v w="$W" -v h="$H" 'BEGIN { print (w > h) ? w : h }')
X=$(awk -v w="$W" -v s="$SIDE" 'BEGIN { print (w - s) / 2 }')
Y=$(awk -v h="$H" -v s="$SIDE" 'BEGIN { print (h - s) / 2 }')

rm -rf icons
mkdir -p "$OUT/scalable/apps"
sed "s/^<svg \([^>]*\)viewBox=\"[^\"]*\" width=\"[^\"]*\" height=\"[^\"]*\"/<svg \1viewBox=\"$X $Y $SIDE $SIDE\" width=\"$SIDE\" height=\"$SIDE\"/" \
	"$SRC" > "$OUT/scalable/apps/fatrat.svg"
grep -q "viewBox=\"$X $Y $SIDE $SIDE\"" "$OUT/scalable/apps/fatrat.svg"

for size in $SIZES; do
	mkdir -p "$OUT/${size}x${size}/apps"
	rsvg-convert -w "$size" -h "$size" "$OUT/scalable/apps/fatrat.svg" -o "$OUT/${size}x${size}/apps/fatrat.png"
done

rsvg-convert -w 128 -h 128 "$OUT/scalable/apps/fatrat.svg" -o fatrat.png
