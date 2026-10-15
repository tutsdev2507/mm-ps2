// F3DEX2 display-list interpreter -> gsKit (PlayStation 2 GS).
//
// This is the M2 milestone: walk a display list built with the gs* macros in
// gbi.h, keep the vertex buffer / matrix stacks / RDP state, and emit gsKit
// primitives for the triangles. It implements the subset needed to interpret
// real F3DEX2 object display lists (matrices, vertices, viewport, segments,
// triangles, colors); textured/combiner commands are recorded but not yet
// rasterised, and unknown commands are no-ops so lists still "run".
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gsKit.h>

#include "gfx.h"

#define MMPS2_DL_DEPTH_MAX 8

// --- fixed-point (16.16 split across intPart/fracPart) <-> float ---
static float mtx_get(const Mtx *m, int r, int c)
{
    u32 word = ((u32)m->intPart[r][c] << 16) | (u32)m->fracPart[r][c];
    return (float)(s32)word / 65536.0f;
}

static void mtx_set(Mtx *m, int r, int c, float v)
{
    float scaled = v * 65536.0f;
    s32 fixed = (s32)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
    m->intPart[r][c] = (u16)((u32)fixed >> 16);
    m->fracPart[r][c] = (u16)((u32)fixed & 0xFFFF);
}

static void mul4(Mtx *dst, const Mtx *a, const Mtx *b)
{
    int i, j, k;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            float s = 0.0f;
            for (k = 0; k < 4; k++)
                s += mtx_get(a, i, k) * mtx_get(b, k, j);
            mtx_set(dst, i, j, s);
        }
    }
}

void mmps2_mtx_multiply(Mtx *dst, const Mtx *a, const Mtx *b)
{
    mul4(dst, a, b);
}

void mmps2_mtx_identity(Mtx *m)
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            mtx_set(m, i, j, (i == j) ? 1.0f : 0.0f);
}

void mmps2_mtx_ortho(Mtx *m, float l, float r, float b, float t, float n, float f)
{
    mmps2_mtx_identity(m);
    mtx_set(m, 0, 0, 2.0f / (r - l));
    mtx_set(m, 1, 1, 2.0f / (t - b));
    mtx_set(m, 2, 2, -2.0f / (f - n));
    mtx_set(m, 0, 3, -(r + l) / (r - l));
    mtx_set(m, 1, 3, -(t + b) / (t - b));
    mtx_set(m, 2, 3, -(f + n) / (f - n));
}

// Standard GL-style perspective (clip.w = -z), matching the y-down viewport
// default set up in mmps2_gfx_init.
void mmps2_mtx_perspective(Mtx *m, float fovy, float aspect, float near, float far)
{
    float f = 1.0f / tanf(fovy * 0.5f);
    mmps2_mtx_identity(m);
    mtx_set(m, 0, 0, f / aspect);
    mtx_set(m, 1, 1, f);
    mtx_set(m, 2, 2, (far + near) / (near - far));
    mtx_set(m, 2, 3, (2.0f * far * near) / (near - far));
    mtx_set(m, 3, 2, -1.0f);
    mtx_set(m, 3, 3, 0.0f);
}

void mmps2_mtx_translate(Mtx *m, float x, float y, float z)
{
    mmps2_mtx_identity(m);
    mtx_set(m, 0, 3, x);
    mtx_set(m, 1, 3, y);
    mtx_set(m, 2, 3, z);
}

void mmps2_mtx_rotate_x(Mtx *m, float rad)
{
    float c = cosf(rad), s = sinf(rad);
    mmps2_mtx_identity(m);
    mtx_set(m, 1, 1, c);
    mtx_set(m, 1, 2, -s);
    mtx_set(m, 2, 1, s);
    mtx_set(m, 2, 2, c);
}

void mmps2_mtx_rotate_y(Mtx *m, float rad)
{
    float c = cosf(rad), s = sinf(rad);
    mmps2_mtx_identity(m);
    mtx_set(m, 0, 0, c);
    mtx_set(m, 0, 2, s);
    mtx_set(m, 2, 0, -s);
    mtx_set(m, 2, 2, c);
}

// Resolve an N64 address word. Extracted assets store two kinds of values:
//   * a 4-bit segment number in the high nibble (e.g. 0x0D000000) whose base is
//     installed with gsSPSegment(G_MWO_SEGMENT); and
//   * a complete host pointer to a static asset (e.g. 0x5656e970).
// A segment-style word whose segment is not currently loaded refers to data
// swapped in at runtime (animated textures, actor overlays); return NULL so the
// caller can skip it instead of dereferencing a bogus address.
static const void *resolve(const Mmps2Gfx *g, u32 w1)
{
    u32 top = (w1 >> 24) & 0xFF;
    u32 seg = (w1 >> 24) & 0x0F;
    if (g->seg[seg] != 0)
        return (const void *)(uintptr_t)(g->seg[seg] + (w1 & 0x00FFFFFF));
    if (top != 0 && top < 0x10)
        return NULL;
    return (const void *)(uintptr_t)w1;
}

