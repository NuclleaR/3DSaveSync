#!/usr/bin/env bash
# Draws the HOME Menu icon (48×48) and banner (256×128) with ImageMagick:
# a white cloud with an upload arrow, in the app's colors.
#
#   bash tools/make-art.sh [output dir]    (default: resources)
set -euo pipefail

out=${1:-resources}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

accent_light='#3b8bff'
accent_dark='#0a3a8c'

# The cloud with its arrow on a transparent 192×192 canvas (drawn 4× larger
# than the icon, then scaled down for smooth edges).
magick -size 192x192 xc:none \
    -fill white \
    -draw "circle 64,108 64,78" \
    -draw "circle 98,88 98,50" \
    -draw "circle 132,112 132,86" \
    -draw "roundrectangle 34,104 158,138 17,17" \
    -fill "$accent_dark" \
    -draw "polygon 72,108 96,80 120,108" \
    -draw "rectangle 89,106 103,128" \
    "$work/cloud.png"

# Icon: the cloud on a blue gradient.
magick -size 192x192 "gradient:$accent_light-$accent_dark" \
    "$work/cloud.png" -composite \
    -resize 48x48 "$out/icon.png"

# Banner: dark background, the cloud on the left, the name on the right.
magick -size 256x128 "gradient:#1c2128-#101418" \
    \( -size 192x192 "gradient:$accent_light-$accent_dark" "$work/cloud.png" -composite -resize 72x72 \) \
    -geometry +14+28 -composite \
    -font DejaVu-Sans-Bold -pointsize 21 -fill white -annotate +96+60 "3DSaveSync" \
    -font DejaVu-Sans -pointsize 11 -fill '#8b949e' -annotate +97+82 "Save backups in Koofr" \
    "$out/banner.png"
