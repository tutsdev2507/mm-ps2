// Host test: run the decomp's float gu math (guPerspectiveF / guLookAtF) through
// the port's adapter and check it produces the expected split-fixed matrices.
//
// This is the first slice of the decomp linked into the port (M3 kickoff): it
// proves the decomp sources compile against their own headers and that the
// float[4][4] -> Mtx (s15.16 intPart/fracPart) bridge is faithful. No -m32 is
// needed because this math involves no pointers baked into assets.
#include <math.h>
#include <stdio.h>

#include "ultra/gu_adapter.h"

static int g_fail = 0;

static float get(const Mtx *m, int r, int c)
{
    u32 word = ((u32)m->intPart[r][c] << 16) | (u32)m->fracPart[r][c];

    return (float)(s32)word / 65536.0f;
}

static void checkf(const Mtx *m, int r, int c, float want, const char *what)
{
    float got = get(m, r, c);

    printf("  [%s] %s = %.4f (want %.4f)\n",
           fabsf(got - want) < 1e-3f ? "ok" : "FAIL", what, got, want);
    if (fabsf(got - want) >= 1e-3f)
        g_fail = 1;
}

int main(void)
{
    Mtx m;

    printf("guPerspectiveF (fovy=90deg, aspect=1, near=1, far=2) -> port column convention:\n");
    mmps2_gu_perspective(&m, 90.0f, 1.0f, 1.0f, 2.0f);
    checkf(&m, 0, 0, 1.0f, "m[0][0]=cot/aspect");
    checkf(&m, 1, 1, 1.0f, "m[1][1]=cot");
    checkf(&m, 2, 2, -3.0f, "m[2][2]=(n+f)/(n-f)");
    checkf(&m, 3, 2, -1.0f, "m[3][2]=-1 (clip.w = -z)");
    checkf(&m, 2, 3, -4.0f, "m[2][3]=2nf/(n-f)");
    checkf(&m, 3, 3, 0.0f, "m[3][3]=0");
    checkf(&m, 3, 0, 0.0f, "no stray translation");

    printf("guLookAtF (eye=(0,0,5), at=origin, up=+Y) -> port column convention:\n");
    mmps2_gu_lookat(&m, 0.0f, 0.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    checkf(&m, 0, 0, 1.0f, "right.x");
    checkf(&m, 1, 1, 1.0f, "up.y");
    checkf(&m, 2, 2, 1.0f, "look.z");
    checkf(&m, 1, 3, 0.0f, "translate.y");
    checkf(&m, 2, 3, -5.0f, "translate.z (column)");
    checkf(&m, 3, 3, 1.0f, "m[3][3]=1");

    printf("\n%s\n", g_fail ? "FAILURES" : "ALL PASSED");
    return g_fail;
}