// --- Combiner word packing (matches the F3DEX2 G_SETCOMBINE encoding) ---
// The F3DEX2 GBI packs a/b into 4 bits, c into 5 bits and d into 3 bits, and
// `_SHIFTL` masks each field to its width. The decomp's G_CCMUX_0 (=31) therefore
// truncates to 15 in a/b and 7 in d; the interpreter must invert that mapping.
#define CSH(v, s, w) ((((u32)(v)) & ((1u << (w)) - 1u)) << (s))

static u32 comb_w0(int a0, int c0, int Aa0, int Ac0, int a1, int c1)
{
    return CSH(a0, 20, 4) | CSH(c0, 15, 5) | CSH(Aa0, 12, 3) |
           CSH(Ac0, 9, 3) | CSH(a1, 5, 4) | CSH(c1, 0, 5);
}

static u32 comb_w1(int b0, int b1, int Aa1, int Ac1, int d0,
                   int Ab0, int Ad0, int d1, int Ab1, int Ad1)
{
    return CSH(b0, 28, 4) | CSH(b1, 24, 4) | CSH(Aa1, 21, 3) |
           CSH(Ac1, 18, 3) | CSH(d0, 15, 3) | CSH(Ab0, 12, 3) |
           CSH(Ad0, 9, 3) | CSH(d1, 6, 3) | CSH(Ab1, 3, 3) | CSH(Ad1, 0, 3);
}

void mmps2_gfx_init(Mmps2Gfx *g, GSGLOBAL *gs)
{
    memset(g, 0, sizeof(*g));
    g->gs = gs;
    g->width = gs->Width;
    g->height = gs->Height;
    g->r = g->g = g->b = 0xFF;
    g->a = 0x80;
    g->ea = 0xFF;
    g->texScale = 0x10000;
    g->filter = GS_FILTER_NEAREST;
    mmps2_mtx_identity(&g->proj[0]);
    mmps2_mtx_identity(&g->mv[0]);

    // Default combiner is G_CC_MODULATEIA (TEXEL0 * SHADE) so geometry drawn
    // before a G_SETCOMBINE still gets the conventional modulate behaviour.
    g->combine_hi = comb_w0(G_CCMUX_TEXEL0, G_CCMUX_SHADE, G_ACMUX_TEXEL0,
                            G_ACMUX_SHADE, G_CCMUX_0, G_CCMUX_0);
    g->combine_lo = comb_w1(G_CCMUX_0, G_CCMUX_0, G_ACMUX_0, G_ACMUX_0, G_CCMUX_0,
                            G_ACMUX_0, G_ACMUX_0, G_CCMUX_0, G_ACMUX_0, G_ACMUX_0);

    // Default viewport matching a y-down 640x448 surface in NDC.
    g->vp.vscale[0] = (s16)(gs->Width * 2);
    g->vp.vscale[1] = (s16)(-gs->Height * 2);
    g->vp.vscale[2] = 0x7FFF;
    g->vp.vtrans[0] = (s16)(gs->Width * 2);
    g->vp.vtrans[1] = (s16)(gs->Height * 2);
}

static void xform(const Mmps2Gfx *g, const Vtx *v, float *sx, float *sy, int *iz)
{
    const Mtx *mv = &g->mv[g->mvTop];
    const Mtx *pr = &g->proj[g->projTop];
    const Vp *vp = &g->vp;
    float x = (float)v->ob[0], y = (float)v->ob[1], z = (float)v->ob[2];
    float wx, wy, wz, cx, cy, cz, cw, invw, ndx, ndy;

    wx = mtx_get(mv, 0, 0) * x + mtx_get(mv, 0, 1) * y + mtx_get(mv, 0, 2) * z + mtx_get(mv, 0, 3);
    wy = mtx_get(mv, 1, 0) * x + mtx_get(mv, 1, 1) * y + mtx_get(mv, 1, 2) * z + mtx_get(mv, 1, 3);
    wz = mtx_get(mv, 2, 0) * x + mtx_get(mv, 2, 1) * y + mtx_get(mv, 2, 2) * z + mtx_get(mv, 2, 3);

    cx = mtx_get(pr, 0, 0) * wx + mtx_get(pr, 0, 1) * wy + mtx_get(pr, 0, 2) * wz + mtx_get(pr, 0, 3);
    cy = mtx_get(pr, 1, 0) * wx + mtx_get(pr, 1, 1) * wy + mtx_get(pr, 1, 2) * wz + mtx_get(pr, 1, 3);
    cz = mtx_get(pr, 2, 0) * wx + mtx_get(pr, 2, 1) * wy + mtx_get(pr, 2, 2) * wz + mtx_get(pr, 2, 3);
    cw = mtx_get(pr, 3, 0) * wx + mtx_get(pr, 3, 1) * wy + mtx_get(pr, 3, 2) * wz + mtx_get(pr, 3, 3);

    if (cw == 0.0f)
        cw = 1.0f;
    invw = 1.0f / cw;
    ndx = cx * invw;
    ndy = cy * invw;

    // vscale/vtrans carry 2 fractional bits.
    *sx = (ndx * (float)vp->vscale[0] + (float)vp->vtrans[0]) * 0.25f;
    *sy = (ndy * (float)vp->vscale[1] + (float)vp->vtrans[1]) * 0.25f;
    *iz = (int)((cz * invw * 0.5f + 0.5f) * 0x3FFFFF);
}

