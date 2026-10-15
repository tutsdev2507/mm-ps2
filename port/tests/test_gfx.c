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

// --- capture emitted textured triangles ----------------------------------
#define MAX_TTRI 8
typedef struct {
    float u1, v1, u2, v2, u3, v3;
    const GSTEXTURE *tex;
    u64 color;
} TT;

static TT g_ttris[MAX_TTRI];
static int g_nttris = 0;
static int g_uploads = 0;

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

void gsKit_prim_triangle_goraud_texture_3d(GSGLOBAL *g, GSTEXTURE *Texture,
                            float x1, float y1, int z1, float u1, float v1,
                            float x2, float y2, int z2, float u2, float v2,
                            float x3, float y3, int z3, float u3, float v3,
                            u64 color1, u64 color2, u64 color3)
{
    (void)g; (void)z1; (void)z2; (void)z3;
    (void)x1; (void)y1; (void)x2; (void)y2; (void)x3; (void)y3;
    (void)color2; (void)color3;
    if (g_nttris < MAX_TTRI) {
        TT *t = &g_ttris[g_nttris++];
        t->u1 = u1; t->v1 = v1; t->u2 = u2; t->v2 = v2; t->u3 = u3; t->v3 = v3;
        t->tex = Texture;
        t->color = color1;
    }
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
static int near(float a, float b) { float d = a - b; return d < 0.5f && d > -0.5f; }

// --- low memory layout ----------------------------------------------------
#define BASE   0x10000000u
#define VERTS  (BASE + 0x1000)
#define PROJ   (BASE + 0x2000)
#define MV     (BASE + 0x3000)
#define DL     (BASE + 0x4000)
#define VP     (BASE + 0x6000)
#define PAL    (BASE + 0x8000)
#define IMG    (BASE + 0x9000)

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

    // 7. Vertices transform on load: the modelview in effect at gsSPVertex is
    //    what positions them (the RSP transforms a vertex when it loads it), so
    //    loading under the pushed matrix and again after the pop gives +100
    //    then identity for the same source vertices.
    {
        Mtx *mv = (Mtx *)MV;
        Gfx *dl = (Gfx *)DL;
        mmps2_mtx_identity(mv);
        mv->intPart[0][3] = 0x0064; // +100.0 in 16.16
        mv->fracPart[0][3] = 0x0000;
        i = 0;
        dl[i++] = gsSPMatrix((Mtx *)PROJ, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        dl[i++] = gsSPMatrix(mv, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH);
        dl[i++] = gsSPVertex((Vtx *)VERTS, 4, 0);
        dl[i++] = gsSP1Triangle(0, 1, 2, 0);
        dl[i++] = gsSPPopMatrix(1);
        dl[i++] = gsSPVertex((Vtx *)VERTS, 4, 0);
        dl[i++] = gsSP1Triangle(0, 1, 2, 0);
        dl[i++] = gsSPEndDisplayList();
        g_ntris = 0;
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("7. Vertices transform on load (modelview push/pop)\n");
        check(g_ntris == 2, "2 triangles emitted");
        check(near(g_tris[0].x1, 320.2f), "loaded under pushed matrix +100px");
        check(near(g_tris[1].x1, 220.2f), "reloaded under identity after pop");
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

    // 10. Texture decode: RGBA16 (R5G5B5A1) -> CT32 (A8B8G8R8).
    {
        u16 src[4] = { 0xFFFF, 0x0001, 0xF800, 0x07C0 };
        u32 dst[4];
        mmps2_tex_decode(dst, G_IM_FMT_RGBA, G_IM_SIZ_16b, 2, 2, src, NULL);
        printf("10. RGBA16 decode\n");
        check(dst[0] == 0xFFFFFFFFu, "white");
        check(dst[1] == 0xFF000000u, "opaque black");
        check(dst[2] == 0x000000FFu, "pure red, transparent");
        check(dst[3] == 0x0000FF00u, "pure green");
    }

    // 11. Texture decode: CI8 through an RGBA16 palette.
    {
        u16 pal[256];
        u8 src[4] = { 0, 1, 2, 0 };
        u32 dst[4];
        memset(pal, 0, sizeof(pal));
        pal[1] = 0xF800; // red
        pal[2] = 0x003F; // opaque blue
        mmps2_tex_decode(dst, G_IM_FMT_CI, G_IM_SIZ_8b, 4, 1, src, pal);
        printf("11. CI8 decode\n");
        check(dst[0] == 0x00000000u, "index 0 (transparent)");
        check(dst[1] == 0x000000FFu, "index 1 -> red");
        check(dst[2] == 0xFFFF0000u, "index 2 -> opaque blue");
    }

    // 12. Texture decode: IA16 and I8 grayscale.
    {
        u16 ia[2] = { 0xFF80, 0x0080 };
        u8 i8[2] = { 0x80, 0x00 };
        u32 dst[2];
        mmps2_tex_decode(dst, G_IM_FMT_IA, G_IM_SIZ_16b, 2, 1, ia, NULL);
        printf("12. IA/I decode\n");
        check(dst[0] == 0x80FFFFFFu, "IA16 white, alpha 128");
        mmps2_tex_decode(dst, G_IM_FMT_I, G_IM_SIZ_8b, 2, 1, i8, NULL);
        check(dst[0] == 0xFF808080u, "I8 grey 128 opaque");
        check(dst[1] == 0xFF000000u, "I8 black opaque");
    }

    // 13. Textured triangle: CI8 tile state drives an upload + textured draw.
    {
        u16 *pal = (u16 *)PAL;
        u8 *img = (u8 *)IMG;
        Vtx *v = (Vtx *)VERTS;
        Gfx *dl = (Gfx *)DL;
        memset(pal, 0, 512);
        pal[1] = 0xF800;
        memset(img, 1, 256);

        i = 0;
        dl[i++] = gsSPMatrix((Mtx *)PROJ, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        // palette: SETTIMG(RGBA/16b/w=1) + SETTILE(load tile 7) + LOADTLUT(256)
        dl[i++] = (Gfx){ _SHIFTL(G_SETTIMG, 24, 8) | _SHIFTL(0, 21, 3) |
                         _SHIFTL(2, 19, 2), (u32)PAL };
        dl[i++] = (Gfx){ _SHIFTL(G_SETTILE, 24, 8) | _SHIFTL(256, 0, 9),
                         _SHIFTL(7, 24, 3) };
        dl[i++] = (Gfx){ _SHIFTL(G_LOADTLUT, 24, 8),
                         _SHIFTL(7, 24, 3) | _SHIFTL(255, 14, 10) };
        // CI8 image: SETTIMG(CI/8b) + SETTILE(render tile 0) + SETTILESIZE
        dl[i++] = (Gfx){ _SHIFTL(G_SETTIMG, 24, 8) | _SHIFTL(2, 21, 3) |
                         _SHIFTL(1, 19, 2), (u32)IMG };
        dl[i++] = (Gfx){ _SHIFTL(G_SETTILE, 24, 8) | _SHIFTL(2, 21, 3) |
                         _SHIFTL(1, 19, 2), _SHIFTL(0, 24, 3) };
        dl[i++] = (Gfx){ _SHIFTL(G_SETTILESIZE, 24, 8),
                         _SHIFTL(0, 24, 3) | _SHIFTL(15 * 4, 12, 12) |
                         _SHIFTL(15 * 4, 0, 12) };
        dl[i++] = gsSPTexture(0xFFFF, 0xFFFF, 0, 0, G_ON);
        dl[i++] = gsSPSetGeometryMode(G_SHADE);
        dl[i++] = gsSPVertex(v, 4, 0);
        dl[i++] = gsSP1Triangle(0, 1, 2, 0);
        dl[i++] = gsSPEndDisplayList();

        g_ntris = 0;
        g_nttris = 0;
        g_uploads = 0;
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("13. Textured triangle\n");
        check(g_nttris == 1, "1 textured triangle emitted");
        check(g_uploads == 1, "texture uploaded once");
        check(g_ttris[0].tex != NULL, "texture bound");
        check(g_ttris[0].tex->PSM == GS_PSM_CT32, "uploaded as CT32");
    }

    // 14. Combiner: G_CC_MODULATEIA with an opaque-white texel reduces to SHADE.
    {
        Vtx *v = (Vtx *)VERTS;
        Gfx *dl = (Gfx *)DL;
        fill_quad(v);
        for (i = 0; i < 4; i++) {
            v[i].cn[0] = 100; v[i].cn[1] = 150; v[i].cn[2] = 200; v[i].cn[3] = 255;
        }
        i = 0;
        dl[i++] = gsSPMatrix((Mtx *)PROJ, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        dl[i++] = gsSPSetGeometryMode(G_SHADE); // not lit
        dl[i++] = gsDPSetCombineLERP(G_CCMUX_TEXEL0, G_CCMUX_0, G_CCMUX_SHADE, G_CCMUX_0,
                                     G_ACMUX_TEXEL0, G_ACMUX_0, G_ACMUX_SHADE, G_ACMUX_0,
                                     G_CCMUX_TEXEL0, G_CCMUX_0, G_CCMUX_SHADE, G_CCMUX_0,
                                     G_ACMUX_TEXEL0, G_ACMUX_0, G_ACMUX_SHADE, G_ACMUX_0);
        dl[i++] = gsSPVertex(v, 4, 0);
        dl[i++] = gsSP1Triangle(0, 1, 2, 0);
        dl[i++] = gsSPEndDisplayList();
        g_ntris = 0;
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("14. Combiner MODULATEIA (texel=white)\n");
        check(g_ntris == 1, "1 triangle emitted");
        check(g_tris[0].color == 0xFFC89664ull, "result == shade (100,150,200)");
    }

    // 15. Combiner: pure SHADE (no texture anywhere).
    {
        Vtx *v = (Vtx *)VERTS;
        Gfx *dl = (Gfx *)DL;
        fill_quad(v);
        for (i = 0; i < 4; i++) {
            v[i].cn[0] = 10; v[i].cn[1] = 20; v[i].cn[2] = 30; v[i].cn[3] = 255;
        }
        i = 0;
        dl[i++] = gsSPMatrix((Mtx *)PROJ, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        dl[i++] = gsDPSetCombineLERP(G_CCMUX_0, G_CCMUX_0, G_CCMUX_0, G_CCMUX_SHADE,
                                     G_ACMUX_0, G_ACMUX_0, G_ACMUX_0, G_ACMUX_SHADE,
                                     G_CCMUX_0, G_CCMUX_0, G_CCMUX_0, G_CCMUX_SHADE,
                                     G_ACMUX_0, G_ACMUX_0, G_ACMUX_0, G_ACMUX_SHADE);
        dl[i++] = gsSPVertex(v, 4, 0);
        dl[i++] = gsSP1Triangle(0, 1, 2, 0);
        dl[i++] = gsSPEndDisplayList();
        g_ntris = 0;
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("15. Combiner SHADE only\n");
        check(g_ntris == 1, "1 triangle emitted");
        check(!mmps2_combiner_uses_texture(&g), "combiner does not reference texels");
        check(g_tris[0].color == 0xFF1E140Aull, "result == shade (10,20,30)");
    }

    // 16. Combiner: pure PRIMITIVE.
    {
        Vtx *v = (Vtx *)VERTS;
        Gfx *dl = (Gfx *)DL;
        fill_quad(v);
        for (i = 0; i < 4; i++) {
            v[i].cn[0] = 1; v[i].cn[1] = 2; v[i].cn[2] = 3; v[i].cn[3] = 255;
        }
        i = 0;
        dl[i++] = gsSPMatrix((Mtx *)PROJ, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        dl[i++] = gsDPSetCombineLERP(G_CCMUX_0, G_CCMUX_0, G_CCMUX_0, G_CCMUX_PRIMITIVE,
                                     G_ACMUX_0, G_ACMUX_0, G_ACMUX_0, G_ACMUX_PRIMITIVE,
                                     G_CCMUX_0, G_CCMUX_0, G_CCMUX_0, G_CCMUX_PRIMITIVE,
                                     G_ACMUX_0, G_ACMUX_0, G_ACMUX_0, G_ACMUX_PRIMITIVE);
        dl[i++] = gsDPSetPrimColor(0, 0, 200, 100, 50, 255);
        dl[i++] = gsSPVertex(v, 4, 0);
        dl[i++] = gsSP1Triangle(0, 1, 2, 0);
        dl[i++] = gsSPEndDisplayList();
        g_ntris = 0;
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("16. Combiner PRIMITIVE only\n");
        check(g_ntris == 1, "1 triangle emitted");
        check(g_tris[0].color == 0xFF3264C8ull, "result == prim (200,100,50)");
    }

    // 17. Combiner references TEXEL0 -> texturing is active.
    {
        u8 *img = (u8 *)IMG;
        Vtx *v = (Vtx *)VERTS;
        Gfx *dl = (Gfx *)DL;
        fill_quad(v);
        memset(img, 0xFF, 256);
        i = 0;
        dl[i++] = gsSPMatrix((Mtx *)PROJ, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        dl[i++] = (Gfx){ _SHIFTL(G_SETTIMG, 24, 8) | _SHIFTL(0, 21, 3) |
                         _SHIFTL(2, 19, 2), (u32)IMG };
        dl[i++] = (Gfx){ _SHIFTL(G_SETTILE, 24, 8) | _SHIFTL(0, 21, 3) |
                         _SHIFTL(2, 19, 2), _SHIFTL(0, 24, 3) };
        dl[i++] = (Gfx){ _SHIFTL(G_SETTILESIZE, 24, 8),
                         _SHIFTL(0, 24, 3) | _SHIFTL(15 * 4, 12, 12) |
                         _SHIFTL(15 * 4, 0, 12) };
        dl[i++] = gsDPSetCombineLERP(G_CCMUX_0, G_CCMUX_0, G_CCMUX_0, G_CCMUX_TEXEL0,
                                     G_ACMUX_0, G_ACMUX_0, G_ACMUX_0, G_ACMUX_TEXEL0,
                                     G_CCMUX_0, G_CCMUX_0, G_CCMUX_0, G_CCMUX_TEXEL0,
                                     G_ACMUX_0, G_ACMUX_0, G_ACMUX_0, G_ACMUX_TEXEL0);
        dl[i++] = gsSPTexture(0xFFFF, 0xFFFF, 0, 0, G_ON);
        dl[i++] = gsSPVertex(v, 4, 0);
        dl[i++] = gsSP1Triangle(0, 1, 2, 0);
        dl[i++] = gsSPEndDisplayList();
        g_ntris = 0; g_nttris = 0; g_uploads = 0;
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("17. Combiner TEXEL0 -> textured path\n");
        check(mmps2_combiner_uses_texture(&g), "combiner references TEXEL0");
        check(g_nttris == 1, "textured triangle emitted");
        check(g_ttris[0].color == 0xFFFFFFFFull, "texel=white -> vertex factor white");
    }

    // 18. Texture dimensions derived from G_SETTILESIZE (render tile).
    {
        u16 *pal = (u16 *)PAL;
        u8 *img = (u8 *)IMG;
        Gfx *dl = (Gfx *)DL;
        memset(pal, 0, 512);
        memset(img, 1, 64);
        i = 0;
        dl[i++] = (Gfx){ _SHIFTL(G_SETTIMG, 24, 8) | _SHIFTL(0, 21, 3) |
                         _SHIFTL(2, 19, 2), (u32)PAL };
        dl[i++] = (Gfx){ _SHIFTL(G_SETTILE, 24, 8) | _SHIFTL(256, 0, 9), _SHIFTL(7, 24, 3) };
        dl[i++] = (Gfx){ _SHIFTL(G_LOADTLUT, 24, 8),
                         _SHIFTL(7, 24, 3) | _SHIFTL(255, 14, 10) };
        dl[i++] = (Gfx){ _SHIFTL(G_SETTIMG, 24, 8) | _SHIFTL(2, 21, 3) |
                         _SHIFTL(1, 19, 2), (u32)IMG };
        dl[i++] = (Gfx){ _SHIFTL(G_SETTILE, 24, 8) | _SHIFTL(2, 21, 3) |
                         _SHIFTL(1, 19, 2), _SHIFTL(0, 24, 3) };
        dl[i++] = (Gfx){ _SHIFTL(G_SETTILESIZE, 24, 8),
                         _SHIFTL(0, 24, 3) | _SHIFTL(7 * 4, 12, 12) | _SHIFTL(7 * 4, 0, 12) };
        dl[i++] = gsSPEndDisplayList();
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("18. Texture dims from SETTILESIZE\n");
        check(g.texW == 8 && g.texH == 8, "8x8 from tile size");
    }

    // 19. Texture dims inferred from G_LOADBLOCK + render-tile line.
    {
        Gfx *dl = (Gfx *)DL;
        i = 0;
        // 16b RGBA 8x8: render tile line = ((8*2)+7)>>3 = 2.
        dl[i++] = (Gfx){ _SHIFTL(G_SETTIMG, 24, 8) | _SHIFTL(0, 21, 3) |
                         _SHIFTL(2, 19, 2), (u32)IMG };
        dl[i++] = (Gfx){ _SHIFTL(G_SETTILE, 24, 8) | _SHIFTL(0, 21, 3) |
                         _SHIFTL(2, 19, 2) | _SHIFTL(2, 9, 9), _SHIFTL(0, 24, 3) };
        dl[i++] = (Gfx){ _SHIFTL(G_LOADBLOCK, 24, 8),
                         _SHIFTL(7, 24, 3) | _SHIFTL(8 * 8 - 1, 12, 12) };
        dl[i++] = gsSPEndDisplayList();
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("19. Texture dims from LOADBLOCK fallback\n");
        check(g.texW == 8 && g.texH == 8, "8x8 inferred");
    }

    // 20. Two-cycle combiner: MODULATEIA twice scales shade by itself.
    {
        Vtx *v = (Vtx *)VERTS;
        Gfx *dl = (Gfx *)DL;
        fill_quad(v);
        for (i = 0; i < 4; i++) {
            v[i].cn[0] = 128; v[i].cn[1] = 128; v[i].cn[2] = 128; v[i].cn[3] = 255;
        }
        i = 0;
        dl[i++] = gsSPMatrix((Mtx *)PROJ, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        // cycle 0: TEXEL0*SHADE, cycle 1: COMBINED*SHADE
        dl[i++] = gsDPSetCombineLERP(G_CCMUX_TEXEL0, G_CCMUX_0, G_CCMUX_SHADE, G_CCMUX_0,
                                     G_ACMUX_TEXEL0, G_ACMUX_0, G_ACMUX_SHADE, G_ACMUX_0,
                                     G_CCMUX_COMBINED, G_CCMUX_0, G_CCMUX_SHADE, G_CCMUX_0,
                                     G_ACMUX_COMBINED, G_ACMUX_0, G_ACMUX_SHADE, G_ACMUX_0);
        dl[i++] = gsSPVertex(v, 4, 0);
        dl[i++] = gsSP1Triangle(0, 1, 2, 0);
        dl[i++] = gsSPEndDisplayList();
        g_ntris = 0;
        mmps2_gfx_init(&g, &g_gs);
        g.othermode_h |= (u32)(G_CYC_2CYCLE << G_MDSFT_CYCLETYPE);
        mmps2_gfx_run(&g, dl);
        printf("20. Two-cycle combiner\n");
        check(g_ntris == 1, "1 triangle emitted");
        // 128 -> 128*128/255 = 64
        check(g_tris[0].color == 0xFF404040ull, "shade squared == 64");
    }

    // 21. Flexible skeleton skinning: a display list loads one vertex batch
    //     under limb matrix A, then swaps to limb matrix B, loads a second
    //     batch, and finally draws a triangle mixing both batches. Each vertex
    //     must keep the matrix it was loaded under (the N64 RSP transforms on
    //     G_VTX); transforming at draw time would push the first batch under B
    //     as well and tear the mesh apart at the joint.
    {
        Mtx *mv = (Mtx *)MV;
        Vtx *v = (Vtx *)VERTS;
        Gfx *dl = (Gfx *)DL;
        fill_quad(v); // v0 (220,140) v1 (420,140) v2 (420,300) v3 (220,300)
        base_proj(); // an earlier test overwrote the projection buffer
        mmps2_mtx_translate(mv, 100.0f, 0.0f, 0.0f); // limb matrix B
        i = 0;
        dl[i++] = gsSPMatrix((Mtx *)PROJ, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        dl[i++] = gsSPVertex(v, 2, 0); // batch A: v0,v1 under identity (matrix A)
        dl[i++] = gsSPMatrix(mv, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        dl[i++] = gsSPVertex(v + 2, 2, 2); // batch B: v2,v3 under +100 (matrix B)
        dl[i++] = gsSP1Triangle(0, 1, 2, 0); // mixes batch A (0,1) with batch B (2)
        dl[i++] = gsSPEndDisplayList();
        g_ntris = 0;
        mmps2_gfx_init(&g, &g_gs);
        mmps2_gfx_run(&g, dl);
        printf("21. Flexible-skeleton skinning (matrix swap between batches)\n");
        check(g_ntris == 1, "1 triangle emitted");
        check(near(g_tris[0].x1, 220.2f), "batch A keeps matrix A (no +100 shift)");
        check(near(g_tris[0].x3, 520.2f), "batch B uses matrix B (+100 shift)");
    }

    printf("\n%s (%d tri captures)\n", g_fail ? "FAILURES" : "ALL PASSED", g_ntris);
    return g_fail;}
