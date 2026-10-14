#!/usr/bin/env bash
# Extract game assets from a user-owned MM (USA 1.0) ROM using ZAPD.
# The ROM is NEVER committed; it is copied locally into the decomp tree only.
#
# Usage: scripts/extract_assets.sh "/path/to/Majoras Mask (USA).z64"
set -euo pipefail

ROM="${1:-}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MM="$ROOT/third_party/mm"

if [ -z "$ROM" ] || [ ! -f "$ROM" ]; then
    echo "usage: $0 /path/to/baserom.z64 (USA 1.0)" >&2
    exit 1
fi

"$ROOT/scripts/setup.sh"

mkdir -p "$MM/baseroms/n64-us"
cp "$ROM" "$MM/baseroms/n64-us/baserom.z64"

echo "MD5 of provided ROM:"
md5sum "$MM/baseroms/n64-us/baserom.z64"

echo "Running ZAPD asset extraction (make assets)..."
make -C "$MM" assets -j"$(nproc 2>/dev/null || echo 2)"
echo "Done. Extracted assets are under: $MM/assets"
