# HANDOFF — estado do port e como continuar

> Documento de passagem de bastão. Explica **o que está pronto**, **o que está
> quebrado**, **como compilar** e **qual é o próximo passo concreto**.
> Última atualização: 2026-10-16.

## TL;DR

- O jogo compila e **linka** inteiro para o EE (ELF code-only, ~11 MB, 0
  símbolos indefinidos). Ver `.ckpt/progress/CURRENT.md`.
- **Mas o jogo não roda ainda**: ao executar no PS2 a tela fica preta.
- Causa mais provável: **os assets nunca são carregados**. `platform/ps2/pi_dma.c`
  é um *stub* (não lê o ROM), então toda DMA de segmento devolve lixo/zeros e o
  engine trava logo no boot, antes de desenhar a tela de título.
- Próximo passo: implementar o carregamento real de assets em `pi_dma.c` (há um
  rascunho em `platform/ps2/pi_dma_fileio.c`) e validar num emulador (PCSX2) ou
  em hardware com saída de debug (ps2link/net).

## Arquitetura do port

Não é um emulador. Compilamos o código C do decomp
[zeldaret/mm](https://github.com/zeldaret/mm) para o Emotion Engine (MIPS
little-endian) e substituímos as camadas de plataforma do N64:

| N64 | PS2 | Estado |
|---|---|---|
| libultra OS (threads, mesg queues, timers, VI, PI/DMA) | shims em `platform/ps2/*.c` sobre `ps2sdk` | parcial (M3) |
| RDP/RSP (F3DEX2 GBI) | interpretador em `port/src/gfx/gfx.c` → `gsKit` | parcial (M2) |
| RSP audio (`aspMain`) | stub silencioso | não iniciado (M5) |
| ROM/assets big-endian | carregados em runtime via PI DMA | **stub — não funciona** |

### Arquivos de plataforma (o coração do port)

- `platform/ps2/mmps2_os.c` — shim do OS libultra: `OSThread`/`OSMesgQueue`/
  timers sobre threads/semáforos do EE. As pilhas minúsculas do N64 são
  redirecionadas para um pool de 16 KiB por thread.
- `platform/ps2/hw_shim.c` — `__osInitialize_common` (o macro `osInitialize`
  expande para cá), `osGetMemSize` (32 MiB), `CIC6105_Init` (no-op), mascara de
  interrupção.
- `platform/ps2/cache_tlb.c` — cache flush (`FlushCache`), `osUnmapTLBAll`,
  `osVirtualToPhysical` (kseg0/kseg1 já é identidade no EE).
- `platform/ps2/vi_gs.c` — VI: retrace via `gsKit_add_vsync_handler`,
  `osViSetEvent` → `osSetEventMesg(OS_EVENT_VI, ...)`.
- `platform/ps2/rsp_shim.c` — `osSpTaskStartGo` chama direto o interpretador
  de display list (`mmps2_run_dl`).
- `platform/ps2/runtime.c` — dono do `GSGLOBAL` (gsKit) e do contexto gráfico;
  `mmps2_run_dl` = clear + interpreta a DL + flip (1 frame por DL mestre).
- `platform/ps2/pi_dma.c` — **STUB**. Deveria ler o ROM. Ver "Próximo passo".
- `platform/ps2/segloader.c` — **STUB** (só faz bookkeeping; `ready=1`).
- `platform/ps2/dmadata_ps2.c` — `gDmaDataTable` real (VROM→ROM) extraída do
  ROM retail. Gerada por `scripts/gen_dmadata_ps2.py`.
- `platform/ps2/linkfile` — linker script (coloca a imagem em kseg0).
- `platform/ps2/override_os_init.c` — `__wrap_osInitialize` (só usado se o link
  tiver `--wrap`, hoje não é o caminho ativo).
- `platform/ps2/audio_stubs.c` — tabelas de áudio vazias (M5).

### Fluxo de boot

