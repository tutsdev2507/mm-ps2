#!/usr/bin/env bash
# Fetch the decompiled MM source at a pinned revision into third_party/mm.
# Pinned so our port never drifts under the game source.
set -euo pipefail

MM_REPO="${MM_REPO:-https://github.com/zeldaret/mm}"
MM_REV="${MM_REV:-bf6bd9d0d3a90fe2f8960e034fbfd328e8bdef22}"
DEST="$(cd "$(dirname "$0")/.." && pwd)/third_party/mm"

mkdir -p "$(dirname "$DEST")"
if [ ! -d "$DEST/.git" ]; then
    git clone "$MM_REPO" "$DEST"
fi
git -C "$DEST" fetch --all --tags
git -C "$DEST" checkout "$MM_REV"

echo "Decomp source ready at: $DEST ($MM_REV)"
