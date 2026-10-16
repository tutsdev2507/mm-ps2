#!/bin/sh
NM=mips64r5900el-ps2-elf-nm
OBJDIR=/src/third_party/mm/build/ee/obj
OUT=/src/third_party/mm/build/ee/asset_stubs_sized.c
TMP=/tmp/gen_sized_tmp.txt
rm -f "$TMP"
find "$OBJDIR" -name '*.o' \( -path '*/extracted/*' -o -path '*/assets/*' \) -print0 2>/dev/null | xargs -0 -r $NM -S --defined-only 2>"$TMP.err" > "$TMP"
{
  echo '/* Generated - do not edit */'
  awk '
    NF>=4 { name=$4; type=$3; size=$2; if(type=="t"||type=="T") next; if(name~/^\./) next; if(name~/^(_gp|_fdata|_ftext|_end|_edata|_fbss|_stack)$/) next; if(name~/^__os/) next; if(seen[name]++) next; sz=strtonum("0x"size); if(sz<8) sz=8; printf "unsigned char %s[%d] __attribute__((used, aligned(16)));\n", name, sz }
  ' "$TMP"
} > "$OUT"
rm -f "$TMP" "$TMP.err"
echo "wrote $(wc -l < "$OUT") lines"
