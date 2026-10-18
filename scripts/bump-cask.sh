#!/bin/sh
# Usage: scripts/bump-cask.sh <version> [path/to/Casks/just-desktops.rb]
# Downloads the release zip for v<version>, then rewrites version and sha256 in the cask.
# Default cask path is the template in this repo; pass the tap's file to update it.
set -eu
version="${1:?usage: bump-cask.sh <version> [cask.rb]}"
root="$(cd "$(dirname "$0")/.." && pwd)"
cask="${2:-$root/packaging/homebrew/just-desktops.rb}"
repo="${REPO:-n-seiji/just-desktops}"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
zip="just-desktops-$version.app.zip"
gh release download "v$version" --repo "$repo" --pattern "$zip" --dir "$tmp"
sum="$(shasum -a 256 "$tmp/$zip" | cut -d' ' -f1)"

sed -i '' -e "s/^  version \".*\"/  version \"$version\"/" \
          -e "s/^  sha256 \".*\"/  sha256 \"$sum\"/" "$cask"
echo "$cask: version $version sha256 $sum"
