# INITIAL_AUDIT — recuperação do projeto mm-ps2

Data: 2026-10-16 ~15:10 (America/Sao_Paulo)
Autor: sessão de retomada (opencode)

## 1. Resumo executivo

`mm-ps2` é um **port de código-fonte** (não emulador) da decompilação
[zeldaret/mm] (TLoZ: Majora's Mask, N64) para o **PlayStation 2** (EE+GS via
`ps2sdk`/`gsKit`). O jogo é recompilado para MIPS64R5900 little-endian e as
camadas de plataforma N64 (libultra/RSP/RDP) são substituídas por um backend PS2.

Estado geral: **M0, M1 e M2 concluídos** (harness, assets extraídos, renderer
F3DEX2→gsKit com texturas e color combiner). **M3 (boot) em andamento**. O
link do jogo completo ainda **não fecha**: a última tentativa de link deu
**5179 símbolos indefinidos**.

## 2. A ÚLTIMA INTERRUPÇÃO (ponto exato)

Os últimos artefatos produzidos antes da queda:

| Hora   | Artefato                                   | Estado |
|--------|--------------------------------------------|--------|
| 14:57  | `third_party/mm/build/ee/asset_stubs.c/.o` | gerado (25478 símbolos) |
| 14:57  | `scripts/gen_asset_stubs.sh` (novo)        | gerado |
| 14:59  | `.ckpt/compile_resume.log`                 | **compilação interrompida** (sem linha `RESULT`) |
| 15:00  | `.ckpt/link_codeonly.log`                  | **link falhou** — 5179 indefinidos |

Interpretação: a sessão anterior estava **recompilando todo o decomp em -O2
para `build/ee/obj_o2`** (build incremental retomado às 14:59) e, em paralelo,
tentou linkar o "code image". A compilação e o link foram interrompidos.

## 3. Árvore de build real

- **Toolchain**: `~/tmp/ps2root` (imagem `ps2dev/ps2dev` x86_64) rodando via
  `proot -q qemu-x86_64` (script `scripts/run_proot.sh`). GCC EE
  `mips64r5900el-ps2-elf-gcc` presente e funcional.
- **Fontes do jogo**: `third_party/mm/` (= zeldaret/mm), cwd de compilação.
- **Objetos**:
  - `third_party/mm/build/ee/obj/` — build **-O0 COMPLETO**: 1872 `.o`, **0 vazios** (15/out).
  - `third_party/mm/build/ee/obj_o2/` — recompilação **-O2 incompleta**: 1454 `.o`,
    **19 vazios**, faltam `interface/`, `archives/`, `misc/`, `boot/` e `assets/text`,
    `assets/archives`.
- **Shims de plataforma**: `third_party/mm/build/ee/obj_ps2/*.o` (12 objetos:
  boot, mmps2_os, hw_shim, cache_tlb, vi_gs, pi_dma, rsp_shim, segloader, runtime,
  dmadata_ps2, audio_stubs, gfx).
- **Stubs de link** (em `build/ee/`): `ps2_stubs.o`, `seg_stubs.o`, `ee_main.o`,
  `asset_stubs.o`.

## 4. ELFs existentes

| Arquivo | Tam. | Data | Observação |
|---|---|---|---|
| `port/mm-ps2.elf` | 1.6 MB | 15/out 13:49 | Demo M2: interpretador F3DEX2 + object_ah |
| `platform/ps2/mmps2-boot.elf` | 1.6 MB | 15/out 13:51 | Runtime de bring-up + math `gu` do decomp |
| `third_party/mm/build/ee/mm_full.elf` | 49 MB | 15/out 22:15 | Link "cheio" com stubs dummy; **usa libultra hardware do decomp** e **excede os 32 MiB** de RAM → inútil em HW |

## 5. DIAGNÓSTICO DA FALHA DE LINK (a causa raiz)

