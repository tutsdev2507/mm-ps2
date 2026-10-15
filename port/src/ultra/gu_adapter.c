// Bridge from the decomp's float gu math to the port's split-fixed Mtx.
//
// guPerspectiveF / guLookAtF are defined in the decomp (src/libultra/gu) and are
// declared here with plain float/u16 types so this translation unit stays free
// of the decomp's N64 headers. The float[4][4] they produce is converted to the
// port's intPart/fracPart (s15.16) representation, identical to the union the
// decomp uses for Mtx.
//
// Convention: the decomp (and N64) transform vertices as p * M (row-vector,
// translation in the last row). The port's interpreter transforms as M * p
// (column-vector, translation in the last column), so the matrix is transposed
// as it is converted.
#include "ultra/gu_adapter.h"

extern void guPerspectiveF(float mf[4][4], u16 *perspNorm, float fovy,
                           float aspect, float near, float far, float scale);
extern void guLookAtF(float mf[4][4], float xEye, float yEye, float zEye,
                      float xAt, float yAt, float zAt,
                      float xUp, float yUp, float zUp);

static void f2l_transposed(Mtx *m, const float mf[4][4])
{
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            float scaled = mf[j][i] * 65536.0f;
            s32 fixed = (s32)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
            m->intPart[i][j] = (u16)((u32)fixed >> 16);
            m->fracPart[i][j] = (u16)((u32)fixed & 0xFFFF);
        }
    }
}

void mmps2_gu_perspective(Mtx *m, float fovy, float aspect, float near, float far)
{
    float mf[4][4];
    u16 perspNorm = 0;

    guPerspectiveF(mf, &perspNorm, fovy, aspect, near, far, 1.0f);
    f2l_transposed(m, mf);
}

void mmps2_gu_lookat(Mtx *m, float xEye, float yEye, float zEye,
                     float xAt, float yAt, float zAt,
                     float xUp, float yUp, float zUp)
{
    float mf[4][4];

    guLookAtF(mf, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);
    f2l_transposed(m, mf);
}
