#!/bin/sh
# Compila todos os fontes do decomp para EE, gerando .o incrementalmente.
# POSIX sh — roda DENTRO do proot (Alpine ash), cwd em /src/third_party/mm.
cd /src/third_party/mm || exit 1

OBJDIR="/src/third_party/mm/build/ee/obj"
LOG="$OBJDIR/compile.log"
mkdir -p "$OBJDIR"
touch "$LOG"

CC="mips64r5900el-ps2-elf-gcc"
F="-O0 -Iinclude -Iinclude/libc -Isrc -I. -Iextracted/n64-us -Ibuild/n64-us"
B='-DF3DEX_GBI_2 -DF3DEX_GBI_PL -DGBI_DOWHILE -DMM_VERSION=N64_US -DNON_MATCHING -DAVOID_UB'
BC='-DBUILD_CREATOR="mmps2"'
BD='-DBUILD_DATE="20261015"'
BT='-DBUILD_TIME="000000"'

if [ "${1:-}" = "--one" ]; then
  f="$2"
  rel="${f#./}"
  obj="$OBJDIR/${rel%.c}.o"
  if [ -f "$obj" ] && [ "$obj" -nt "$f" ] && [ -s "$obj" ]; then
    echo "SKIP $f" >> "$LOG"
    exit 0
  fi
  mkdir -p "$(dirname "$obj")"
  EXTRA=""
  case "$f" in
    */assets/*) EXTRA="-fno-zero-initialized-in-bss -fno-toplevel-reorder -Wno-error=int-conversion" ;;
  esac
  case "$f" in
    src/audio/*) EXTRA="$EXTRA -DMML_VERSION=MML_VERSION_MM -Iinclude/tables -Iinclude/audio" ;;
  esac
  if $CC -c $F $B $BC $BD $BT $EXTRA -o "$obj" "$f" 2>> "$LOG"; then
    echo "PASS $f" >> "$LOG"
  else
    echo "FAIL $f" >> "$LOG"
  fi
  exit 0
fi

SELF="/src/scripts/compile_ee.sh"
SOURCES="$(find src/code -name '*.c' ! -name 'z_sram_JPN.c' ! -name '*_JPN.c' ! -name '*_NES.c' ! -name '*_EU.c' ! -name '*_EUR.c' ! -name '*_CN.c' ! -name '*_GER.c' ! -name '*_FRA.c' ! -name '*_SP.c'
find src/libultra -name '*.c' ! -name '*_JPN.c' ! -name '*_NES.c' ! -name '*_EU.c' ! -name '*_EUR.c' ! -name '*_CN.c' ! -name '*_GER.c' ! -name '*_FRA.c' ! -name '*_SP.c'
find src/boot -name '*.c' ! -name '*_JPN.c' ! -name '*_NES.c' ! -name '*_EU.c' ! -name '*_EUR.c' ! -name '*_CN.c' ! -name '*_GER.c' ! -name '*_FRA.c' ! -name '*_SP.c'
find src/audio -name '*.c' ! -name '*_JPN.c' ! -name '*_NES.c' ! -name '*_EU.c' ! -name '*_EUR.c' ! -name '*_CN.c' ! -name '*_GER.c' ! -name '*_FRA.c' ! -name '*_SP.c'
find src/buffers -name '*.c' ! -name '*_JPN.c' ! -name '*_NES.c' ! -name '*_EU.c' ! -name '*_EUR.c' ! -name '*_CN.c' ! -name '*_GER.c' ! -name '*_FRA.c' ! -name '*_SP.c'
find src/dmadata -name '*.c' ! -name '*_JPN.c' ! -name '*_NES.c' ! -name '*_EU.c' ! -name '*_EUR.c' ! -name '*_CN.c' ! -name '*_GER.c' ! -name '*_FRA.c' ! -name '*_SP.c'
find src/overlays -name '*.c' ! -name '*_JPN.c' ! -name '*_NES.c' ! -name '*_EU.c' ! -name '*_EUR.c' ! -name '*_CN.c' ! -name '*_GER.c' ! -name '*_FRA.c' ! -name '*_SP.c'
find extracted/n64-us/assets/objects -name '*.c' ! -name '*_JPN.c' ! -name '*_NES.c' ! -name '*_EU.c' ! -name '*_EUR.c' ! -name '*_CN.c' ! -name '*_GER.c' ! -name '*_FRA.c' ! -name '*_SP.c'
find extracted/n64-us/assets/scenes -name '*.c' ! -name '*_JPN.c' ! -name '*_NES.c' ! -name '*_EU.c' ! -name '*_EUR.c' ! -name '*_CN.c' ! -name '*_GER.c' ! -name '*_FRA.c' ! -name '*_SP.c'
find extracted/n64-us/assets/interface -name '*.c' ! -name '*_JPN.c' ! -name '*_NES.c' ! -name '*_EU.c' ! -name '*_EUR.c' ! -name '*_CN.c' ! -name '*_GER.c' ! -name '*_FRA.c' ! -name '*_SP.c'
find extracted/n64-us/assets/misc -name '*.c' ! -name '*_JPN.c' ! -name '*_NES.c' ! -name '*_EU.c' ! -name '*_EUR.c' ! -name '*_CN.c' ! -name '*_GER.c' ! -name '*_FRA.c' ! -name '*_SP.c'
find extracted/n64-us/assets/archives -name '*.c' ! -name '*_JPN.c' ! -name '*_NES.c' ! -name '*_EU.c' ! -name '*_EUR.c' ! -name '*_CN.c' ! -name '*_GER.c' ! -name '*_FRA.c' ! -name '*_SP.c'
find extracted/n64-us/assets/boot -name '*.c' ! -name '*_JPN.c' ! -name '*_NES.c' ! -name '*_EU.c' ! -name '*_EUR.c' ! -name '*_CN.c' ! -name '*_GER.c' ! -name '*_FRA.c' ! -name '*_SP.c'
find assets/text -name '*.c' ! -name '*_JPN.c' ! -name '*_NES.c' ! -name '*_EU.c' ! -name '*_EUR.c' ! -name '*_CN.c' ! -name '*_GER.c' ! -name '*_FRA.c' ! -name '*_SP.c'
find assets/archives -name '*.c' ! -name '*_JPN.c' ! -name '*_NES.c' ! -name '*_EU.c' ! -name '*_EUR.c' ! -name '*_CN.c' ! -name '*_GER.c' ! -name '*_FRA.c' ! -name '*_SP.c')"
TOTAL=$(echo "$SOURCES" | wc -l)

echo "compilando $TOTAL fontes..."
echo "$SOURCES" | tr ' ' '\n' | grep -v '^$' | xargs -P6 -I{} sh "$SELF" --one {}

p=$(grep -c '^PASS' "$LOG")
f=$(grep -c '^FAIL' "$LOG")
echo "RESULT PASS=$p FAIL=$f TOTAL=$TOTAL"
echo "--- falhas ---"
grep '^FAIL' "$LOG"