`.ckpt/link_codeonly.log` → 5179 símbolos indefinidos, categorizados:

| Categoria | Qtd | Origem real |
|---|---:|---|
| `_message_0x*` | 4637 | dados de mensagem (asset `assets/text`) **não incluídos no stub** |
| `*Tex` | 462 | texturas de interface/archives não incluídas no stub |
| `*SegmentRom*` | 60 | símbolos de segmento do linker |
| `*_Profile` | 6 | perfis de ator — objetos **vazios** em obj_o2 |
| `Entrance_Get*` | 4 | `z_scene_table.o` **vazio** em obj_o2 |
| `gSceneTable` e afins | ~10 | idem (código real, não asset) |

**Causas-raiz confirmadas:**

1. **Bug de SKIP de objetos vazios.** `scripts/compile_ee_o2.sh:24` faz
   `if [ -f "$obj" ] && [ "$obj" -nt "$f" ]` — um `.o` de **0 byte** criado por
   uma compilação interrompida é "mais novo" que o `.c` e passa a ser **SALTADO
   para sempre**. Por isso `z_en_trt2.o`, `z_scene_table.o` (que define
   `gSceneTable`/`Entrance_*`) e 12 assets de cena estão vazios.
2. **`gen_asset_stubs.sh` só varre `obj_o2/extracted/`.** Não cobre
   `assets/text` (`_message_*`), `assets/archives` nem
   `extracted/.../interface|archives|misc|boot`. Daí milhares de símbolos de
   asset sem stub.
3. **Recompilação -O2 incompleta.** `obj_o2` não tem os diretórios de asset
   `interface/archives/misc/boot` — a compilação foi morta antes de terminá-los
   (a linha `RESULT` nunca foi impressa em `compile_resume.log`).

## 6. O que está de fato CONCLUÍDO vs planejado

- **Concluído e verificável**: toolchain proot/qemu; renderer F3DEX2 (M2) com
  testes host (`port/tests/run.sh`); extração de assets (presentes em
  `third_party/mm/extracted`); build -O0 completo do decomp para EE; math `gu`
  float do decomp compilada e linkada.
- **Em andamento não finalizado**: recompilação -O2; link do code image;
  runtime de boot (`runtime.c`, `segloader.c`, `dmadata_ps2.c`, `hw_shim.c`).
- **Modificações não commitadas a preservar** (`git status`):
  `platform/ps2/{Makefile,mmps2_os.c,mmps2_os.h}`, `port/src/gfx/gfx.c` e todo o
  conjunto de shims não rastreado listado em `git status`. **Nada foi perdido.**
  Branch `main` está 32 commits à frente de `origin/main`.

## 7. Estratégia de retomada (maior impacto primeiro)

1. Corrigir o bug de SKIP (`-s "$obj"` em ambos scripts de compilação).
2. Regerar os asset stubs a partir do build **completo** (`obj`), cobrindo
   `assets/**` e `extracted/**`.
3. Fechar o link do code image: objetos de código + stubs de asset + símbolos de
   segmento (`SegmentRom*`) → resolver cada indefinido restante com evidência.
4. Recompilar -O2 (incremental, paralelismo baixo) para performance jogável.
5. Construir `MMPS2.ELF` executável e empacotar para USB/memcard.

## 8. Limitações honestas

- **Sem emulador de PS2 disponível** no Termux (só `qemu-user-x86_64`, que não
  emula GS/EE de PS2). Validação em emulador **não é possível** neste ambiente;
  a validação possível é: link limpo, `readelf`/`elf-size`, e correção de
  lógica. A validação em console real depende do usuário.
- **Meta "completo e zerável hoje"**: o port ainda está em M3 (boot). Fechar
  boot + input + áudio + persistência + o ciclo de 3 dias em um dia é
  irreal; o objetivo praticável é **entregar o máximo de progresso validado**
  (link limpo + ELF de boot o mais avançado possível) e documentar os bloqueios.
