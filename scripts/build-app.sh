#!/bin/sh
# Builds dist/just-desktops.app (universal, arm64 + x86_64).
# Env: CFLAGS and FRAMEWORKS come from mise.toml. CODESIGN_IDENTITY defaults to "-"
# (ad-hoc); any other value signs with the hardened runtime and a secure timestamp.
set -eu
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

version="$(sed -n 's/^#define JD_VERSION "\(.*\)"$/\1/p' src/jd.h)"
[ -n "$version" ] || { echo "JD_VERSION not found in src/jd.h" >&2; exit 1; }

app=dist/just-desktops.app
rm -rf "$app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"

# shellcheck disable=SC2086
clang $CFLAGS -arch arm64 -arch x86_64 src/*.m src/*.c \
  -o "$app/Contents/MacOS/just-desktops" $FRAMEWORKS
cp assets/AppIcon.icns "$app/Contents/Resources/AppIcon.icns"

cat > "$app/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleIdentifier</key>
    <string>dev.n-seiji.just-desktops</string>
    <key>CFBundleName</key>
    <string>just-desktops</string>
    <key>CFBundleDisplayName</key>
    <string>just-desktops</string>
    <key>CFBundleExecutable</key>
    <string>just-desktops</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleShortVersionString</key>
    <string>$version</string>
    <key>CFBundleVersion</key>
    <string>$version</string>
    <key>LSMinimumSystemVersion</key>
    <string>13.0</string>
    <key>LSUIElement</key>
    <true/>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSHumanReadableCopyright</key>
    <string>Copyright (c) 2026 n-seiji. GPL-3.0.</string>
</dict>
</plist>
PLIST
plutil -lint "$app/Contents/Info.plist" >/dev/null

identity="${CODESIGN_IDENTITY:--}"
if [ "$identity" = "-" ]; then
  codesign --force --deep -s - "$app"
else
  codesign --force --deep --options runtime --timestamp -s "$identity" "$app"
fi
codesign --verify --deep --strict "$app"
echo "built $app ($version)"
