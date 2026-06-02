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
TLx=0.305; TLy=0.300
TRx=0.715; TRy=0.305
BRx=0.710; BRy=0.615
BLx=0.300; BLy=0.600

IM=convert; command -v convert >/dev/null 2>&1 || IM=magick
# Use $(...) not `read < <(...)`: convert's info: output has no trailing newline,
# which makes `read` return non-zero and `set -e` abort the script silently.
W=$($IM "$CASE"  -format '%w' info:);  H=$($IM "$CASE"  -format '%h' info:)
LW=$($IM "$LIVE" -format '%w' info:); LH=$($IM "$LIVE" -format '%h' info:)
f2p(){ awk "BEGIN{printf \"%d\", ($1)*($2)+0.5}"; }
tlx=$(f2p "$TLx" "$W"); tly=$(f2p "$TLy" "$H")
trx=$(f2p "$TRx" "$W"); try=$(f2p "$TRy" "$H")
brx=$(f2p "$BRx" "$W"); bry=$(f2p "$BRy" "$H")
blx=$(f2p "$BLx" "$W"); bly=$(f2p "$BLy" "$H")
echo "case ${W}x${H} | live ${LW}x${LH} | quad TL($tlx,$tly) TR($trx,$try) BR($brx,$bry) BL($blx,$bly)"

# Warp the live screenshot onto a transparent canvas the size of the photo,
# mapping its four corners to the screen quad, then composite it over the photo.
$IM "$LIVE" -alpha set -virtual-pixel transparent -extent "${W}x${H}" \
  -distort Perspective "0,0 ${tlx},${tly} ${LW},0 ${trx},${try} ${LW},${LH} ${brx},${bry} 0,${LH} ${blx},${bly}" \
  miff:- | $IM "$CASE" miff:- -composite "$OUT"
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
