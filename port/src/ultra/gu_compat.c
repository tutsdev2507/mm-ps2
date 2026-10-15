// C replacements for the decomp libultra gu helpers that the decomp only ships
// as MIPS assembly (src/libultra/mgu/mtxidentf.s and mgu/mtxf2l.s). They are
// needed to link the decomp's gu C sources on a non-N64 target (EE or host).
//
// Compiled with the decomp's own headers so the Mtx union layout matches.
#include "ultra64.h"

void guMtxIdentF(float mf[4][4])
{
    int i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            mf[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
}

void guMtxF2L(float mf[4][4], Mtx *m)
{
    int i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            float scaled = mf[i][j] * 65536.0f;
            s32 fixed = (s32)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);

            m->intPart[i][j] = (u16)((u32)fixed >> 16);
            m->fracPart[i][j] = (u16)((u32)fixed & 0xFFFF);
        }
    }
}
