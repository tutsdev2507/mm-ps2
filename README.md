# mm-ps2 — Majora's Mask source port to the PlayStation 2

Port of the [zeldaret/mm](https://github.com/zeldaret/mm) decompilation (The Legend of
Zelda: Majora's Mask, N64) to the Sony PlayStation 2 (EE + GS, `ps2sdk` / `gsKit`).

This is **not** an emulator. We compile the decompiled game C code for the PS2 Emotion
Engine and replace the N64 platform layers (libultra / RSP / RDP) with a PS2 backend.

## Why this is hard (read first)

The decomp produces a **big-endian MIPS (N64)** binary that drives the **RCP** (RSP+RDP).
The PS2 is **little-endian MIPS** with a completely different GPU (GS) and no RSP.
So a port must replace four subsystems:

| N64 subsystem | PS2 replacement | Difficulty |
|---|---|---|
| libultra OS (OSThread, mesg queues, VI, PI/DMA, timers) | `ps2sdk` (EE threads / DMAC / gsKit present) | medium |
| RDP/RSP graphics (F3DEX2 GBI display lists) | GBI interpreter -> `gsKit` (GS) | **very high** |
| RSP audio microcode (`aspMain`) | stub (silence) first, port engine later | high |
| ROM asset format (big-endian) | extract from ROM, convert to native at load | medium |

Endianness: game *logic* is endian-agnostic C, but *assets* (display lists, textures,
actor data) are big-endian and must be byte-swapped on load.

## Architecture

```
mm-ps2/
  port/
    src/main.c              # EE entry: init gsKit, run the game loop
    src/gfx/gbi.h           # F3DEX2 GBI subset types/macros            [M2]
    src/gfx/gfx.{h,c}       # F3DEX2 display-list interpreter -> gsKit  [M2]
    src/ultra/              # libultra shim (OS/VI/controller/gu/DMA)  [M3]
    src/audio/              # audio backend (stub first)                [M5]
    src/asset/              # ROM asset loader + endian conversion     [M1]
    tests/                  # host-side interpreter tests (native cc)
  scripts/
    setup.sh                # fetch decompiled game source at pinned rev
    extract_assets.sh       # run ZAPD extraction (needs user's ROM)
  .github/workflows/ps2.yml # EE build (ps2dev/ps2dev) + host tests
```

## The GBI interpreter (M2)

The game emits **F3DEX2 / F3DZEX2** display lists (`-DF3DEX_GBI_2`, which also defines
`F3DEX_GBI`, so triangle indices are the `×2` encoding, in `w0` for `G_TRI1` and in
`w0`/`w1` for `G_TRI2`/`G_QUAD`). `port/src/gfx/gfx.c` walks a list and emits gsKit
primitives. Implemented: `G_MTX`/`G_POPMTX` (proj + modelview stacks), `G_VTX`/
`G_MODIFYVTX`, `G_TRI1`/`G_TRI2`/`G_QUAD`, `G_DL`/`G_ENDDL`, `G_MOVEMEM` (viewport),
`G_MOVEWORD` (`G_MW_SEGMENT`/`G_MW_PERSPNORM`), colors, `G_GEOMETRYMODE`, and the
N64 colour combiner (`G_SETCOMBINE`, 1- and 2-cycle, evaluated per vertex with an
opaque-white texel; the GS then multiplies by the sampled texel).

Textures are decoded to a 32-bit surface on first use and uploaded through gsKit:
RGBA16/32, CI4/8 (with the loaded TLUT) and IA/I, with dimensions taken from
`G_SETTILESIZE` or inferred from `G_LOADBLOCK` + the render tile line. Tile state is
tracked per tile descriptor. Unknown commands are no-ops so lists run.

Addresses in `w1` are resolved through the 16-entry segment table (`gsSPSegment`), falling
back to a raw pointer for hand-built lists.

## Current status (2026-10-16)

The full decompiled game now **compiles and links** for the EE into a single
code-only ELF (`third_party/mm/build/ee/MMPS2.ELF`, ~11 MB, **0 undefined
symbols**). Assets are stubbed at link time and are meant to be streamed from
the ROM at runtime.

**It does not run yet** — on real PS2 the screen stays black. The blocking
problem is that the runtime asset loader is still a stub
(`platform/ps2/pi_dma.c` does not read the ROM), so segment DMAs return
garbage and the engine stalls before the title screen.

See **[HANDOFF.md](HANDOFF.md)** for the exact state, the boot flow, the build
pipeline, and the next concrete step (implementing the loader). Quick build:
`scripts/build_full_game.sh`.

## Roadmap / milestones

- [x] **M0 — toolchain harness**: build a bootable PS2 ELF that inits gsKit and
      clears the screen. Proves the CI + gsKit pipeline.
- [x] **M1 — assets**: extract assets from the user's USA 1.0 ROM with ZAPD.
- [x] **M2 — renderer**: F3DEX2 GBI display-list interpreter emitting `gsKit` prims.
      Geometry, per-tile textures (RGBA/CI/IA/I, TLUT) and the colour combiner are
      implemented. Renders an animated textured 3D cube and, via the integration
      test, the real extracted `object_ah` display lists (textured, 8x8..32x32).
- [ ] **M3 — boot**: libultra shim (threads, mesg queues, timers, VI swap) so
      `bootproc -> Main -> graph thread` runs and presents frames. *(started: the
      decomp's own float `gu` math — `guPerspectiveF`/`guLookAtF` — is compiled,
      linked and tested through `port/src/ultra/`; build the bring-up runtime with
      `make -C platform/ps2 MM_DIR=third_party/mm` to build its projection/view
      matrices. C replacements for the assembly `guMtxIdentF`/`guMtxF2L` live
      there too)*
- [ ] **M4 — input**: controller shim over `libpad` (PadMgr).
- [ ] **M5 — audio**: port the audio engine (or stub) to the EE.
- [ ] **M6 — persistence**: save / flash / SRAM backend to memory card.

## Build

Requires a **user-owned** copy of the USA 1.0 ROM for asset extraction only; the ROM is
never committed. See `scripts/setup.sh` and `scripts/extract_assets.sh`.

The PS2 ELFs are built either by GitHub Actions (`.github/workflows/ps2.yml`, uses the
`ps2dev/ps2dev` image) or **locally on Android/Termux**.

### Local Termux build (no CI, no token)

Termux is aarch64 and cannot run the ps2dev x86_64 toolchain natively, but it runs fine
through `qemu-x86_64` + `proot`:

```
pkg install proot qemu-user-x86_64 python curl
scripts/setup.sh                       # fetch the pinned decomp source
scripts/build_termux.sh                # downloads the ps2dev image, builds both ELFs
```

`scripts/build_termux.sh` pulls the public `ps2dev/ps2dev` OCI image from `ghcr.io`,
unpacks its filesystem + `/usr/local/ps2dev` toolchain into a local rootfs
(`$HOME/tmp/ps2root`), recreates the toolchain's `$sysroot/bin` links (the app-data
filesystem rejects the image's hard links), installs `make`, and produces:

```
port/mm-ps2.elf                  # F3DEX2 interpreter + object_ah demo
platform/ps2/mmps2-boot.elf      # bring-up runtime (+ decomp gu math)
```

The GBI interpreter also has native host tests (no EE toolchain needed):

```
bash port/tests/run.sh
```

They compile `src/gfx/gfx.c` against stub headers and assert the emitted triangles for
TRI1/TRI2/QUAD, viewport, segment addressing, geometry-mode set/clear and per-tile
texture decoding/UVs.
