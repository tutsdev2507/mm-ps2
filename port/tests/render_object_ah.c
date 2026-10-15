// Host-side "renderer" harness: pose object_ah exactly as port/src/main.c does
// and dump every emitted triangle (screen-space coords, z, colour and, for
// textured prims, the texture pointer + u/v) as text. A companion script
// (port/tests/render_ppm.py) rasterises the dump to a PNG so the geometry can
// be inspected without a PS2.
//
// Needs 32-bit pointers (the asset stores (unsigned int)&sym in its display
// lists), so build with -m32. See port/tests/render_object_ah.sh.
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/gfx.h"
#include "object_ah.h"

static GSGLOBAL g_gs = { .Width = 640, .Height = 448 };

#define MAXTRI 20000
typedef struct {
    int kind; // 0 = flat, 1 = textured
    float x[3], y[3];
    int z[3];
    unsigned int rgba[3];
    const void *tex;
    float u[3], v[3];
    int limb;
} Tri;
static Tri g_out[MAXTRI];
static int g_nout;
static int g_curLimb;
static float g_rot = 0.7f;

GSGLOBAL *gsKit_init_global(void) { return &g_gs; }

static void push_common(int kind, float x1, float y1, int z1, float x2, float y2,
                        int z2, float x3, float y3, int z3)
{
    Tri *t;
    if (g_nout >= MAXTRI)
        return;
    t = &g_out[g_nout++];
    memset(t, 0, sizeof(*t));
    t->kind = kind;
    t->x[0] = x1; t->y[0] = y1; t->z[0] = z1;
    t->x[1] = x2; t->y[1] = y2; t->z[1] = z2;
    t->x[2] = x3; t->y[2] = y3; t->z[2] = z3;
}

void gsKit_prim_triangle_3d(GSGLOBAL *g, float x1, float y1, int z1,
                            float x2, float y2, int z2,
                            float x3, float y3, int z3, u64 color)
{
    (void)g;
    push_common(0, x1, y1, z1, x2, y2, z2, x3, y3, z3);
    g_out[g_nout - 1].rgba[0] = (unsigned int)color;
    g_out[g_nout - 1].limb = g_curLimb;
}

void gsKit_prim_triangle_goraud_texture_3d(GSGLOBAL *g, GSTEXTURE *Texture,
                            float x1, float y1, int z1, float u1, float v1,
                            float x2, float y2, int z2, float u2, float v2,
                            float x3, float y3, int z3, float u3, float v3,
                            u64 color1, u64 color2, u64 color3)
{
    (void)g;
    push_common(1, x1, y1, z1, x2, y2, z2, x3, y3, z3);
    g_out[g_nout - 1].tex = Texture;
    g_out[g_nout - 1].u[0] = u1; g_out[g_nout - 1].v[0] = v1;
    g_out[g_nout - 1].u[1] = u2; g_out[g_nout - 1].v[1] = v2;
    g_out[g_nout - 1].u[2] = u3; g_out[g_nout - 1].v[2] = v3;
    g_out[g_nout - 1].rgba[0] = (unsigned int)color1;
    g_out[g_nout - 1].rgba[1] = (unsigned int)color2;
    g_out[g_nout - 1].rgba[2] = (unsigned int)color3;
    g_out[g_nout - 1].limb = g_curLimb;
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
}

// --- pose, copied from port/src/main.c --------------------------------------
#define MAX_LIMB 16
static Mtx s_proj, s_view, s_rot;
static Mtx s_limbMtx[MAX_LIMB];
static int s_slotOf[64];

static void build_limbs(void **seg, int index, const Mtx *parent, int depth, int *slot)
{
    const StandardLimb *limb;
    Mtx local, tmp;

    if (index == 0xFF || index < 0 || index >= 64 || depth > 32)
        return;
    limb = (const StandardLimb *)seg[index];

    if (depth == 0)
        mmps2_mtx_identity(&tmp);
    else
        mmps2_mtx_translate(&tmp, (float)limb->jointPos.x,
                            (float)limb->jointPos.y, (float)limb->jointPos.z);
    mmps2_mtx_multiply(&local, parent, &tmp);

    s_slotOf[index] = -1;
    if (limb->dList != NULL && *slot < MAX_LIMB) {
        s_slotOf[index] = *slot;
        s_limbMtx[*slot] = local;
        (*slot)++;
    }
    if (limb->child != 0xFF)
        build_limbs(seg, limb->child, &local, depth + 1, slot);
    if (limb->sibling != 0xFF)
        build_limbs(seg, limb->sibling, parent, depth + 1, slot);
}

