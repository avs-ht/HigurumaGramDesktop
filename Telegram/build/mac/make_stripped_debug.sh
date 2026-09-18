#!/usr/bin/env bash
#
# Called as a POST_BUILD step for the Debug config only (see Telegram/CMakeLists.txt).
# The Debug binary carries its full local symbol table, which inflates
# __LINKEDIT past the point where dyld can map the shared cache alongside it
# (dyld then fails to resolve cache-only system frameworks and the app dies
# on launch). This produces a `-stripped` sibling app with local symbols
# removed, which launches normally from Finder; the original Debug app is
# left untouched for attaching a debugger.
set -euo pipefail

bundle="$1"
bundle="${bundle%/}"
name="$(basename "$bundle" .app)"
parent="$(dirname "$bundle")"
stripped="$parent/${name}-stripped.app"

rm -rf "$stripped"
ditto "$bundle" "$stripped"

exe="$stripped/Contents/MacOS/$name"
if [ ! -f "$exe" ]; then
    exe="$(find "$stripped/Contents/MacOS" -maxdepth 1 -type f | head -1)"
fi

strip -x "$exe"
codesign --force --deep -s - "$stripped"

echo "make_stripped_debug: wrote $stripped"
