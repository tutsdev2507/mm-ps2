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

**Tela colorida por estágio** (mais confiável, sobrevive ao init do gsKit):
após o boot o GS fica numa cor sólida que indica o último estágio alcançado.

| Cor | Significado |
|---|---|
| **Preto** | O ELF não arrancou (ou travou antes do gsKit) — confirme que está executando o `MMPS2.ELF` novo. |
| **Azul** | gsKit/DMA no ar. Travou antes de achar o ROM / antes da 1ª display list. |
| **Verde** | `baserom.z64` encontrado e aberto. Travou no conteúdo do jogo (M4–M6), não no boot. |
| **Vermelho** | `baserom.z64` NÃO encontrado no pendrive. Confira o caminho. |

**Depuração em texto na tela** (libdebug, verde): durante o boot (antes do
gsKit limpar) o jogo imprime marcadores por estágio. Ordem:

```
crt0: gp/sp/bss ok -> bootproc   ← o ELF arrancou
boot: __osInitialize_common      ← entrou no osInitialize
boot: gsKit up                   ← gsKit/DMA inicializados
rom: opened mass:/MMPS2/baserom.z64   ← ROM encontrada (ou "rom: ERROR ...")
gfx: first display list running  ← o jogo desenhou o primeiro frame
```

**Log em arquivo** (complementar): `MMPS2/mmps2.log` no pendrive (via fileXio).
Requer driver `mass:` já carregado pelo launcher. Se não houver log, confie na
tela colorida — ela funciona sem dispositivo de arquivo.
