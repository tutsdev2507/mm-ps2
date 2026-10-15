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

# Decomp-integration test: link the decomp's real gu math (float[4][4]) through
# the port adapter. Needs the decomp source, fetched by scripts/setup.sh.
mm="$src/../../third_party/mm"
if [ -f "$mm/include/ultra64.h" ]; then
    dec_cflags=(-O1 -w -D_LANGUAGE_C -DNON_MATCHING
                -I "$mm/include" -I "$mm/include/libc" -I "$mm")
    "$CC" -c "${dec_cflags[@]}" "$mm/src/libultra/gu/perspective.c" -o "$out/perspective.o"
    "$CC" -c "${dec_cflags[@]}" "$mm/src/libultra/gu/lookat.c" -o "$out/lookat.o"
    "$CC" -c "${dec_cflags[@]}" "$src/ultra/gu_compat.c" -o "$out/gu_compat.o"
    "$CC" -std=c11 -Wall -Wextra -O1 \
        -I "$here/stub" -I "$src" -I "$src/gfx" \
        "$here/test_gu.c" "$src/ultra/gu_adapter.c" \
        "$out/perspective.o" "$out/lookat.o" "$out/gu_compat.o" \
        -lm -o "$out/test_gu"
    "$out/test_gu"
else
    echo "-- skipping gu integration test (third_party/mm missing; run scripts/setup.sh)"
fi

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
