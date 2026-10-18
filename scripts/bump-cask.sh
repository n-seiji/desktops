#!/bin/bash
# Usage: scripts/bump-cask.sh <version> [path/to/Casks/just-desktops.rb]
# Downloads the release zip and its published .sha256 for v<version>, verifies the zip against it
# (and the build attestation when `gh attestation` is available), then rewrites version and sha256
# in the cask. Default cask path is the template in this repo; pass the tap's file to update it.
set -eu
version="${1:?usage: bump-cask.sh <version> [cask.rb]}"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "bump-cask: version must look like 1.2.3" >&2; exit 1; }
root="$(cd "$(dirname "$0")/.." && pwd)"
cask="${2:-$root/packaging/homebrew/just-desktops.rb}"
repo="${REPO:-n-seiji/just-desktops}"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
zip="just-desktops-$version.app.zip"
gh release download "v$version" --repo "$repo" --pattern "$zip" --pattern "$zip.sha256" --dir "$tmp"

sum="$(shasum -a 256 "$tmp/$zip" | cut -d' ' -f1)"
published="$(cut -d' ' -f1 "$tmp/$zip.sha256")"
[ "$sum" = "$published" ] || { echo "bump-cask: sha256 of $zip ($sum) does not match the published $published" >&2; exit 1; }

if gh attestation verify --help >/dev/null 2>&1; then
  gh attestation verify "$tmp/$zip" --repo "$repo" >/dev/null \
    || { echo "bump-cask: attestation verification failed for $zip" >&2; exit 1; }
  echo "attestation verified"
else
  echo "note: this gh has no 'attestation' command, so the build attestation was not checked" >&2
fi

[ -f "$cask" ] || { echo "bump-cask: $cask not found" >&2; exit 1; }
out="$tmp/cask.rb"
while IFS= read -r line || [ -n "$line" ]; do
  case "$line" in
    '  version "'*) line="  version \"$version\"" ;;
    '  sha256 "'*) line="  sha256 \"$sum\"" ;;
  esac
  printf '%s\n' "$line"
done < "$cask" > "$out"
cat "$out" > "$cask"
echo "$cask: version $version sha256 $sum"
