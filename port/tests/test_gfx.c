// Host-side logic tests for the F3DEX2 display-list interpreter.
//
// The interpreter and the gs* authoring macros both assume 32-bit pointers
// (true on the EE). On a 64-bit host the pointer stored in Gfx.w1 would
// truncate, so we place every display list / vertex / matrix in low
// (32-bit-addressable) memory via a fixed mmap.
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#include "gfx/gfx.h"

static GSGLOBAL g_gs = { .Width = 640, .Height = 448 };

GSGLOBAL *gsKit_init_global(void) { return &g_gs; }

// --- capture emitted triangles -------------------------------------------
#define MAX_TRI 16
typedef struct {
    float x1, y1, x2, y2, x3, y3;
    u64 color;
} Tri;

static Tri g_tris[MAX_TRI];
static int g_ntris = 0;

void gsKit_prim_triangle_3d(GSGLOBAL *g, float x1, float y1, int z1,
                            float x2, float y2, int z2,
                            float x3, float y3, int z3, u64 color)
{
    (void)g; (void)z1; (void)z2; (void)z3;
    if (g_ntris < MAX_TRI) {
        g_tris[g_ntris].x1 = x1; g_tris[g_ntris].y1 = y1;
        g_tris[g_ntris].x2 = x2; g_tris[g_ntris].y2 = y2;
        g_tris[g_ntris].x3 = x3; g_tris[g_ntris].y3 = y3;
        g_tris[g_ntris].color = color;
        g_ntris++;
    }
}

static int g_fail = 0;
static void check(int cond, const char *what)
{
    printf("  [%s] %s\n", cond ? "ok" : "FAIL", what);
    if (!cond)
        g_fail = 1;
}
static int near(float a, float b) { float d = a - b; return d < 0.5f && d > -0.5f; }

// --- low memory layout ----------------------------------------------------
#define BASE   0x10000000u
#define VERTS  (BASE + 0x1000)
#define PROJ   (BASE + 0x2000)
#define MV     (BASE + 0x3000)
#define DL     (BASE + 0x4000)
#define VP     (BASE + 0x6000)

static void fill_quad(Vtx *v)
{
    memset(v, 0, 4 * sizeof(Vtx));
    v[0].ob[0] = 220; v[0].ob[1] = 140;
    v[1].ob[0] = 420; v[1].ob[1] = 140;
    v[2].ob[0] = 420; v[2].ob[1] = 300;
    v[3].ob[0] = 220; v[3].ob[1] = 300;
}

static void base_proj(void)
{
    mmps2_mtx_ortho((Mtx *)PROJ, 0.0f, 640.0f, 448.0f, 0.0f, -1.0f, 1.0f);
}

