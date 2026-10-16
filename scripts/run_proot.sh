#!/bin/bash
# Generic proot wrapper: run a command inside the ps2dev rootfs with the
# project bind-mounted at /src.  Used to drive the EE toolchain from Termux
# (aarch64 host) via qemu-x86_64 + proot.
#
#   scripts/run_proot.sh <command ...>
#
# The toolchain is expected at $HOME/tmp/ps2root (see scripts/build_termux.sh).
set -euo pipefail
unset LD_PRELOAD LD_LIBRARY_PATH

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
R="${PS2ROOT:-$HOME/tmp/ps2root}"

exec proot -q qemu-x86_64 -r "$R" -b /dev -b /proc -b /sys \
  -b "$PREFIX/etc/resolv.conf:/etc/resolv.conf" \
  -b "$ROOT_DIR:/src" -b "$HOME/tmp:/hosttmp" -w /src \
  /bin/sh -c '
    export PATH=/usr/local/ps2dev/ee/bin:/usr/local/ps2dev/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
    export PS2DEV=/usr/local/ps2dev; export PS2SDK=$PS2DEV/ps2sdk
    export TMPDIR=/tmp HOME=/root
    exec "$@"
  ' sh "$@"