static void emit_triangle(Mmps2Gfx *g, int a, int b, int c);

// --- Texture decoding -------------------------------------------------------

static u32 expand5(u32 v)
{
    return (v << 3) | (v >> 2);
}

// N64 RGBA16 (R5G5B5A1) -> PS2 CT32 (A8B8G8R8, little-endian byte order A,B,G,R).
static u32 rgba16_to_ct32(u16 v)
{
    u32 r = expand5((v >> 11) & 0x1F);
    u32 g = expand5((v >> 6) & 0x1F);
    u32 b = expand5((v >> 1) & 0x1F);
    u32 a = (v & 1) ? 0xFFu : 0x00u;
    return (a << 24) | (b << 16) | (g << 8) | r;
}

static u32 gray_to_ct32(u32 i, u32 a)
{
    return (a << 24) | (i << 16) | (i << 8) | i;
}

void mmps2_tex_decode(u32 *dst, u32 fmt, u32 siz, u32 w, u32 h,
                      const void *src, const u16 *pal)
{
    u32 n = w * h;
    u32 i;

    if (!src)
        return;

    switch (fmt) {
    case G_IM_FMT_RGBA: {
        const u16 *s = (const u16 *)src;
        for (i = 0; i < n; i++)
            dst[i] = rgba16_to_ct32(s[i]);
        break;
    }
    case G_IM_FMT_CI: {
        const u8 *s = (const u8 *)src;
        if (siz == G_IM_SIZ_4b) {
            for (i = 0; i < n; i++) {
                u32 idx = (i & 1) ? (s[i >> 1] & 0x0F) : (s[i >> 1] >> 4);
                dst[i] = pal ? rgba16_to_ct32(pal[idx]) : gray_to_ct32(idx * 17, 0xFF);
            }
        } else {
            for (i = 0; i < n; i++) {
                u32 idx = s[i];
                dst[i] = pal ? rgba16_to_ct32(pal[idx]) : gray_to_ct32(idx, 0xFF);
            }
        }
        break;
    }
    case G_IM_FMT_IA: {
        if (siz == G_IM_SIZ_16b) {
            const u16 *s = (const u16 *)src;
            for (i = 0; i < n; i++)
                dst[i] = gray_to_ct32((s[i] >> 8) & 0xFF, s[i] & 0xFF);
        } else {
            const u8 *s = (const u8 *)src;
            for (i = 0; i < n; i++)
                dst[i] = gray_to_ct32(((s[i] >> 4) & 0xF) * 17, (s[i] & 0xF) * 17);
        }
        break;
    }
    case G_IM_FMT_I:
    default: {
        const u8 *s = (const u8 *)src;
        if (siz == G_IM_SIZ_4b) {
            for (i = 0; i < n; i++) {
                u32 idx = (i & 1) ? (s[i >> 1] & 0x0F) : (s[i >> 1] >> 4);
                dst[i] = gray_to_ct32(idx * 17, 0xFF);
            }
        } else {
            for (i = 0; i < n; i++)
                dst[i] = gray_to_ct32(s[i], 0xFF);
        }
        break;
    }
    }
}

static const GSTEXTURE *bind_texture(Mmps2Gfx *g)
{
    const void *img = g->texImg[G_TX_RENDERTILE];
    const u16 *pal = NULL;
    u32 i, w = g->texW, h = g->texH;

    if (!img || w == 0 || h == 0)
        return NULL;
    // Bail out on implausible tile dimensions rather than over-read the source.
    if (w > 1024 || h > 1024)
        return NULL;
    if (g->tileFmt == G_IM_FMT_CI && g->tlut) {
        if (g->tlutCount < 256)
            return NULL;
        pal = (const u16 *)g->tlut;
    }

    for (i = 0; i < (u32)g->texCacheCount; i++) {
        Mmps2TexEnt *c = &g->texCache[i];
        if (c->valid && c->img == img && c->tlut == g->tlut &&
            c->fmt == g->tileFmt && c->siz == g->tileSiz &&
            c->w == w && c->h == h)
            return &c->tex;
    }
    if (g->texCacheCount >= MMPS2_TEX_CACHE)
        return NULL;

    {
        Mmps2TexEnt *e = &g->texCache[g->texCacheCount++];
        e->img = img;
        e->tlut = g->tlut;
        e->fmt = (u8)g->tileFmt;
        e->siz = (u8)g->tileSiz;
        e->w = w;
        e->h = h;

        e->mem = (u8 *)malloc(w * h * 4);
        if (!e->mem) {
            e->valid = 0;
            return NULL;
        }
        mmps2_tex_decode((u32 *)e->mem, g->tileFmt, g->tileSiz, w, h, img, pal);

        memset(&e->tex, 0, sizeof(e->tex));
        e->tex.Width = w;
        e->tex.Height = h;
        e->tex.PSM = GS_PSM_CT32;
        e->tex.ClutPSM = GS_PSM_CT16;
        e->tex.Mem = (u32 *)e->mem;
        e->tex.Filter = (u8)g->filter;
        e->tex.Vram = gsKit_vram_alloc(g->gs, gsKit_texture_size(w, (int)h, GS_PSM_CT32),
                                       GSKIT_ALLOC_USERBUFFER);
        e->tex.VramClut = 0;
        gsKit_texture_upload(g->gs, &e->tex);
        e->valid = 1;
        return &e->tex;
    }
}

