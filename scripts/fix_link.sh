#!/bin/sh
# patch link_full_elf.sh to add seg_addenda.o
sed -i "s|    \[ -f \"\$STUBDIR/asset_stubs.o\" \] && EXTRA=\"\$STUBDIR/asset_stubs.o\"|    [ -f \"\$STUBDIR/asset_stubs.o\" ] && EXTRA=\"\$STUBDIR/asset_stubs.o\"\n    [ -f \"\$STUBDIR/seg_addenda.o\" ] && EXTRA=\"\$EXTRA \$STUBDIR/seg_addenda.o\"|" /src/scripts/link_full_elf.sh 2>/dev/null || true
