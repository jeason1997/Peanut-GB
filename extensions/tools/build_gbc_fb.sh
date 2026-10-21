#!/bin/sh
# Optional Linux framebuffer frontend. CC may name a target cross compiler.
set -eu
cd "$(dirname "$0")/../.."
mkdir -p build-extensions/bin
${CC:-cc} ${CFLAGS:--O3} -std=c99 -Wall -Wextra extensions/gbc/frontend_fb.c ${LDFLAGS:-} -o build-extensions/bin/peanut-fb-gbc
