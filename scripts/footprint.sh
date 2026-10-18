#!/usr/bin/env bash
# Idle memory of a build: scripts/footprint.sh [config-file] [seconds]
# Runs bin/just-desktops under a temporary HOME with the given config, prints its footprint in bytes.
set -eu
root="$(cd "$(dirname "$0")/.." && pwd)"
bin="$root/bin/just-desktops"
[ -x "$bin" ] || { echo "build first: mise run build" >&2; exit 1; }
cfg="${1:-}"
secs="${2:-10}"
home="$(mktemp -d)"
mkdir -p "$home/.config/just-desktops"
[ -z "$cfg" ] || cp "$cfg" "$home/.config/just-desktops/config"
HOME="$home" "$bin" 2>/dev/null &
pid=$!
trap 'kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; rm -rf "$home"' EXIT
sleep "$secs"
kill -0 "$pid" 2>/dev/null || { echo "just-desktops exited early" >&2; exit 1; }
footprint --format bytes "$pid"
