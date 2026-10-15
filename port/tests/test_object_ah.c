// Host-side integration test: run the real extracted object_ah display lists
// through the interpreter and check that geometry and textures come out.
//
// The assets and the interpreter both assume 32-bit pointers, so this test is
// built with -m32 (only available in CI / on x86-64 hosts with multilib). It
// needs no low-memory mmap because the asset's data really is 32-bit static.

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include "gfx/gfx.h"
#include "object_ah.h"

static GSGLOBAL g_gs = { .Width = 640, .Height = 448 };

static int g_tris = 0;
static int g_ttris = 0;
static int g_uploads = 0;

GSGLOBAL *gsKit_init_global(void) { return &g_gs; }

void gsKit_prim_triangle_3d(GSGLOBAL *g, float x1, float y1, int z1,
                            float x2, float y2, int z2,
                            float x3, float y3, int z3, u64 color)
{
    (void)g; (void)x1; (void)y1; (void)z1; (void)x2; (void)y2; (void)z2;
    (void)x3; (void)y3; (void)z3; (void)color;
    g_tris++;
}

void gsKit_prim_triangle_goraud_texture_3d(GSGLOBAL *g, GSTEXTURE *Texture,
                            float x1, float y1, int z1, float u1, float v1,
                            float x2, float y2, int z2, float u2, float v2,
                            float x3, float y3, int z3, float u3, float v3,
                            u64 color1, u64 color2, u64 color3)
{
    (void)g; (void)Texture;
    (void)x1; (void)y1; (void)z1; (void)x2; (void)y2; (void)z2;
    (void)x3; (void)y3; (void)z3;
    (void)u1; (void)v1; (void)u2; (void)v2; (void)u3; (void)v3;
    (void)color1; (void)color2; (void)color3;
    g_ttris++;
}

int gsKit_texture_size(int width, int height, u32 psm)
{
    (void)psm;
    return width * height * 4;
}

u32 gsKit_vram_alloc(GSGLOBAL *g, u32 bytes, u8 type)
{
    (void)g; (void)bytes; (void)type;
    return 0x1000;
}

void gsKit_texture_upload(GSGLOBAL *g, GSTEXTURE *Texture)
{
    (void)g; (void)Texture;
    g_uploads++;
}

static int g_fail = 0;
static void check(int cond, const char *what)
{
    printf("  [%s] %s\n", cond ? "ok" : "FAIL", what);
    if (!cond)
        g_fail = 1;
}

int main(void)
{
    Mmps2Gfx g;
    void **seg = object_ah_Skel_009E70.sh.segment;
    int limbCount = object_ah_Skel_009E70.sh.limbCount;
    int i;

    mmps2_gfx_init(&g, &g_gs);

    printf("object_ah: %d limbs\n", limbCount);

    for (i = 0; i < limbCount; i++) {
        const StandardLimb *limb = (const StandardLimb *)seg[i];
        if (limb == NULL || limb->dList == NULL)
            continue;
        printf("  limb %d: dList=%p tris=%d ttris=%d up=%d\n", i,
               (void *)limb->dList, g_tris, g_ttris, g_uploads);
        fflush(stdout);
        mmps2_gfx_run(&g, limb->dList);
    }

    printf("  plain triangles:   %d\n", g_tris);
    printf("  textured triangles: %d\n", g_ttris);
    printf("  texture uploads:   %d\n", g_uploads);

    check(g_tris + g_ttris > 100, "geometry produced");
    check(g_ttris > 0, "textured geometry produced");
    check(g_uploads > 0, "textures decoded and uploaded");
    check(g_uploads <= MMPS2_TEX_CACHE, "uploads fit the cache");

    printf("\n%s\n", g_fail ? "FAILURES" : "ALL PASSED");
    return g_fail;
}