// --- N64 color combiner -----------------------------------------------------

// One combiner channel: (A - B) * C + D, with the product scaled by 1/255.
static u8 comb_chan(int a, int b, int c, int d)
{
    int t = (a - b) * c;
    int v;
    t = (t >= 0) ? (t + 127) / 255 : (t - 127) / 255;
    v = t + d;
    if (v < 0)
        v = 0;
    if (v > 255)
        v = 255;
    return (u8)v;
}

// Broadcast a scalar into an RGB triple.
static void set_rgb(u8 out[3], u8 v)
{
    out[0] = out[1] = out[2] = v;
}

static void mux_rgb(int sel, int wide, const Mmps2Rgba *comb, const Mmps2Rgba *t0,
                    const Mmps2Rgba *t1, const Mmps2Rgba *sh,
                    const Mmps2Rgba *pr, const Mmps2Rgba *en, u8 out[3])
{
    // `wide` selects the 5-bit c field, where 7 means COMBINED_ALPHA. In the
    // 4-bit a/b and 3-bit d fields, 7 is NOISE/0 and G_CCMUX_0 (31) truncates
    // down to 15/7, both of which the RDP treats as a literal zero.
    switch (sel) {
    case G_CCMUX_COMBINED:      out[0] = comb->r; out[1] = comb->g; out[2] = comb->b; break;
    case G_CCMUX_TEXEL0:        out[0] = t0->r; out[1] = t0->g; out[2] = t0->b; break;
    case G_CCMUX_TEXEL1:        out[0] = t1->r; out[1] = t1->g; out[2] = t1->b; break;
    case G_CCMUX_PRIMITIVE:     out[0] = pr->r; out[1] = pr->g; out[2] = pr->b; break;
    case G_CCMUX_SHADE:         out[0] = sh->r; out[1] = sh->g; out[2] = sh->b; break;
    case G_CCMUX_ENVIRONMENT:   out[0] = en->r; out[1] = en->g; out[2] = en->b; break;
    case G_CCMUX_1:             set_rgb(out, 255); break;
    case G_CCMUX_COMBINED_ALPHA:
        if (wide)
            set_rgb(out, comb->a);
        else
            set_rgb(out, 0);
        break;
    case G_CCMUX_TEXEL0_ALPHA:  set_rgb(out, t0->a); break;
    case G_CCMUX_TEXEL1_ALPHA:  set_rgb(out, t1->a); break;
    case G_CCMUX_PRIMITIVE_ALPHA:set_rgb(out, pr->a); break;
    case G_CCMUX_SHADE_ALPHA:   set_rgb(out, sh->a); break;
    case G_CCMUX_ENV_ALPHA:     set_rgb(out, en->a); break;
    case G_CCMUX_LOD_FRACTION:
    case G_CCMUX_PRIM_LOD_FRAC:
    case G_CCMUX_0:
    default:                    set_rgb(out, 0); break;
    }
}

static u8 mux_alpha(int sel, const Mmps2Rgba *comb, const Mmps2Rgba *t0,
                    const Mmps2Rgba *t1, const Mmps2Rgba *sh,
                    const Mmps2Rgba *pr, const Mmps2Rgba *en)
{
    switch (sel) {
    case G_ACMUX_COMBINED:   return comb->a;
    case G_ACMUX_TEXEL0:     return t0->a;
    case G_ACMUX_TEXEL1:     return t1->a;
    case G_ACMUX_PRIMITIVE:  return pr->a;
    case G_ACMUX_SHADE:      return sh->a;
    case G_ACMUX_ENVIRONMENT:return en->a;
    case G_ACMUX_1:          return 255;
    case G_ACMUX_0:
    default:                 return 0;
    }
}

