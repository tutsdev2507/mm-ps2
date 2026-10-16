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

- ELF code-only: 11 MB. Assets carregados em runtime via PI DMA do baserom.
- Link limpo (0 símbolos indefinidos), boot chain completa (_start → bootproc).
- Interpretador F3DEX2 + gsKit implementado. Sistema de boot/threading funcional.

## Diagnóstico

O jogo grava `MMPS2/mmps2.log` no pendrive. Se a tela ficar preta, envie esse arquivo.
