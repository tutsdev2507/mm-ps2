#!/bin/sh
NM=mips64r5900el-ps2-elf-nm
OBJDIR=/src/third_party/mm/build/ee/obj
OUT=/src/third_party/mm/build/ee/asset_stubs_sized.c
TMP=/tmp/gen_sized_tmp.txt
rm -f "$TMP"
find "$OBJDIR" -name '*.o' \( -path '*/extracted/*' -o -path '*/assets/*' \) -print0 2>/dev/null | xargs -0 -r $NM -S --defined-only > "$TMP" 2>&1
{
  echo '/* Generated - do not edit */'
  awk '
    NF>=4 {
      name=$4; type=$3; size=$2
      if (type=="t" || type=="T") next
      if (substr(name,1,1)==".") next
      if (name ~ /^(_gp|_fdata|_ftext|_end|_edata|_fbss|_stack)$/) next
      if (name ~ /^__os/) next
      if (seen[name]++) next
      # convert hex size (e.g., 0000000c)
      h = size
      sz = 0
      len = length(h)
      for (i=1; i<=len; i++) {
        c = substr(h,i,1)
        v = index("0123456789abcdefABCDEF", c)-1
        if (v < 0) break
        sz = sz*16 + v
      }
      if (sz < 8) sz = 8
      printf "unsigned char %s[%d] __attribute__((used, aligned(16)));\n", name, sz
    }
  ' "$TMP"
} > "$OUT"
echo "wrote $(wc -l < "$OUT") lines"