Mmps2Rgba mmps2_combine(const Mmps2Gfx *g, Mmps2Rgba texel, Mmps2Rgba shade,
                        Mmps2Rgba prim, Mmps2Rgba env)
{
    Mmps2Rgba tex1 = texel; // no multi-texture: TEXEL1 falls back to TEXEL0
    Mmps2Rgba comb = { 0, 0, 0, 0 };
    u32 hi = g->combine_hi, lo = g->combine_lo;
    int cyc = (g->othermode_h >> G_MDSFT_CYCLETYPE) & 3;
    int i, ncyc = (cyc == G_CYC_2CYCLE) ? 2 : 1;

    for (i = 0; i < ncyc; i++) {
        int a, b, c, d, Aa, Ab, Ac, Ad;
        u8 ar[3], br[3], cr[3], dr[3];

        a  = (i == 0) ? ((hi >> 20) & 0xF) : ((hi >> 5) & 0xF);
        c  = (i == 0) ? ((hi >> 15) & 0x1F) : (hi & 0x1F);
        Aa = (i == 0) ? ((hi >> 12) & 0x7) : ((lo >> 21) & 0x7);
        Ac = (i == 0) ? ((hi >> 9) & 0x7) : ((lo >> 18) & 0x7);
        b  = (i == 0) ? ((lo >> 28) & 0xF) : ((lo >> 24) & 0xF);
        d  = (i == 0) ? ((lo >> 15) & 0x7) : ((lo >> 6) & 0x7);
        Ab = (i == 0) ? ((lo >> 12) & 0x7) : ((lo >> 3) & 0x7);
        Ad = (i == 0) ? ((lo >> 9) & 0x7) : (lo & 0x7);

        mux_rgb(a, 0, &comb, &texel, &tex1, &shade, &prim, &env, ar);
        mux_rgb(b, 0, &comb, &texel, &tex1, &shade, &prim, &env, br);
        mux_rgb(c, 1, &comb, &texel, &tex1, &shade, &prim, &env, cr);
        mux_rgb(d, 0, &comb, &texel, &tex1, &shade, &prim, &env, dr);
        comb.r = comb_chan(ar[0], br[0], cr[0], dr[0]);
        comb.g = comb_chan(ar[1], br[1], cr[1], dr[1]);
        comb.b = comb_chan(ar[2], br[2], cr[2], dr[2]);

        comb.a = comb_chan(mux_alpha(Aa, &comb, &texel, &tex1, &shade, &prim, &env),
                           mux_alpha(Ab, &comb, &texel, &tex1, &shade, &prim, &env),
                           mux_alpha(Ac, &comb, &texel, &tex1, &shade, &prim, &env),
                           mux_alpha(Ad, &comb, &texel, &tex1, &shade, &prim, &env));
    }

    return comb;
}

static int mux_is_texel(int sel)
{
    return sel == G_CCMUX_TEXEL0 || sel == G_CCMUX_TEXEL1 ||
           sel == G_CCMUX_TEXEL0_ALPHA || sel == G_CCMUX_TEXEL1_ALPHA;
}

int mmps2_combiner_uses_texture(const Mmps2Gfx *g)
{
    u32 hi = g->combine_hi, lo = g->combine_lo;
    int a0 = (hi >> 20) & 0xF, c0 = (hi >> 15) & 0x1F;
    int Aa0 = (hi >> 12) & 0x7, Ac0 = (hi >> 9) & 0x7;
    int a1 = (hi >> 5) & 0xF, c1 = hi & 0x1F;
    int b0 = (lo >> 28) & 0xF, b1 = (lo >> 24) & 0xF;
    int Aa1 = (lo >> 21) & 0x7, Ac1 = (lo >> 18) & 0x7;
    int d0 = (lo >> 15) & 0x7, d1 = (lo >> 6) & 0x7;
    int Ab0 = (lo >> 12) & 0x7, Ad0 = (lo >> 9) & 0x7;
    int Ab1 = (lo >> 3) & 0x7, Ad1 = lo & 0x7;

    return mux_is_texel(a0) || mux_is_texel(c0) ||
           mux_is_texel(Aa0) || mux_is_texel(Ac0) ||
           mux_is_texel(a1) || mux_is_texel(c1) ||
           mux_is_texel(Aa1) || mux_is_texel(Ac1) ||
           mux_is_texel(b0) || mux_is_texel(b1) ||
           mux_is_texel(d0) || mux_is_texel(d1) ||
           mux_is_texel(Ab0) || mux_is_texel(Ad0) ||
           mux_is_texel(Ab1) || mux_is_texel(Ad1);
}

