#!/usr/bin/env bash
# Generate build-time asset .inc.c files for the EE build, mirroring the
# rules in third_party/mm/Makefile:
#   $(BUILD_DIR)/assets/%.inc.c      : $(EXTRACTED_DIR)/assets/%.png   (ZAPD btex)
#   $(BUILD_DIR)/assets/%.bin.inc.c  : $(EXTRACTED_DIR)/assets/%.bin   (ZAPD bblb)
#   $(BUILD_DIR)/assets/%.jpg.inc.c  : $(EXTRACTED_DIR)/assets/%.jpg   (ZAPD bren)
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
MM="$ROOT/third_party/mm"
ZAPD="${ZAPD:-$HOME/tmp/zapd/ZAPD.out}"
EXTRACTED="$MM/extracted/n64-us"
BUILD="$MM/build/n64-us"
EXTRACTED_ASSETS="$EXTRACTED/assets"
COMMITTED_ASSETS="$MM/assets"

gen_one() {
  src="$1"
  case "$src" in
    "$EXTRACTED_ASSETS"/*) rel="${src#"$EXTRACTED_ASSETS"/}" ;;
    "$COMMITTED_ASSETS"/*) rel="${src#"$COMMITTED_ASSETS"/}" ;;
    *) return 0 ;;
  esac
  out="$BUILD/assets/$rel"
  case "$src" in
    *.png)
      out="${out%.png}.inc.c"
      stem="${rel%.png}"; ext="${stem##*.}"
      [ "$ext" = "$stem" ] && ext=""
      mkdir -p "$(dirname "$out")"
      "$ZAPD" btex -eh -tt "$ext" -i "$src" -o "$out" >/dev/null 2>&1 ;;
    *.bin)
      out="${out%.bin}.bin.inc.c"
      mkdir -p "$(dirname "$out")"
      "$ZAPD" bblb -eh -i "$src" -o "$out" >/dev/null 2>&1 ;;
    *.jpg)
      out="${out%.jpg}.jpg.inc.c"
      mkdir -p "$(dirname "$out")"
      "$ZAPD" bren -eh -i "$src" -o "$out" >/dev/null 2>&1 ;;
    *) return 0 ;;
  esac
}

if [ "${1:-}" = "--one" ]; then
  shift
  gen_one "$1"
  exit $?
fi

export ZAPD EXTRACTED_ASSETS COMMITTED_ASSETS BUILD
[ -x "$ZAPD" ] || { echo "ZAPD not executable: $ZAPD" >&2; exit 1; }

find "$EXTRACTED_ASSETS" "$COMMITTED_ASSETS" -type f \( -name '*.png' -o -name '*.bin' -o -name '*.jpg' \) -print0 \
  | grep -zv '/xml/' | grep -zv '/audio/' \
  | xargs -0 -P4 -n1 bash "$0" --one

echo "gen_inc: generated $(find "$BUILD/assets" -name '*.inc.c' 2>/dev/null | wc -l) inc.c files"
