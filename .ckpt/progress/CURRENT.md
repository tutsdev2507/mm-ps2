# CURRENT — checkpoint de trabalho

Atualizado: 2026-10-16 15:36
Sessão: retomada pós-interrupção
Fase: 2 (desenvolvimento) — em progresso

## Avanços realizados

- [x] FASE 1: Auditoria completa (leitura de logs, .txt, git, build). Diagnóstico em `.ckpt/progress/INITIAL_AUDIT.md`.
- [x] Corrigido bug de SKIP: objetos `.o` vazios não são mais pulados (scripts `compile_ee.sh`, `compile_ee_o2.sh`).
- [x] Recompilados objetos vazios de código em `obj_o2` (incluindo `z_scene_table.o`, perfis de atores).
- [x] Gerados asset stubs abrangentes a partir do build COMPLETO (`obj`): `asset_stubs_all.c` → 60828 símbolos (cobre `_message_*`, texturas, etc.).
- [x] Adicionados addenda de símbolos de segmento ROM (`seg_addenda.o`) para símbolos faltantes (`_anime_*`, `_week_*`, `_makerom*`, etc.).
- [x] **LINK CODE-ONLY COM SUCESSO**: `third_party/mm/build/ee/MMPS2.ELF` (code image, assets stubbed).
  - Tamanho: ~11 MB (code+bss+data sem assets embutidos) — cabe em RAM PS2 (32 MB).
  - Símbolos indefinidos: **0**.

## Estado dos ELF

| Arquivo | Tam. | Estado |
|---|---|---|
| `port/mm-ps2.elf` | 1.6 MB | Demo M2 (F3DEX2 interpreter + object_ah) — existente |
| `platform/ps2/mmps2-boot.elf` | 1.6 MB | Bring-up runtime — existente |
| `third_party/mm/build/ee/MMPS2.ELF` | 11.0 MB | **NOVA — link code-only limpo (0 indefinidos)** |

## Próximo passo concreto

Agora precisamos:
1. Verificar se o ELF está funcional (entry point, estrutura). Validar com `readelf -h`/`objdump`.
2. Considerar compilar plataforma (`obj_ps2`) e também o build completo "full" com loader de assets? Ou gerar ELF final com link específico do port/PS2 (o Makefile do platform/ps2 constrói com runtime PS2).
3. O `platform/ps2/Makefile` atualmente gera `MMPS2.ELF` localmente com runtime gsKit + shims. Também temos o link `link_full_elf.sh` que produziu `third_party/mm/build/ee/MMPS2.ELF`. Precisamos consolidar: qual é o ELF final a rodar no PS2?

## Bloqueios

- Sem emulador PS2 no ambiente (validação em hardware necessário). Mas o link está limpo.
- Compilação -O2 de `obj_o2` ainda incompleta (assets não compilados lá; mas não precisamos para o link code-only atual). Podemos completar incrementalmente em background se desejado.

## Progresso relativo

O maior bloqueio (link com 5179 indefinidos) foi **resolvido**. Faltam agora validar boot/runtime e levar ao estado jogável.

## Destaque

**ELF PRONTO PARA TESTE:** `MMPS2.ELF` (11 MB) - link code-only limpo, 0 símbolos indefinidos, entry point válido, segmentos kseg0 corretos. Inclui código do jogo + shims de plataforma PS2.

O maior bloqueio foi resolvido. Próximo passo: validar boot/runtime e avançar no M3 conforme necessário.

## Status atual

ELF consolidado: `MMPS2.ELF` (11 MB) na raiz - link completo do jogo (code-only + stubs de assets + shims PS2), 0 símbolos indefinidos, válido para EE (MIPS R5900). Entry point aponta para boot chain correta (_start → bootproc).

Tentativa de build via platform/ps2/Makefile foi investigada; o link_final_elf.sh já produziu o ELF correto com todos os objetos do jogo. Mantemos esse ELF.

Próximo: testar execução (se possível em emulador) ou avançar M3 conforme necessário.

## Conclusão da etapa atual

ELF `MMPS2.ELF` (11 MB) está pronto: 0 símbolos indefinidos, entry point válido (_start), segmentos em kseg0, inclui runtime PS2 (shims) e todo o código do jogo. O link é limpo e estruturalmente correto.


## Comparação: mm_full.elf vs MMPS2.ELF

- `mm_full.elf` (47 MB, 15/out 22:15): link limpo, mas muito provavelmente com **assets embutidos** no .data (layout diferente, entry 0x100b20). Não adequado para PS2 (32 MB RAM).
- `MMPS2.ELF` (11 MB): **code-only + asset stubs + shims PS2** no layout kseg0 (entry em 0x804ed474), compatível com linkfile do port. Arquitetura correta (assets carregados em runtime).

Optamos pelo ELF code-only (11 MB) por ser o correto para o port PS2.

## Atualização (CORREÇÃO)

⚠️ A nota anterior dizendo "pi_dma funcional" estava **incorreta**. O
`platform/ps2/pi_dma.c` linkado no ELF é um **stub** (não lê o ROM): apenas
invalida cache e envia a completion com dados não inicializados. Por isso, ao
rodar no PS2, a tela fica **preta** — os assets nunca carregam.

Há um rascunho de implementação real com file I/O em
`platform/ps2/pi_dma_fileio.c` (não compila ainda por conflito de headers
`stdint.h` do decomp vs `sys/types.h` do ps2sdk).

Próximo passo detalhado em **`HANDOFF.md`** na raiz do repositório.

