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
    src/ultra/              # libultra shim (OS/VI/controller/gu/DMA)  [M2]
    src/gfx/                # F3DEX2 GBI interpreter -> gsKit           [M2]
    src/audio/              # audio backend (stub first)                [M5]
    src/asset/              # ROM asset loader + endian conversion     [M1]
  scripts/
    setup.sh                # fetch decompiled game source at pinned rev
    extract_assets.sh       # run ZAPD extraction (needs user's ROM)
  .github/workflows/ps2.yml # build with ps2dev/ps2dev + prebuilt gsKit
```

## Roadmap / milestones

- [x] **M0 — toolchain harness**: build a bootable PS2 ELF that inits gsKit and
      clears the screen. Proves the CI + gsKit pipeline. *(in progress)*
- [ ] **M1 — assets**: extract assets from the user's USA 1.0 ROM with ZAPD.
- [ ] **M2 — renderer**: F3DEX2 GBI display-list interpreter emitting `gsKit` prims;
      draw one static display list (a room / title logo).
- [ ] **M3 — boot**: libultra shim (threads, mesg queues, timers, VI swap) so
      `bootproc -> Main -> graph thread` runs and presents frames.
- [ ] **M4 — input**: controller shim over `libpad` (PadMgr).
- [ ] **M5 — audio**: port the audio engine (or stub) to the EE.
- [ ] **M6 — persistence**: save / flash / SRAM backend to memory card.

## Build

Requires a **user-owned** copy of the USA 1.0 ROM for asset extraction only; the ROM is
never committed. See `scripts/setup.sh` and `scripts/extract_assets.sh`.

The PS2 ELF is built via GitHub Actions (`ps2dev/ps2dev` docker + prebuilt `gsKit`);
local Android/Termux cannot build MIPS/EE (`/storage` is `noexec`).
