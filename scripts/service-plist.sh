#!/bin/bash
# Usage: scripts/service-plist.sh <binary> <log file>
# Prints the LaunchAgent plist with @BIN@ and @LOG@ filled in. Values are XML-escaped and
# substituted with parameter expansion, so no character in a path can change the template.
set -eu
[ $# -eq 2 ] || { echo "usage: service-plist.sh <binary> <log file>" >&2; exit 1; }
root="$(cd "$(dirname "$0")/.." && pwd)"

xml_escape() {
  local s="$1"
  s="${s//&/&amp;}"
  s="${s//</&lt;}"
  s="${s//>/&gt;}"
  printf '%s' "$s"
}

bin="$(xml_escape "$1")"
log="$(xml_escape "$2")"
tpl="$(cat "$root/launchd/com.just-desktops.plist")"
tpl="${tpl//@BIN@/$bin}"
tpl="${tpl//@LOG@/$log}"
printf '%s\n' "$tpl"