// Per-vertex shade colour fed to the GS. The N64 shade is the interpolated
// vertex colour; for lit geometry the RSP would have applied lighting into it,
// which we don't implement, so fall back to the primitive colour.
static Mmps2Rgba vertex_shade(const Mmps2Gfx *g, const Vtx *v)
{
    Mmps2Rgba c;
    if ((g->geom & G_LIGHTING) ||
        (v->cn[0] == 0 && v->cn[1] == 0 && v->cn[2] == 0)) {
        c.r = g->r; c.g = g->g; c.b = g->b; c.a = g->a;
    } else {
        c.r = v->cn[0]; c.g = v->cn[1]; c.b = v->cn[2]; c.a = v->cn[3];
    }
    return c;
}
static void emit_triangle(Mmps2Gfx *g, int a, int b, int c)
{
    float ax, ay, bx, by, cx, cy;
    int az, bz, cz;
    const Vtx *va, *vb, *vc;
    u64 ca, cb, cc;
    const GSTEXTURE *tex;
    Mmps2Rgba prim, env, white;
    int wantTex;

    if (a < 0 || a >= MMPS2_VTX_MAX || b < 0 || b >= MMPS2_VTX_MAX ||
        c < 0 || c >= MMPS2_VTX_MAX)
        return;

    va = &g->vtx[a];
    vb = &g->vtx[b];
    vc = &g->vtx[c];

    // Positions were captured when the vertices were loaded (the RSP does the
    // vertex transform on G_VTX), so a triangle can mix batches loaded under
    // different matrices — which is how flexible skeletons skin a joint.
    ax = g->sx[a]; ay = g->sy[a]; az = g->sz[a];
    bx = g->sx[b]; by = g->sy[b]; bz = g->sz[b];
    cx = g->sx[c]; cy = g->sy[c]; cz = g->sz[c];

    prim.r = g->r; prim.g = g->g; prim.b = g->b; prim.a = g->a;
    env.r = g->er; env.g = g->eg; env.b = g->eb; env.a = g->ea;
    white.r = white.g = white.b = white.a = 0xFF;

    // The RDP combiner mixes per-pixel texel with per-vertex shade/prim/env.
    // gsKit samples the texture itself, so we evaluate the combiner with an
    // opaque-white texel to obtain the vertex-side colour factor; the GS then
    // multiplies it by the sampled texel during rasterisation.
    wantTex = g->texOn && mmps2_combiner_uses_texture(g) &&
              g->texImg[G_TX_RENDERTILE] != NULL;
    tex = wantTex ? bind_texture(g) : NULL;
    if (!tex)
        wantTex = 0;

    {
        Mmps2Rgba sa = vertex_shade(g, va);
        Mmps2Rgba sb = vertex_shade(g, vb);
        Mmps2Rgba sc = vertex_shade(g, vc);
        Mmps2Rgba ra = mmps2_combine(g, white, sa, prim, env);
        Mmps2Rgba rb = mmps2_combine(g, white, sb, prim, env);
        Mmps2Rgba rc = mmps2_combine(g, white, sc, prim, env);
        ca = GS_SETREG_RGBAQ(ra.r, ra.g, ra.b, ra.a, 0);
        cb = GS_SETREG_RGBAQ(rb.r, rb.g, rb.b, rb.a, 0);
        cc = GS_SETREG_RGBAQ(rc.r, rc.g, rc.b, rc.a, 0);
    }

    if (tex) {
        float sc = (float)g->texScale / (32.0f * 65536.0f);
        float iw = sc / (float)g->texW;
        float ih = sc / (float)g->texH;
        gsKit_prim_triangle_goraud_texture_3d(g->gs, (GSTEXTURE *)tex,
            ax, ay, az, (float)va->tc[0] * iw, (float)va->tc[1] * ih,
            bx, by, bz, (float)vb->tc[0] * iw, (float)vb->tc[1] * ih,
            cx, cy, cz, (float)vc->tc[0] * iw, (float)vc->tc[1] * ih,
            ca, cb, cc);
    } else {
        gsKit_prim_triangle_3d(g->gs, ax, ay, az, bx, by, bz, cx, cy, cz, ca);
    }
}

void mmps2_gfx_set_modelview(Mmps2Gfx *g, const Mtx *m)
{
    g->mv[0] = *m;
    g->mvTop = 0;
}

void mmps2_gfx_set_projection(Mmps2Gfx *g, const Mtx *m)
{
    g->proj[0] = *m;
    g->projTop = 0;
}


