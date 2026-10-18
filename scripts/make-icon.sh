#!/bin/sh
# Regenerates assets/AppIcon.icns from assets/logo-dark.svg using headless Chrome.
# CI does not run this; the generated .icns is committed.
set -eu
root="$(cd "$(dirname "$0")/.." && pwd)"
chrome="${CHROME:-/Applications/Google Chrome.app/Contents/MacOS/Google Chrome}"
[ -x "$chrome" ] || { echo "set CHROME to a Chromium-based browser binary" >&2; exit 1; }

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cp "$root/assets/logo-dark.svg" "$work/logo.svg"
# macOS icons keep the tile inside an 824/1024 square with transparent margin.
cat > "$work/icon.html" <<'HTML'
<!doctype html><meta charset="utf-8">
<style>html,body{margin:0;background:transparent}
img{position:absolute;left:100px;top:100px;width:824px;height:824px}</style>
<img src="logo.svg">
HTML
"$chrome" --headless=new --disable-gpu --hide-scrollbars \
  --default-background-color=00000000 --window-size=1024,1024 \
  --screenshot="$work/base.png" "file://$work/icon.html" >/dev/null 2>&1

set="$work/AppIcon.iconset"
mkdir "$set"
for s in 16 32 128 256 512; do
  sips -z "$s" "$s" "$work/base.png" --out "$set/icon_${s}x${s}.png" >/dev/null
  d=$((s * 2))
  sips -z "$d" "$d" "$work/base.png" --out "$set/icon_${s}x${s}@2x.png" >/dev/null
done
iconutil -c icns "$set" -o "$root/assets/AppIcon.icns"
echo "wrote assets/AppIcon.icns"
