#!/usr/bin/env bash
# Build the FULL game EE ELF (code image + asset stubs + PS2 shims).
#
# This is the pipeline that produced third_party/mm/build/ee/MMPS2.ELF
# (~11 MB, 0 undefined symbols).  It runs the heavy lifting INSIDE the
# proot/qemu rootfs used by the rest of the Termux build.
#
# Prerequisites:
#   scripts/setup.sh                 # decomp source at pinned rev
#   scripts/extract_assets.sh ROM    # ZAPD assets from YOUR USA 1.0 ROM
#   scripts/build_termux.sh          # ps2dev rootfs + gsKit (once)
#
# Usage:
#   scripts/build_full_game.sh [--clean]
#
#   --clean   wipe build/ee/obj_o2 first (do this after changing compile flags)
#
# Output: third_party/mm/build/ee/MMPS2.ELF
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MM="$ROOT/third_party/mm"

if [ "${1:-}" = "--clean" ]; then
    echo "[clean] rm -rf $MM/build/ee/obj_o2"
    rm -rf "$MM/build/ee/obj_o2"
fi

# 0. sanity: decomp present
[ -d "$MM/src/code" ] || { echo "decomp ausente: rode scripts/setup.sh" >&2; exit 1; }
[ -d "$MM/extracted/n64-us" ] || { echo "assets nao extraidos: rode scripts/extract_assets.sh ROM" >&2; exit 1; }

# Everything below runs inside the ps2dev rootfs mounted at /src.
run() { "$ROOT/scripts/run_proot.sh" sh -c "$1"; }

echo "[1/4] gerar header do dmadata spec"
run 'python3 /src/scripts/gen_dmadata_spec.py || true'

echo "[2/4] compilar objetos do decomp (-O2) para EE"
run 'sh /src/scripts/compile_ee_o2.sh'

echo "[3/4] gerar stubs de assets + addenda de segmentos"
#  asset_stubs_all.c: todos os simbolos de asset (extracted/** e assets/**)
#  seg_addenda.c   : simbolos *SegmentRomStart/End faltantes
run 'sh /src/scripts/gen_asset_stubs_all.sh /src/third_party/mm/build/ee/obj /src/third_party/mm/build/ee/asset_stubs_all.c'
run 'mips64r5900el-ps2-elf-gcc -c -O2 -o /src/third_party/mm/build/ee/asset_stubs_all.o /src/third_party/mm/build/ee/asset_stubs_all.c'

echo "[4/4] linkar ELF code-only"
run 'sh /src/scripts/link_full_elf.sh /src/third_party/mm/build/ee/obj_o2 /src/third_party/mm/build/ee/MMPS2.ELF --code-only'

ELF="$MM/build/ee/MMPS2.ELF"
echo
echo "ELF: $ELF"
ls -lh "$ELF" 2>/dev/null || true
echo "undefined: $(/usr/bin/true; echo see link log)"