static void run_dl(Mmps2Gfx *g, const Gfx *dl, int depth)
{
    const Gfx *p = dl;

    if (depth > MMPS2_DL_DEPTH_MAX)
        return;

    for (;;) {
        u32 w0 = p->w0;
        u32 w1 = p->w1;
        u32 cmd = w0 >> 24;

        switch (cmd) {
        case G_ENDDL:
            return;

        case G_DL: {
            const Gfx *target = (const Gfx *)resolve(g, w1);
            if (target)
                run_dl(g, target, depth + 1);
            p++;
            break;
        }

        case G_VTX: {
            int n = (int)((w0 >> 12) & 0xFF);
            int base = (int)((w0 >> 1) & 0x7F) - n;
            const Vtx *src = (const Vtx *)resolve(g, w1);
            int i;
            if (src) {
                for (i = 0; i < n && base + i < MMPS2_VTX_MAX; i++) {
                    if (base + i >= 0) {
                        g->vtx[base + i] = src[i];
                        // The RSP transforms a vertex when it loads it, so the
                        // matrix in effect here is what positions this vertex.
                        // Flexible skeletons rely on it: they swap limb
                        // matrices between vertex batches and then mix the
                        // batches inside a single triangle group.
                        xform(g, &g->vtx[base + i], &g->sx[base + i],
                              &g->sy[base + i], &g->sz[base + i]);
                    }
                }
                if (base + n > g->nvtx && base + n <= MMPS2_VTX_MAX)
                    g->nvtx = base + n;
            }
            p++;
            break;
        }

        case G_MODIFYVTX: {
            // w0 = |cmd|where:8|00000000|vtx*2:16|, w1 = value
            int where = (int)((w0 >> 16) & 0xFF);
            int idx = (int)(w0 & 0xFFFF) / 2;
            if (where == G_MWO_POINT_ZSCREEN && idx >= 0 && idx < MMPS2_VTX_MAX) {
                g->vtx[idx].ob[2] = (s16)w1;
                xform(g, &g->vtx[idx], &g->sx[idx], &g->sy[idx], &g->sz[idx]);
            }
            p++;
            break;
        }

        case G_MTX: {
            int flags = (int)(w0 & 0xFF) ^ G_MTX_PUSH;
            const Mtx *m;
            Mtx *stack;
            int *top;

            // Extracted actor limbs load their bone matrix from a per-limb table
            // at segment 0x0D. We pose the skeleton ourselves, so ignore these.
            if (((w1 >> 24) & 0x0F) == 0x0D && g->seg[0x0D] == 0) {
                p++;
                break;
            }

            m = (const Mtx *)resolve(g, w1);
            if (!m) {
                p++;
                break;
            }
            stack = (flags & G_MTX_PROJECTION) ? g->proj : g->mv;
            top = (flags & G_MTX_PROJECTION) ? &g->projTop : &g->mvTop;

            if ((flags & G_MTX_PUSH) && *top < MMPS2_MTX_STACK - 1) {
                stack[*top + 1] = stack[*top];
                (*top)++;
            }
            if (flags & G_MTX_LOAD) {
                stack[*top] = *m;
            } else {
                Mtx out;
                mul4(&out, &stack[*top], m);
                stack[*top] = out;
            }
            p++;
            break;
        }

        case G_POPMTX: {
            // gsSPPopMatrix(n): w1 = n (count). The older gsDma2p encoding puts
            // a byte size (n*64) in w1; treat anything out of range as one pop.
            int n = (int)w1;
            if (n < 1 || n > MMPS2_MTX_STACK)
                n = 1;
            g->mvTop -= n;
            if (g->mvTop < 0)
                g->mvTop = 0;
            p++;
            break;
        }

        case G_MOVEMEM: {
            int idx = (int)(w0 & 0xFF);
            if (idx == G_MV_VIEWPORT) {
                const Vp *vp = (const Vp *)resolve(g, w1);
                if (vp)
                    g->vp = *vp;
            }
            p++;
            break;
        }

        case G_MOVEWORD: {
            int index = (int)((w0 >> 16) & 0xFF);
            int offset = (int)(w0 & 0xFFFF);
            switch (index) {
            case G_MW_SEGMENT: {
                int seg = (offset / 4) & (MMPS2_SEG_MAX - 1);
                g->seg[seg] = w1;
                break;
            }
            case G_MW_PERSPNORM:
                g->perspNorm = (u16)w1;
                break;
            default:
                break;
            }
            p++;
            break;
        }

        case G_TRI1: {
            // F3DEX_GBI_2 packs the indices into w0 (cmd occupies bits 24..31).
            int a = (int)((w0 >> 16) & 0xFF) / 2;
            int b = (int)((w0 >> 8) & 0xFF) / 2;
            int c = (int)(w0 & 0xFF) / 2;
            emit_triangle(g, a, b, c);
            p++;
            break;
        }

        case G_TRI2:
        case G_QUAD: {
            int a0 = (int)((w0 >> 16) & 0xFF) / 2;
            int b0 = (int)((w0 >> 8) & 0xFF) / 2;
            int c0 = (int)(w0 & 0xFF) / 2;
            int a1 = (int)((w1 >> 16) & 0xFF) / 2;
            int b1 = (int)((w1 >> 8) & 0xFF) / 2;
            int c1 = (int)(w1 & 0xFF) / 2;
            emit_triangle(g, a0, b0, c0);
            emit_triangle(g, a1, b1, c1);
            p++;
            break;
        }

        case G_SETPRIMCOLOR:
            g->r = (u8)((w1 >> 24) & 0xFF);
            g->g = (u8)((w1 >> 16) & 0xFF);
            g->b = (u8)((w1 >> 8) & 0xFF);
            g->a = (u8)(w1 & 0xFF);
            p++;
            break;

        case G_SETENVCOLOR:
            g->er = (u8)((w1 >> 24) & 0xFF);
            g->eg = (u8)((w1 >> 16) & 0xFF);
            g->eb = (u8)((w1 >> 8) & 0xFF);
            g->ea = (u8)(w1 & 0xFF);
            p++;
            break;

        case G_GEOMETRYMODE:
            // F3DEX_GBI(2): w0 low 24 bits = ~clear, w1 = set bits.
            g->geom = (g->geom & (w0 & 0x00FFFFFF)) | w1;
            p++;
            break;

        case G_TEXTURE:
            // w0: |cmd|bowtie|level:3|tile:3|on:8|  w1: |s:16|t:16|
            g->texLevel = (int)((w0 >> 11) & 0x07);
            g->texOn = (int)((w0 >> 1) & 0x7F);
            g->texScale = (s32)((w1 >> 16) & 0xFFFF);
            p++;
            break;

        case G_SETOTHERMODE_H:
        case G_SETOTHERMODE_L: {
            int len = (int)(w0 & 0xFF) + 1;
            int sft = 32 - (int)((w0 >> 8) & 0xFF) - len;
            u32 mask, data;

            if (sft < 0 || sft > 31)
                sft = 0;
            mask = (len >= 32) ? 0xFFFFFFFFu : (((1u << len) - 1u) << sft);
            data = (w1 << sft);

            if (cmd == G_SETOTHERMODE_H) {
                g->othermode_h = (g->othermode_h & ~mask) | (data & mask);
                g->tlutType = (int)((g->othermode_h >> G_MDSFT_TEXTLUT) & 3);
                g->filter = ((g->othermode_h >> G_MDSFT_TEXTFILT) & 3)
                                ? GS_FILTER_LINEAR : GS_FILTER_NEAREST;
            } else {
                g->othermode_l = (g->othermode_l & ~mask) | (data & mask);
            }
            p++;
            break;
        }

        case G_SETCOMBINE:
            g->combine_hi = w0;
            g->combine_lo = w1;
            p++;
            break;

        case G_SETTILE: {
            u32 tile = (w1 >> 24) & 0x07;
            g->tile[tile] = w0;
            g->tileFmtT[tile] = (w0 >> 21) & 0x07;
            g->tileSizT[tile] = (w0 >> 19) & 0x03;
            g->tileLineT[tile] = (s32)((w0 >> 9) & 0x1FF);
            g->tileTmemT[tile] = (s32)(w0 & 0x1FF);
            if (tile == G_TX_RENDERTILE) {
                g->tileFmt = g->tileFmtT[tile];
                g->tileSiz = g->tileSizT[tile];
            }
            p++;
            break;
        }

        case G_SETTILESIZE:
        case G_LOADTILE: {
            u32 tile = (w1 >> 24) & 0x07;
            s32 uls = (s32)((w0 >> 12) & 0xFFF), ult = (s32)(w0 & 0xFFF);
            s32 lrs = (s32)((w1 >> 12) & 0xFFF), lrt = (s32)(w1 & 0xFFF);
            g->tileSize[tile] = w0;
            g->tileULS[tile] = uls; g->tileULT[tile] = ult;
            g->tileLRS[tile] = lrs; g->tileLRT[tile] = lrt;
            if (tile == G_TX_RENDERTILE) {
                g->texW = (lrs >= uls) ? (u32)(((lrs - uls) >> 2) + 1) : 0;
                g->texH = (lrt >= ult) ? (u32)(((lrt - ult) >> 2) + 1) : 0;
            }
            p++;
            break;
        }

        case G_LOADBLOCK: {
            // w1 = |tile:3| 000| lrs(texels-1):12 | dxt:12|
            u32 tile = (w1 >> 24) & 0x07;
            u32 lrs = (w1 >> 12) & 0xFFF;
            u32 dxt = w1 & 0xFFF;
            if (tile == G_TX_LOADTILE) {
                g->blkTexels = lrs + 1;
                if (dxt == 0 && g->texW == 0 && g->tileLineT[G_TX_RENDERTILE] > 0) {
                    // No G_SETTILESIZE was emitted (hand-written texture load):
                    // infer the width from the render tile line (64-bit words).
                    int bits = 4 << g->tileSizT[G_TX_RENDERTILE];
                    u32 row = (u32)g->tileLineT[G_TX_RENDERTILE];
                    u32 texels = g->blkTexels;
                    if (g->tileSizT[G_TX_RENDERTILE] == G_IM_SIZ_4b)
                        texels *= 2; // loaded as 8b
                    if (bits >= 4 && row > 0 && (row * 64u) / (u32)bits > 0) {
                        g->texW = (row * 64u) / (u32)bits;
                        g->texH = (g->texW > 0) ? texels / g->texW : 0;
                    }
                }
            }
            p++;
            break;
        }

        case G_SETTIMG:
            g->texImgLoad = resolve(g, w1);
            g->texImg[G_TX_RENDERTILE] = g->texImgLoad;
            p++;
            break;

        case G_LOADTLUT:
            g->tlut = g->texImgLoad;
            g->tlutCount = ((w1 >> 14) & 0x3FF) + 1;
            g->tlutLoadCount = g->tlutCount;
            p++;
            break;

        default:
            p++;
            break;
        }
    }
}

void mmps2_gfx_run(Mmps2Gfx *g, const Gfx *dl)
{
    if (g && dl)
        run_dl(g, dl, 0);
}
