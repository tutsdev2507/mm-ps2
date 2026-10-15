#!/usr/bin/env bash
#
# Builds the object_ah "renderer" harness for 32-bit x86 and runs it, emitting a
# triangle dump on stdout. The asset stores 32-bit pointers, so the build needs
# -m32. This script targets the local proot-distro 'deb64' container (x86_64
# under qemu) plus the Termux qemu-i386 to run the resulting 32-bit binary.
#
# Usage: port/tests/render_object_ah.sh > /tmp/dump.txt
#
# Env:
#   CONTAINER  proot-distro container name (default: deb64)
#   CROOT      repo path as seen inside the container (default: /storage/emulated/0/Download/mm-ps2)
#   OUT        output binary path inside the container (default: /root/render_object_ah)
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
container="${CONTAINER:-deb64}"
croot="${CROOT:-/storage/emulated/0/Download/mm-ps2}"
rootfs="$PREFIX/var/lib/proot-distro/containers/$container/rootfs"
out="${OUT:-/root/render_object_ah}"

if [ ! -d "$rootfs" ]; then
    echo "no such container: $container ($rootfs)" >&2
    exit 1
fi

# The portable C sources are shared with the host build.
srcs="$croot/port/tests/render_object_ah.c $croot/port/src/gfx/gfx.c $croot/port/assets/object_ah/object_ah.c"
incs="-I $croot/port/tests/stub -I $croot/port/src -I $croot/port/src/gfx -I $croot/port/assets/object_ah"

# Compile + link inside the container (native x86_64 gcc, -m32). Must stay
# PIE/dynamic: the interpreter treats a display-list word whose top byte is
# >= 0x10 as an absolute pointer, and non-PIE (-static) images load at
# 0x08048xxx whose 0x08 top byte would be misread as a segment reference.
proot-distro login "$container" -- /bin/bash -lc \
    "gcc -m32 -std=gnu89 -O1 -g -w $incs $srcs -lm -o $out"

# Run the 32-bit binary with qemu-i386, using the container as its sysroot.
qemu-i386 -L "$rootfs" "$rootfs$out"
