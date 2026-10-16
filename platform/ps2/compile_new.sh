#!/bin/sh
# Compile the PS2 platform shim into build/ee/obj_ps2.
#
# Runs INSIDE the proot rootfs (see scripts/build_termux.sh), cwd /src.
#   sh /src/platform/ps2/compile_new.sh [file.c ...]
#
# With no arguments every shim source is (re)built.
#
# Two include worlds have to coexist:
#   * decomp shims (platform/ps2/*) need the game's libultra headers so the
#     OSThread/OSMesgQueue/OSTask layouts match.  They compile with
#     -DMMPS2_NATIVE_TYPES so include/PR/ultratypes.h spells s32/u32 as
#     `int`, which is what ps2sdk's <tamtypes.h> uses (both are 32-bit on
#     the EE, so this only has to be consistent *inside* a translation unit).
#   * port code (port/src/**) is plain C against newlib + gsKit.  It must
#     NOT see third_party/mm/include or include/libc, otherwise the decomp's
#     stdarg.h/math.h shadow newlib's and <stdio.h> dies with
#     "unknown type name '__gnuc_va_list'".
cd /src/third_party/mm || exit 1

OBJDIR=/src/third_party/mm/build/ee/obj_ps2
mkdir -p "$OBJDIR"

CC=mips64r5900el-ps2-elf-gcc
PS2SDK_INC="-I/usr/local/ps2dev/ps2sdk/common/include -I/usr/local/ps2dev/ps2sdk/ee/include"
GSKIT_INC="-I/usr/local/ps2dev/gsKit/include"
PORT_INC="-I../../platform/ps2 -I../../include -I../../include/audio_stubs -I../../port/src"

# decomp-context shims
F_DECOMP="-O2 -g0 $GSKIT_INC $PS2SDK_INC $PORT_INC -Iinclude -Iinclude/libc -Isrc -I. -Iextracted/n64-us -Ibuild/n64-us -Ibuild/gen -I/src/include/tables"
B_DECOMP="-DF3DEX_GBI_2 -DF3DEX_GBI_PL -DGBI_DOWHILE -DMM_VERSION=N64_US -DNON_MATCHING -DAVOID_UB -D_EE -DMMPS2_NATIVE_TYPES -G0 -DFRAMEBUFFERS_START_ADDR=0x81F80000"

# port-context sources (newlib/libm headers must win)
F_PORT="-O2 -g0 $GSKIT_INC $PS2SDK_INC $PORT_INC -I/src/include/tables"
B_PORT="-DF3DEX_GBI_2 -DF3DEX_GBI_PL -DGBI_DOWHILE -DMM_VERSION=N64_US -DNON_MATCHING -DAVOID_UB -D_EE -DMMPS2_NATIVE_TYPES -G0"

if [ $# -gt 0 ]; then
    SRC="$*"
else
    SRC="/src/platform/ps2/mmps2_os.c /src/platform/ps2/hw_shim.c \
         /src/platform/ps2/runtime.c \
         /src/platform/ps2/cache_tlb.c /src/platform/ps2/vi_gs.c \
         /src/platform/ps2/pi_dma.c /src/platform/ps2/rsp_shim.c \
         /src/platform/ps2/segloader.c /src/platform/ps2/crt0_mmps2.c /src/platform/ps2/audio_stubs.c \
         /src/platform/ps2/dmadata_ps2.c /src/port/src/gfx/gfx.c /src/port/src/ps2/rom_io.c \
         /src/port/src/ps2/dbg_scr.c"
fi

rc=0
for f in $SRC; do
    base=$(basename "$f" .c)
    out="$OBJDIR/$base.o"
    case "$f" in
        /src/port/*) FL="$F_PORT";  BL="$B_PORT" ;;
        *)           FL="$F_DECOMP"; BL="$B_DECOMP" ;;
    esac
    if log=$($CC -c $FL $BL -o "$out" "$f" 2>&1); then
        echo "PASS $base"
    else
        echo "$log" | grep -v linkerconfig
        echo "FAIL $base"
        rc=1
    fi
done
exit $rc
