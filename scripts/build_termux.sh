#!/usr/bin/env bash
# Build the PS2 ELFs on Android/Termux -- no GitHub Actions, no token needed.
#
# Termux is aarch64 and cannot run the ps2dev x86_64 (musl) toolchain directly,
# but qemu-x86_64 + proot can. This script:
#   1. pulls the public ps2dev/ps2dev Docker image (OCI layers) and unpacks its
#      filesystem + the /usr/local/ps2dev toolchain into a local rootfs,
#   2. recreates the toolchain's hardlinked $sysroot/bin (as/ld/nm/...),
#   3. builds `port/mm-ps2.elf` and `platform/ps2/mmps2-boot.elf`.
#
# Requirements (Termux): proot, qemu-user-x86_64, curl/python3, tar.
#   pkg install proot qemu-user-x86_64 python curl
#
# Usage:
#   scripts/build_termux.sh [rootfs-dir]      (default: $HOME/tmp/ps2root)
set -euo pipefail

# Termux sets an aarch64 LD_PRELOAD (libtermux-exec) that would be handed to the
# x86_64 binaries; drop it so qemu can exec the toolchain cleanly.
unset LD_PRELOAD LD_LIBRARY_PATH

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="${IMAGE:-ps2dev/ps2dev:latest}"
R="${1:-$HOME/tmp/ps2root}"
LAYER_DIR="$HOME/tmp/ps2dev_layers"
MUSL="${MUSL:-/lib/ld-musl-x86_64.so.1}"

fetch_rootfs() {
    if [ -e "$R$MUSL" ] && [ -x "$R/usr/local/ps2dev/ee/bin/mips64r5900el-ps2-elf-gcc" ]; then
        echo "[rootfs] reusing $R"
        return
    fi
    echo "[rootfs] downloading $IMAGE from ghcr.io"
    mkdir -p "$LAYER_DIR" "$R"
    TOKEN="$(curl -fsS "https://ghcr.io/token?scope=repository%3Aps2dev%2Fps2dev%3Apull&service=ghcr.io" \
            | python3 -c 'import sys,json;print(json.load(sys.stdin)["token"])')"
    ACCEPT="application/vnd.docker.distribution.manifest.list.v2+json,application/vnd.oci.image.index.v1+json,application/vnd.docker.distribution.manifest.v2+json"
    curl -fsS -H "Authorization: Bearer $TOKEN" -H "Accept: $ACCEPT" \
        "https://ghcr.io/v2/ps2dev/ps2dev/manifests/latest" -o "$LAYER_DIR/index.json"
    python3 - "$LAYER_DIR" "$TOKEN" <<'PY'
import json,sys,urllib.request,os
d,tok=sys.argv[1],sys.argv[2]
idx=json.load(open(os.path.join(d,"index.json")))
amd=next(m for m in idx["manifests"] if m.get("platform",{}).get("architecture")=="amd64"
         and m.get("platform",{}).get("os")=="linux")
req=urllib.request.Request(f"https://ghcr.io/v2/ps2dev/ps2dev/manifests/{amd['digest']}",
    headers={"Authorization":f"Bearer {tok}","Accept":"application/vnd.oci.image.manifest.v1+json"})
man=json.load(urllib.request.urlopen(req))
json.dump(man,open(os.path.join(d,"manifest.json"),"w"))
for i,l in enumerate(man["layers"]):
    fn=os.path.join(d,f"layer{i}.tar")
    if os.path.exists(fn) and os.path.getsize(fn)==l["size"]:
        continue
    print(f"[rootfs] layer {i}: {l['size']/1e6:.1f} MB")
    r=urllib.request.Request(f"https://ghcr.io/v2/ps2dev/ps2dev/blobs/{l['digest']}",
        headers={"Authorization":f"Bearer {tok}"})
    with urllib.request.urlopen(r) as s, open(fn,"wb") as o:
        while True:
            b=s.read(1<<20)
            if not b: break
            o.write(b)
PY
    for i in 0 1 2; do
        [ -e "$LAYER_DIR/layer$i.tar" ] || continue
        echo "[rootfs] extracting layer$i"
        tar xzf "$LAYER_DIR/layer$i.tar" -C "$R" --hard-dereference 2>&1 | grep -v "Cannot hard link" || true
    done
    # Recreate the $sysroot/bin hardlinks as relative symlinks (the app-data
    # filesystem rejects the image's hard links; GCC finds as/ld there).
    python3 - "$LAYER_DIR/layer1.tar" "$R" <<'PY'
import tarfile,os,sys
tf=tarfile.open(sys.argv[1]); root=sys.argv[2]; n=0
for m in tf:
    if m.islnk() and ("/ee/mips64r5900el-ps2-elf/bin/" in m.name or "/iop/mipsel-none-elf/bin/" in m.name):
        dest=os.path.join(root,m.name); os.makedirs(os.path.dirname(dest),exist_ok=True)
        if os.path.lexists(dest): os.remove(dest)
        os.symlink(os.path.relpath(m.linkname,os.path.dirname(m.name)),dest); n+=1
print(f"[rootfs] toolchain sysroot links: {n}")
PY
}

# Install `make` into the rootfs (the ps2dev image ships without it).
ensure_make() {
    if [ -x "$R/bin/make" ] || [ -x "$R/usr/bin/make" ]; then return; fi
    echo "[rootfs] installing make via apk"
    proot -q qemu-x86_64 -r "$R" -b /dev -b /proc -b /sys \
        -b "$PREFIX/etc/resolv.conf:/etc/resolv.conf" -w /root /bin/sh -c \
        'export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin; apk add --no-cache make' \
        2>&1 | tail -3 || true
}

run_in_rootfs() {
    proot -q qemu-x86_64 -r "$R" \
        -b /dev -b /proc -b /sys \
        -b "$PREFIX/etc/resolv.conf:/etc/resolv.conf" \
        -b "$ROOT_DIR:/src" -b "$HOME/tmp:/hosttmp" -w /src \
        /bin/sh -c '
            export PATH=/usr/local/ps2dev/ee/bin:/usr/local/ps2dev/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
            export PS2DEV=/usr/local/ps2dev
            export PS2SDK=$PS2DEV/ps2sdk
            export GSKIT=$PS2DEV/gsKit
            export TMPDIR=/tmp HOME=/root
            '"$*"
}

fetch_rootfs
ensure_make

echo "[build] port/mm-ps2.elf"
run_in_rootfs 'make -C port GSKIT=$GSKIT'
echo "[build] platform/ps2/mmps2-boot.elf"
run_in_rootfs 'make -C platform/ps2 MM_DIR=/src/third_party/mm'

echo
echo "Built:"
echo "  $ROOT_DIR/port/mm-ps2.elf"
echo "  $ROOT_DIR/platform/ps2/mmps2-boot.elf"
