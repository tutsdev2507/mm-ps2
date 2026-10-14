#!/usr/bin/env bash
#
# Builds and runs the host-side interpreter tests with a native compiler.
# These exercise the F3DEX2 display-list -> gsKit translation on the dev
# machine (no EE toolchain required); the authoritative build is still CI.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
src="$here/../src"
CC="${CC:-cc}"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

"$CC" -std=c11 -Wall -Wextra -O1 \
    -I "$here/stub" -I "$src" -I "$src/gfx" \
    "$here/test_gfx.c" "$src/gfx/gfx.c" -lm -o "$out/test_gfx"

"$out/test_gfx"