static void draw_limbs(Mmps2Gfx *gfx, void **seg, int index, int depth)
{
    const StandardLimb *limb;
    if (index == 0xFF || index < 0 || index >= 64 || depth > 32)
        return;
    limb = (const StandardLimb *)seg[index];
    if (limb->dList != NULL && s_slotOf[index] >= 0) {
        g_curLimb = index;
        mmps2_gfx_set_modelview(gfx, &s_limbMtx[s_slotOf[index]]);
        mmps2_gfx_run(gfx, limb->dList);
    }
    if (limb->child != 0xFF)
        draw_limbs(gfx, seg, limb->child, depth + 1);
    if (limb->sibling != 0xFF)
        draw_limbs(gfx, seg, limb->sibling, depth + 1);
}

int main(void)
{
    Mmps2Gfx g;
    void **seg = object_ah_Skel_009E70.sh.segment;
    int slot = 0;
    int i;

    {
        const char *e = getenv("MM_ROT");
        if (e)
            g_rot = (float)atof(e);
    }

    mmps2_gfx_init(&g, &g_gs);
    mmps2_mtx_perspective(&s_proj, 45.0f * 3.14159265f / 180.0f,
                          640.0f / 448.0f, 10.0f, 20000.0f);
    mmps2_gfx_set_projection(&g, &s_proj);

    mmps2_mtx_rotate_y(&s_rot, g_rot);
    mmps2_mtx_translate(&s_view, 0.0f, -1300.0f, -6000.0f);
    mmps2_mtx_multiply(&s_view, &s_view, &s_rot);

    build_limbs(seg, 0, &s_view, 0, &slot);
    g.seg[0x0D] = (u32)(uintptr_t)s_limbMtx;
    draw_limbs(&g, seg, 0, 0);

    printf("# limb -> slot, translation (intpart, 16.16)\n");
    {
        int l;
        for (l = 1; l < 16; l++) {
            if (s_slotOf[l] >= 0) {
                const Mtx *m = &s_limbMtx[s_slotOf[l]];
                printf("# limb %2d slot %2d t=(%d.%04x %d.%04x %d.%04x)\n", l,
                       s_slotOf[l],
                       (int)(s16)m->intPart[0][3], (unsigned)m->fracPart[0][3],
                       (int)(s16)m->intPart[1][3], (unsigned)m->fracPart[1][3],
                       (int)(s16)m->intPart[2][3], (unsigned)m->fracPart[2][3]);
            }
        }
    }
    printf("# %d triangles\n", g_nout);
    for (i = 0; i < g_nout; i++) {
        Tri *t = &g_out[i];
        if (t->kind == 0) {
            printf("F %d %d %d %d %d %d %d %d %d %06x %d\n",
                   (int)(t->x[0] * 16), (int)(t->y[0] * 16), t->z[0],
                   (int)(t->x[1] * 16), (int)(t->y[1] * 16), t->z[1],
                   (int)(t->x[2] * 16), (int)(t->y[2] * 16), t->z[2],
                   t->rgba[0] & 0xFFFFFF, t->limb);
        } else {
            printf("T %d %d %d %d %d %d %d %d %d %06x %d %d %d %d %d %d %d %d\n",
                   (int)(t->x[0] * 16), (int)(t->y[0] * 16), t->z[0],
                   (int)(t->x[1] * 16), (int)(t->y[1] * 16), t->z[1],
                   (int)(t->x[2] * 16), (int)(t->y[2] * 16), t->z[2],
                   t->rgba[0] & 0xFFFFFF,
                   (int)(unsigned long)t->tex,
                   (int)(t->u[0] * 16), (int)(t->v[0] * 16),
                   (int)(t->u[1] * 16), (int)(t->v[1] * 16),
                   (int)(t->u[2] * 16), (int)(t->v[2] * 16), t->limb);
        }
    }
    return 0;
}