```
_start (build/ee/ps2_stubs.c)
  → bootproc (src/boot/boot_main.c)
      → osGetMemSize / CIC6105_Init / osInitialize (=__osInitialize_common)
          → mmps2_os_init + publica osTvType/osMemSize + mmps2_runtime_init (gsKit up)
      → osCartRomInit / cria thread idle
  → Idle_ThreadEntry → Main_ThreadEntry → Main_Init → Graph_ThreadEntry
  → graph thread espera VI e emite a DL mestre
      → osSpTaskStartGo → mmps2_run_dl → interpretador → gsKit flip
```

Se a tela fica preta, ou o boot trava antes do graph thread (provável: DMA de
assets devolvendo lixo), ou o `gsKit` não subiu (checar `g_gs`).

## Como compilar

Requisitos: Termux (aarch64) com `proot`, `qemu-user-x86_64`, `python3`, `curl`,
`tar`, `zip`. O toolchain ps2dev x86_64 roda via qemu/proot.

```bash
pkg install proot qemu-user-x86_64 python curl
scripts/setup.sh              # clona o decomp no rev fixo
scripts/build_termux.sh       # baixa a imagem ps2dev e constrói os ELFs de bring-up
```

Para o **ELF completo do jogo** (o de 11 MB), o pipeline é:

```bash
# 1) extrair assets do SEU ROM (USA 1.0) — nunca commitado
scripts/extract_assets.sh "/caminho/para/baserom.z64"

# 2) compilar todos os objetos do decomp para EE (dentro do proot)
#    gera third_party/mm/build/ee/obj_o2/**
scripts/run_proot.sh sh /src/scripts/compile_ee_o2.sh

# 3) gerar stubs de assets + addenda de segmentos e linkar o code-only ELF
#    → third_party/mm/build/ee/MMPS2.ELF  (ver scripts/handoff notes)
```

> Observação: os scripts `compile_ee*.sh`, `link_full_elf.sh`,
> `gen_asset_stubs_all.sh`, `gen_sized*.sh` usam caminhos absolutos `/src/...`
> e devem rodar **dentro** do proot (via `scripts/run_proot.sh`), onde o repo é
> montado em `/src`.

## Como rodar no PS2

Ver `HOW_TO_RUN_PS2.md`. Resumo: pendrive (FAT32/exFAT com OPL novo) com

```
mass0:/MMPS2/MMPS2.ELF
mass0:/MMPS2/baserom.z64      (ou baserom-decompressed.z64)
```

execute o ELF via uLaunchELF ou OPL. **Hoje isso mostra tela preta** (assets não
carregam).

## Correções de boot (2026-10-16, tarde) — NÃO TESTADAS em hardware

Análise do `MMPS2.ELF` mostrou que a tela preta tinha causas anteriores ao
`pi_dma`:

1. **Imagem linkada em 0x80010000 (físico 0x10000)** = em cima do kernel do
   EE (1º MiB da RAM). Agora `.text` começa em `0x80100000` (`linkfile`).
2. **Sem crt0**: o `_start` antigo (em `build/ee/ps2_stubs.c`) só chamava
   `bootproc`. Agora `platform/ps2/crt0_mmps2.c` define `_start` (gp, pilha
   própria dentro da imagem, zera `.bss`, chama `bootproc`) e `_sbrk` (heap
   de 2 MiB logo após `_end`). Precisa vencer o `_start` antigo: está em
   `obj_ps2`, que é linkado antes de `ps2_stubs.o` (`--allow-multiple-definition`).
3. **I/O de arquivo** (`port/src/ps2/rom_io.c`) usa `fioOpen/fioRead` do
   ps2sdk (não depende de init da libc) e lê via buffer alinhado.
   `pi_dma.c` agora lê o ROM de verdade.
4. **Log em arquivo**: `mass:/MMPS2/mmps2.log` (linhas `rom:`, `dma:`, `boot:`,
   `gfx:`). A última linha mostra até onde o boot chegou.
