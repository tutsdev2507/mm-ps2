# Como rodar Majora's Mask (mm-ps2) no PlayStation 2

## Estrutura no pendrive (FAT32)

Coloque os arquivos assim na raiz do USB:

```
mass0:/
  └── MMPS2/
      ├── MMPS2.ELF
      └── baserom.z64  (ou baserom-decompressed.z64)
```

O pacote `mm-ps2_for_ps2.zip` já vem com essa estrutura pronta. Basta extrair direto para o pendrive.

## Como executar

1. **uLaunchELF** (mais direto) - Abra uLaunchELF, vá em `File Browser > mass:/MMPS2/` e rode `MMPS2.ELF`.
2. **OPL** - Coloque a pasta `MMPS2/` inteira dentro de `mass0:/APPS/`. Assim fica: `mass0:/APPS/MMPS2/MMPS2.ELF` e `baserom.z64` junto.
3. **HDLoader/ETH** - Similar ao OPL.

## Dicas

- Prefira `baserom.z64` (32 MB) ao invés do descomprimido (47 MB).
- Use pendrive USB 2.0 para melhor compatibilidade.
- Formate em FAT32 (não exFAT/NTFS).
- Se ficar tela preta, confirme que o baserom está na **mesma pasta** que o ELF.

## Notas técnicas

- ELF full-game: ~25 MB. Assets carregados em runtime via PI DMA do baserom.
- Link limpo (0 símbolos indefinidos), boot chain completa (_start → bootproc).
- Interpretador F3DEX2 + gsKit implementado. Sistema de boot/threading funcional.

## Diagnóstico

**Depuração em tela** (não precisa de log nem de dispositivo nenhum): o jogo
imprime texto verde na tela a cada estágio do boot, da fonte de debug embutida
do PS2 (libdebug). Ordem dos marcadores:

```
crt0: gp/sp/bss ok -> bootproc   ← o ELF arrancou (se não aparecer nada, o loader não carregou)
boot: __osInitialize_common      ← entrou no osInitialize
boot: gsKit up                   ← gsKit/DMA inicializados
rom: opened mass:/MMPS2/baserom.z64   ← ROM encontrada (ou "rom: ERROR ..." se não achar)
gfx: first display list running  ← o jogo desenhou o primeiro frame
```

A **última linha visível** diz onde o boot parou. Se aparecer `rom: ERROR`, o
baserom não foi encontrado no pendrive. Se aparecer tudo até `gfx:`, o problema
é de conteúdo (M4–M6), não de boot.

**Log em arquivo** (complementar): o jogo grava `MMPS2/mmps2.log` no pendrive
(via fileXio). Requer que o launcher já tenha carregado o driver `mass:`.
Se a tela ficar preta e não houver log, confie na tela — ela funciona sem o
dispositivo de arquivo.
