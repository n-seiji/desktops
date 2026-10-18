#!/bin/sh
# Zips dist/just-desktops.app into dist/just-desktops-<version>.app.zip plus a .sha256 file.
set -eu
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root/dist"
version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' just-desktops.app/Contents/Info.plist)"
zip="just-desktops-$version.app.zip"
rm -f "$zip" "$zip.sha256"
ditto -c -k --keepParent just-desktops.app "$zip"
shasum -a 256 "$zip" > "$zip.sha256"
echo "packaged dist/$zip"
