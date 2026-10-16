#!/bin/sh
# Link the full ported ELF from the EE object set.
#
# Runs INSIDE the proot rootfs (scripts/build_termux.sh style), cwd /src.
#
#   sh /src/scripts/link_full_elf.sh [OBJDIR] [OUT] [--code-only]
#
#   OBJDIR   object tree to link (default: /src/third_party/mm/build/ee/obj_o2)
#   OUT      output ELF   (default: /src/third_party/mm/build/ee/MMPS2.ELF)
#   --code-only
#            skip the extracted asset objects (extracted/**).  Their symbols
#            are then satisfied by scripts/gen_asset_stubs.py output, which
#            proves the *code* image fits the EE's 32 MiB; the assets are
#            loaded at runtime by platform/ps2/segloader.c.
set -e

OBJDIR="${1:-/src/third_party/mm/build/ee/obj_o2}"
OUT="${2:-/src/third_party/mm/build/ee/MMPS2.ELF}"

MODE=full
for a in "$@"; do
    [ "$a" = "--code-only" ] && MODE=code-only
done

export PATH=/usr/local/ps2dev/ee/bin:/usr/local/ps2dev/bin:/usr/bin:/bin
export PS2DEV=/usr/local/ps2dev
export PS2SDK=$PS2DEV/ps2sdk
CC=mips64r5900el-ps2-elf-gcc
LINKFILE=/src/platform/ps2/linkfile

STUBDIR=/src/third_party/mm/build/ee
PS2OBJ=/src/third_party/mm/build/ee/obj_ps2

# Two object sets collide on purpose and are resolved by first-definition-wins:
#   * obj_ps2/*.o  (platform shims, plus decomp files recompiled with port
#                   flags, e.g. src/code/main.c -DFRAMEBUFFERS_START_ADDR=...)
#   * obj_o2/**    (the game)
# so the platform objects must be listed first and the linker must not abort
# on the duplicate libultra/DMA symbols they override.
#
# `*.inc.o` and src/code/z_cheap_proc.o are excluded because those sources are
# #included by another TU (z_camera.c -> z_camera_data.inc.c, z_actor.c ->
# z_cheap_proc.c); compiling them standalone as well duplicates every symbol
# they define.
if [ "$MODE" = "code-only" ]; then
    OBJS=$(find "$OBJDIR" -name '*.o' ! -path '*/extracted/*' \
             ! -name '*.inc.o' ! -name 'z_cheap_proc.o')
else
    OBJS=$(find "$OBJDIR" -name '*.o' \
             ! -name '*.inc.o' ! -name 'z_cheap_proc.o')
fi

PLATFORM=$(find "$PS2OBJ" -name '*.o' | sort)

# Asset stubs satisfy the symbols the (excluded) asset objects would provide.
# Prefer the comprehensive set (asset_stubs_all.o); fall back to asset_stubs.o.
EXTRA=""
if [ -f "$STUBDIR/asset_stubs_all.o" ]; then
    EXTRA="$STUBDIR/asset_stubs_all.o"
elif [ -f "$STUBDIR/asset_stubs.o" ]; then
    EXTRA="$STUBDIR/asset_stubs.o"
fi
[ -f "$STUBDIR/seg_addenda.o" ] && EXTRA="$EXTRA $STUBDIR/seg_addenda.o"

echo "[link] objects: $(echo $OBJS | wc -w) + platform + stubs  ($MODE)"
# shellcheck disable=SC2086
$CC -T"$LINKFILE" -o "$OUT" \
    $PLATFORM $EXTRA \
    $OBJS \
    "$STUBDIR/ee_main.o" "$STUBDIR/seg_stubs.o" "$STUBDIR/ps2_stubs.o" \
    -Wl,--allow-multiple-definition \
    -Wl,--start-group -L$PS2SDK/ee/lib -L/usr/local/ps2dev/gsKit/lib \
    -lkernel -lc -ldebug -lm -lpad -ldma -lfileXio -lgskit -ldmakit -Wl,--end-group \
    -Wl,-Map="$OUT.map"

echo "[link] wrote $OUT"
mips64r5900el-ps2-elf-size "$OUT" || true
echo "[link] undefined: $(mips64r5900el-ps2-elf-nm -u "$OUT" | wc -l)"
