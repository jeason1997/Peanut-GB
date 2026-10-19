#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
work=$(mktemp -d /tmp/gx-tests.XXXXXX)
trap 'rm -rf "$work"' EXIT
cc="${CC:-cc}"
"$cc" -std=c99 -O2 extensions/tests/test_viewport.c -o "$work/viewport"
"$work/viewport"
"$cc" -std=c99 -O2 extensions/tests/test_config.c -o "$work/config"
"$work/config"
"$cc" -std=c99 -O2 -DPGB_SILVER_MAX_OBJECTS=28 -DPGB_SILVER_MAX_MAPS=4 -DPGB_SILVER_BLOCK_CACHE=0 -DPGB_SILVER_LAYER_TAGS=0 extensions/tests/test_config.c -o "$work/small"
"$work/small"
if [ "$#" -eq 2 ]; then
 "$cc" -std=c99 -O2 extensions/tests/test_portable_silver.c -o "$work/silver"
 "$work/silver" "$1" "$2"
 "$cc" -std=c99 -O2 -DPGB_SILVER_MAX_OBJECTS=28 -DPGB_SILVER_MAX_MAPS=4 -DPGB_SILVER_BLOCK_CACHE=0 extensions/tests/test_portable_silver.c -o "$work/silver-small"
 "$work/silver-small" "$1" "$2"
fi
