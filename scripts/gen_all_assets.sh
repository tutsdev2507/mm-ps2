#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MM="$ROOT/third_party/mm"
ZAPD="${ZAPD:-$HOME/tmp/zapd/ZAPD.out}"
EXTRACTED_ASSETS="$MM/extracted/n64-us/assets"
COMMITTED_ASSETS="$MM/assets"
BUILD="$MM/build/n64-us"

[ -x "$ZAPD" ] || { echo "ZAPD nao encontrado: $ZAPD" >&2; exit 1; }

gen_inc_one() {
  src="$1"
  rel=""
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
      "$ZAPD" btex -eh -tt "$ext" -i "$src" -o "$out" >/dev/null 2>&1
      ;;
    *.bin)
      out="${out%.bin}.bin.inc.c"
      mkdir -p "$(dirname "$out")"
      "$ZAPD" bblb -eh -i "$src" -o "$out" >/dev/null 2>&1
      ;;
    *.jpg)
      out="${out%.jpg}.jpg.inc.c"
      mkdir -p "$(dirname "$out")"
      "$ZAPD" bren -eh -i "$src" -o "$out" >/dev/null 2>&1
      ;;
  esac
}

gen_schl_one() {
  s="$1"
  out="${s%.schl}.schl.inc"
  mkdir -p "$(dirname "$out")"
  python3 "$MM/tools/buildtools/schc.py" -o "$out" "$s" >/dev/null 2>&1
}

export -f gen_inc_one gen_schl_one
export ZAPD EXTRACTED_ASSETS COMMITTED_ASSETS BUILD MM

find "$EXTRACTED_ASSETS" "$COMMITTED_ASSETS" -type f \( -name '*.png' -o -name '*.bin' -o -name '*.jpg' \) 2>/dev/null \
  | grep -v '/xml/' | grep -v '/audio/' \
  | xargs -P6 -I{} bash -c 'gen_inc_one "$@"' _ {} >/dev/null 2>&1 || true

find "$MM/src" -name '*.schl' 2>/dev/null \
  | xargs -P4 -I{} bash -c 'gen_schl_one "$@"' _ {} >/dev/null 2>&1 || true

echo "gen_all_assets: $(find "$BUILD/assets" -name '*.inc.c' 2>/dev/null | wc -l) .inc.c, $(find "$MM/src" -name '*.schl.inc' 2>/dev/null | wc -l) .schl.inc"