5. `runtime.c` inicializa o canal DMA GIF (`dmaKit`) antes do gsKit.

Para aplicar: apague `build/ee/obj_ps2`, rode `compile_new.sh` (agora inclui
`crt0_mmps2.c` e `rom_io.c`) e `link_full_elf.sh`.

Riscos conhecidos ainda abertos: endianness dos assets (ROM é big-endian),
`Main_InitMemory` apagando RAM até 0x82000000, heap de 2 MiB pequeno para
texturas decodificadas, entrada/áudio/save (M4–M6) não iniciados.

## Próximo passo concreto (para quem continuar)

### 1. Implementar `pi_dma.c` de verdade  ← maior valor

O engine chama `osEPiStartDma(handle, mb, dir)` com `mb->devAddr` = offset
físico no ROM (vem de `gDmaDataTable`) e `mb->dramAddr`/`mb->size` o destino.
É preciso ler `mb->size` bytes do arquivo ROM em `mb->devAddr` para `dramAddr`,
invalidar o cache e postar a completion em `mb->hdr.retQueue`.

Rascunho em `platform/ps2/pi_dma_fileio.c` (não compila ainda por conflito de
headers). **Solução recomendada**: isolar o I/O num *translation unit* que veja
**só** headers do ps2sdk (sem `ultra64.h`), expondo:

```c
int  mmps2_rom_open(void);
int  mmps2_rom_read(unsigned int off, void *buf, unsigned int size);
```

e em `pi_dma.c` (que inclui `mmps2_os.h`/`ultra64.h`) apenas declarar e chamar
essas funções. Isso evita o choque entre `third_party/mm/include/libc/stdint.h`
e `sys/types.h` do SDK (`uintptr_t`/`wint_t` redefinidos).

Formatos de ROM: as entradas comprimidas do `gDmaDataTable` (`romEnd != 0`) são
Yaz0 e o próprio engine chama `Yaz0_Decompress(romStart, ram, romSize)`; para
essas, `devAddr` é o offset do bloco Yaz0 no ROM retail (`baserom.z64`).

### 2. Validar com emulador

Não há PCSX2 no ambiente de build atual. Rodar em PCSX2 (com o ROM do lado de
fora do repositório) acelera muito o debug. Alternativas: `ps2link` + `ps2client`
para logs, ou `ee-printf` para o console.

### 3. Conferir o boot antes dos assets

Adicionar um clear colorido em `_start`/`__osInitialize_common` para provar que
o GS sobe antes de qualquer DMA. Se aparecer cor, o problema é o loader; se não,
é o `gsKit`/linkfile.

### 4. Milestones restantes

- **M3 (boot)** — fechar o runtime (ver acima).
- **M4 (input)** — shim de controle sobre `libpad` (PadMgr).
- **M5 (audio)** — portar o engine de áudio (hoje stub silencioso).
- **M6 (save)** — backend de save no memory card.

## Dívidas / armadilhas conhecidas

- `third_party/`, `build/`, `*.o`, `*.elf`, `*.z64`, `baserom.*` estão no
  `.gitignore`. O decomp é obtido por `scripts/setup.sh`; o ROM é do usuário.
- Objetos `.o` vazios já foram corrigidos (`compile_ee*.sh` não pulam `.o` de
  tamanho 0), mas ao mudar flags **apague `build/ee/obj_o2`** e recompile.
- O link usa `--allow-multiple-definition` e lista os objetos de plataforma
  **primeiro**, para que os shims vençam o libultra do decomp.
- `dmadata_ps2.c` corresponde ao ROM **USA 1.0**. Outro ROM → regenere com
  `scripts/gen_dmadata_ps2.py` (ajustando o caminho).
- As `*SegmentRomStart/End` em `build/ee/seg_addenda.c` são símbolos dummy de
  1 byte — servem só para linkar; as posições reais vêm do `gDmaDataTable`.
