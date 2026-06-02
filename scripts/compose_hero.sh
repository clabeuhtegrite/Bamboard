#!/usr/bin/env bash
# Composite Bamboard's live UI screenshot onto TomE1337's case photo, warped to
# the screen's perspective, to produce the project hero render.
#
#   compose_hero.sh <case_photo> <live_png> <out_png>
#
# docs/screenshots/case.jpg is TomE1337's product image (CC BY-NC-SA 4.0); the
# resulting hero is a derivative under the same license — keep the attribution.
#
# The screen quad is expressed as fractions of the photo (TL, TR, BR, BL), so it
# is resolution-independent; re-tune the eight numbers below if the photo is
# replaced. Set HERO_DEBUG=1 to also emit <out>_debug.png with the quad drawn,
# which is how the corners were calibrated.
set -euo pipefail

CASE="${1:?case photo path}"; LIVE="${2:?live png path}"; OUT="${3:?output path}"

# --- screen quad, fractions of the photo (x y in 0..1), clockwise from TL -----
# Calibrated to the screen on the 1280x960 case photo: right edge fixed, the two
# left corners nudged up, and the bottom-left also nudged left (slight parallax).
TLx=0.295; TLy=0.232
TRx=0.751; TRy=0.243
BRx=0.751; BRy=0.619
BLx=0.288; BLy=0.608

IM=convert; command -v convert >/dev/null 2>&1 || IM=magick
# Use $(...) not `read < <(...)`: convert's info: output has no trailing newline,
# which makes `read` return non-zero and `set -e` abort the script silently.
W=$($IM "$CASE"  -format '%w' info:);  H=$($IM "$CASE"  -format '%h' info:)

# The panel is natively 480x272, which looks soft when blown up into the hero.
# Sharpen a touch, then Lanczos-upscale the screenshot before warping, so the
# screen reads as crisp as the source allows instead of a mushy blow-up. (No
# extra detail is invented; it just avoids a soft bilinear enlargement.)
HDLIVE="${LIVE%.*}_hd.png"
$IM "$LIVE" -unsharp 0x0.6+0.8+0 -filter Lanczos -resize 300% "$HDLIVE"
LIVE="$HDLIVE"
LW=$($IM "$LIVE" -format '%w' info:); LH=$($IM "$LIVE" -format '%h' info:)
f2p(){ awk "BEGIN{printf \"%d\", ($1)*($2)+0.5}"; }
tlx=$(f2p "$TLx" "$W"); tly=$(f2p "$TLy" "$H")
trx=$(f2p "$TRx" "$W"); try=$(f2p "$TRy" "$H")
brx=$(f2p "$BRx" "$W"); bry=$(f2p "$BRy" "$H")
blx=$(f2p "$BLx" "$W"); bly=$(f2p "$BLy" "$H")
echo "case ${W}x${H} | live ${LW}x${LH} | quad TL($tlx,$tly) TR($trx,$try) BR($brx,$bry) BL($blx,$bly)"

# Warp the live screenshot onto a transparent canvas the size of the photo,
# mapping its four corners to the screen quad, then composite it over the photo.
# Warp the (upscaled) screenshot onto the screen quad with +distort, which sizes
# and offsets the layer itself, then flatten it onto the photo. Using -extent to
# the photo size would crop the layer whenever the upscaled screenshot is wider
# than the photo (that crop let the original screen show through = "two photos").
$IM "$CASE" \
  \( "$LIVE" -alpha set -virtual-pixel none \
     +distort Perspective "0,0 ${tlx},${tly} ${LW},0 ${trx},${try} ${LW},${LH} ${brx},${bry} 0,${LH} ${blx},${bly}" \) \
  -flatten "$OUT"
echo "wrote $OUT"

if [ "${HERO_DEBUG:-0}" = "1" ]; then
  $IM "$OUT" -fill none -stroke red -strokewidth 3 \
    -draw "polyline ${tlx},${tly} ${trx},${try} ${brx},${bry} ${blx},${bly} ${tlx},${tly}" \
    -stroke none -fill yellow \
    -draw "circle ${tlx},${tly} $((tlx+9)),${tly}" \
    -draw "circle ${trx},${try} $((trx+9)),${try}" \
    -draw "circle ${brx},${bry} $((brx+9)),${bry}" \
    -draw "circle ${blx},${bly} $((blx+9)),${bly}" \
    "${OUT%.png}_debug.png"
  echo "wrote ${OUT%.png}_debug.png"
fi
