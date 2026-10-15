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

# Integration test against the real extracted object_ah asset. It needs 32-bit
# pointers, so it is only built where -m32 links+runs (CI, x86-64 multilib).
if "$CC" -m32 -x c - -o "$out/probe" >/dev/null 2>&1 <<<'int main(void){return 0;}'; then
    "$CC" -m32 -std=gnu89 -O1 -g -w \
        -I "$here/stub" -I "$src" -I "$src/gfx" -I "$src/../assets/object_ah" \
        "$here/test_object_ah.c" "$src/gfx/gfx.c" \
        "$src/../assets/object_ah/object_ah.c" -lm -o "$out/test_object_ah"
    if [ -n "${MMPS2_GDB:-}" ] && command -v gdb >/dev/null 2>&1; then
        gdb -batch -ex run -ex bt --args "$out/test_object_ah"
    else
        "$out/test_object_ah"
    fi
else
    echo "-- skipping object_ah integration test (-m32 unavailable)"
fi
