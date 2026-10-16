#!/bin/sh
# Mapeia símbolos definidos vs indefinidos em todos os objetos EE do decomp.
# Roda DENTRO do proot, cwd /src/third_party/mm.
cd /src/third_party/mm || exit 1
NM=mips64r5900el-ps2-elf-nm
OUT=/src/third_party/mm/build/ee
mkdir -p "$OUT"
OBJS=$(find build/ee/obj -name '*.o')

# Definidos (globais)
echo "$OBJS" | tr ' ' '\n' | grep -v '^$' | while read -r o; do
  $NM -g --defined-only "$o" 2>/dev/null
done | awk 'NF>=3 {print $NF}' | sort -u > "$OUT/sym_defined.txt"

# Indefinidos (não resolvidos)
echo "$OBJS" | tr ' ' '\n' | grep -v '^$' | while read -r o; do
  $NM -u "$o" 2>/dev/null
done | awk '{print $NF}' | sort -u > "$OUT/sym_undefined.txt"

# Faltantes = indefinidos - definidos
comm -23 "$OUT/sym_undefined.txt" "$OUT/sym_defined.txt" > "$OUT/sym_missing.txt"

echo "definidos:  $(wc -l < "$OUT/sym_defined.txt")"
echo "indefinidos:$(wc -l < "$OUT/sym_undefined.txt")"
echo "faltantes:  $(wc -l < "$OUT/sym_missing.txt")"

echo "--- amostra faltantes (por prefixo) ---"
sed 's/[0-9].*//' "$OUT/sym_missing.txt" | sort | uniq -c | sort -rn | head -30