int main(void)
{
    void *low = mmap((void *)(uintptr_t)BASE, 0x10000, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (low == MAP_FAILED) {
        perror("mmap low");
        return 2;
    }

    Mmps2Gfx g;
    int i;

    fill_quad((Vtx *)VERTS);
    base_proj();

    // 1. TRI1: a quad drawn as two triangles.
    {
        Gfx *dl = (Gfx *)DL;
        i = 0;
        dl[i++] = gsSPMatrix((Mtx *)PROJ, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        dl[i++] = gsSPVertex((Vtx *)VERTS, 4, 0);
        dl[i++] = gsDPSetPrimColor(0, 0, 0x20, 0xC0, 0xFF, 0xFF);
        dl[i++] = gsSP1Triangle(0, 1, 2, 0);
        dl[i++] = gsSP1Triangle(0, 2, 3, 0);
        dl[i++] = gsSPEndDisplayList();
        g_ntris = 0;
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("1. TRI1 quad\n");
        check(g_ntris == 2, "2 triangles emitted");
        check(near(g_tris[0].x1, 220.2f) && near(g_tris[0].y1, 140.2f), "tri1 v0 = (220,140)");
        check(near(g_tris[1].x3, 220.2f) && near(g_tris[1].y3, 300.4f), "tri2 v2 = (220,300)");
        check(g_tris[0].color == 0xFFFFC020ull, "prim color applied");
    }

    // 2. TRI2 (gsSP2Triangles).
    {
        Gfx *dl = (Gfx *)DL;
        i = 0;
        dl[i++] = gsSPMatrix((Mtx *)PROJ, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        dl[i++] = gsSPVertex((Vtx *)VERTS, 4, 0);
        dl[i++] = gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0);
        dl[i++] = gsSPEndDisplayList();
        g_ntris = 0;
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("2. TRI2\n");
        check(g_ntris == 2, "2 triangles emitted");
    }

    // 3. QUAD (gsSP1Quadrangle).
    {
        Gfx *dl = (Gfx *)DL;
        i = 0;
        dl[i++] = gsSPMatrix((Mtx *)PROJ, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        dl[i++] = gsSPVertex((Vtx *)VERTS, 4, 0);
        dl[i++] = gsSP1Quadrangle(0, 1, 2, 3, 0);
        dl[i++] = gsSPEndDisplayList();
        g_ntris = 0;
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("3. QUAD\n");
        check(g_ntris == 2, "2 triangles emitted");
        check(near(g_tris[0].x1, 220.2f) && near(g_tris[0].y1, 140.2f), "quad starts at v0");
    }

    // 4. Viewport: a +100px horizontal translate (vtrans has 2 frac bits).
    {
        Vp *vp = (Vp *)VP;
        Gfx *dl = (Gfx *)DL;
        memset(vp, 0, sizeof(*vp));
        vp->vscale[0] = (s16)(g_gs.Width * 2);
        vp->vscale[1] = (s16)(-g_gs.Height * 2);
        vp->vscale[2] = 0x7FFF;
        vp->vtrans[0] = (s16)(g_gs.Width * 2 + 400);
        vp->vtrans[1] = (s16)(g_gs.Height * 2);
        i = 0;
        dl[i++] = gsSPViewport(vp);
        dl[i++] = gsSPMatrix((Mtx *)PROJ, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        dl[i++] = gsSPVertex((Vtx *)VERTS, 4, 0);
        dl[i++] = gsSP1Triangle(0, 1, 2, 0);
        dl[i++] = gsSPEndDisplayList();
        g_ntris = 0;
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("4. Viewport\n");
        check(g_ntris == 1, "1 triangle emitted");
        check(near(g_tris[0].x1, 320.2f), "translated +100px");
    }

    // 5. Segments: reference vertices via segment 1 instead of a raw pointer.
    {
        Gfx *dl = (Gfx *)DL;
        u32 segaddr = 0x01000000u | (VERTS - BASE);
        i = 0;
        dl[i++] = gsSPSegment(1, BASE);
        dl[i++] = gsSPMatrix((Mtx *)PROJ, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        dl[i++] = (Gfx){ (_SHIFTL(G_VTX, 24, 8) | _SHIFTL(4, 12, 8) | _SHIFTL(0 + 4, 1, 7)), segaddr };
        dl[i++] = gsSP1Triangle(0, 1, 2, 0);
        dl[i++] = gsSPEndDisplayList();
        g_ntris = 0;
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("5. Segment addressing\n");
        check(g_ntris == 1, "1 triangle emitted");
        check(near(g_tris[0].x1, 220.2f) && near(g_tris[0].y1, 140.2f), "segment-resolved vertex");
    }

    // 6. Geometry mode set/clear semantics.
    {
        Gfx *dl = (Gfx *)DL;
        i = 0;
        dl[i++] = gsSPClearGeometryMode(0xFFFFFFFFu); // geom = 0
        dl[i++] = gsSPSetGeometryMode(G_LIGHTING);
        dl[i++] = gsSPEndDisplayList();
        mmps2_gfx_init(&g, &g_gs);
        g.geom = 0xFFFF;
        mmps2_gfx_run(&g, dl);
        printf("6. Geometry mode\n");
        check(g.geom == G_LIGHTING, "clear-then-set");
    }

    // 7. Modelview push/pop: second triangle is drawn untranslated.
    {
        Mtx *mv = (Mtx *)MV;
        Gfx *dl = (Gfx *)DL;
        mmps2_mtx_identity(mv);
        mv->intPart[0][3] = 0x0064; // +100.0 in 16.16
        mv->fracPart[0][3] = 0x0000;
        i = 0;
        dl[i++] = gsSPMatrix((Mtx *)PROJ, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        dl[i++] = gsSPVertex((Vtx *)VERTS, 4, 0);
        dl[i++] = gsSPMatrix(mv, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH);
        dl[i++] = gsSP1Triangle(0, 1, 2, 0);
        dl[i++] = gsSPPopMatrix(1);
        dl[i++] = gsSP1Triangle(0, 1, 2, 0);
        dl[i++] = gsSPEndDisplayList();
        g_ntris = 0;
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("7. Modelview push/pop\n");
        check(g_ntris == 2, "2 triangles emitted");
        check(near(g_tris[0].x1, 320.2f), "pushed matrix translates +100px");
        check(near(g_tris[1].x1, 220.2f), "pop restores identity");
    }

    // 8. Perspective: a point on the view axis projects to screen centre.
    {
        Vtx *v = (Vtx *)VERTS;
        Mtx *mv = (Mtx *)MV;
        Gfx *dl = (Gfx *)DL;
        memset(v, 0, 3 * sizeof(Vtx)); // three vertices at the origin
        mmps2_mtx_perspective((Mtx *)PROJ, 45.0f * 3.14159265f / 180.0f,
                              640.0f / 448.0f, 1.0f, 100.0f);
        mmps2_mtx_translate(mv, 0.0f, 0.0f, -4.0f);
        i = 0;
        dl[i++] = gsSPMatrix((Mtx *)PROJ, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        dl[i++] = gsSPMatrix(mv, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        dl[i++] = gsSPVertex(v, 3, 0);
        dl[i++] = gsSP1Triangle(0, 1, 2, 0);
        dl[i++] = gsSPEndDisplayList();
        g_ntris = 0;
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("8. Perspective projection\n");
        check(g_ntris == 1, "triangle emitted");
        check(near(g_tris[0].x1, 320.0f) && near(g_tris[0].y1, 224.0f),
              "on-axis point -> screen centre");
    }

    // 9. Matrix multiply: R * I == R and I * T == T.
    {
        Mtx a, b, c;
        int k, ok = 1;
        mmps2_mtx_rotate_y(&a, 0.5f);
        mmps2_mtx_identity(&b);
        mmps2_mtx_multiply(&c, &a, &b);
        for (k = 0; k < 4 && ok; k++) {
            int j;
            for (j = 0; j < 4; j++)
                if (c.intPart[k][j] != a.intPart[k][j] ||
                    c.fracPart[k][j] != a.fracPart[k][j])
                    ok = 0;
        }
        printf("9. Matrix multiply\n");
        check(ok, "R * I == R");
    }

    printf("\n%s (%d tri captures)\n", g_fail ? "FAILURES" : "ALL PASSED", g_ntris);
    return g_fail;
}
