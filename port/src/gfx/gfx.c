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

// Resolve an N64 address word: if the 4-bit segment is loaded, base + offset,
// otherwise treat the word as a direct host pointer (used by hand-built DLs).
static const void *resolve(const Mmps2Gfx *g, u32 w1)
{
    u32 seg = (w1 >> 24) & 0x0F;
    if (g->seg[seg] != 0)
        return (const void *)(uintptr_t)(g->seg[seg] + (w1 & 0x00FFFFFF));
    return (const void *)(uintptr_t)(w1 & ~7u);
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
    const u16 *pal = NULL;
    u32 i, w = g->texW, h = g->texH;

    if (!g->texImg || w == 0 || h == 0)
        return NULL;
    if (g->tileFmt == G_IM_FMT_CI && g->tlut) {
        if (g->tlutCount < 256)
            return NULL;
        pal = (const u16 *)g->tlut;
    }

    for (i = 0; i < (u32)g->texCacheCount; i++) {
        Mmps2TexEnt *c = &g->texCache[i];
        if (c->valid && c->img == g->texImg && c->tlut == g->tlut &&
            c->fmt == g->tileFmt && c->siz == g->tileSiz &&
            c->w == w && c->h == h)
            return &c->tex;
    }
    if (g->texCacheCount >= MMPS2_TEX_CACHE)
        return NULL;

    {
        Mmps2TexEnt *e = &g->texCache[g->texCacheCount++];
        e->img = g->texImg;
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
        mmps2_tex_decode((u32 *)e->mem, g->tileFmt, g->tileSiz, w, h, g->texImg, pal);

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

// Per-vertex colour: when lighting is on we don't evaluate the combiner, so fall
// back to the primitive colour (white) rather than the raw normal in cn[].
static u64 vertex_color(const Mmps2Gfx *g, const Vtx *v)
{
    u8 r, gr, b;

    if ((g->geom & G_LIGHTING) ||
        (v->cn[0] == 0 && v->cn[1] == 0 && v->cn[2] == 0)) {
        // Lit geometry (cn[] holds the normal, not a colour) or unset vertex
        // colours: fall back to the primitive colour.
        r = g->r;
        gr = g->g;
        b = g->b;
    } else {
        r = v->cn[0];
        gr = v->cn[1];
        b = v->cn[2];
    }
    return GS_SETREG_RGBAQ(r, gr, b, 0xFF, 0);
}

static void emit_triangle(Mmps2Gfx *g, int a, int b, int c)
{
    float ax, ay, bx, by, cx, cy;
    int az, bz, cz;
    const Vtx *va, *vb, *vc;
    u64 ca, cb, cc;
    const GSTEXTURE *tex;

    if (a < 0 || a >= MMPS2_VTX_MAX || b < 0 || b >= MMPS2_VTX_MAX ||
        c < 0 || c >= MMPS2_VTX_MAX)
        return;

    va = &g->vtx[a];
    vb = &g->vtx[b];
    vc = &g->vtx[c];

    xform(g, va, &ax, &ay, &az);
    xform(g, vb, &bx, &by, &bz);
    xform(g, vc, &cx, &cy, &cz);

    ca = vertex_color(g, va);
    cb = vertex_color(g, vb);
    cc = vertex_color(g, vc);

    tex = (g->texOn && g->texImg) ? bind_texture(g) : NULL;

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
            for (i = 0; i < n && base + i < MMPS2_VTX_MAX; i++) {
                if (base + i >= 0)
                    g->vtx[base + i] = src[i];
            }
            if (base + n > g->nvtx && base + n <= MMPS2_VTX_MAX)
                g->nvtx = base + n;
            p++;
            break;
        }

        case G_MODIFYVTX: {
            // w0 = |cmd|where:8|00000000|vtx*2:16|, w1 = value
            int where = (int)((w0 >> 16) & 0xFF);
            int idx = (int)(w0 & 0xFFFF) / 2;
            if (where == G_MWO_POINT_ZSCREEN && idx >= 0 && idx < MMPS2_VTX_MAX)
                g->vtx[idx].ob[2] = (s16)w1;
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
            if (tile == G_TX_RENDERTILE) {
                g->tileFmt = (w0 >> 21) & 0x07;
                g->tileSiz = (w0 >> 19) & 0x03;
            }
            p++;
            break;
        }

        case G_SETTILESIZE: {
            u32 tile = (w1 >> 24) & 0x07;
            g->tileSize[tile] = w0;
            if (tile == G_TX_RENDERTILE) {
                u32 uls = (w0 >> 12) & 0xFFF, ult = w0 & 0xFFF;
                u32 lrs = (w1 >> 12) & 0xFFF, lrt = w1 & 0xFFF;
                g->texW = ((lrs - uls) >> 2) + 1;
                g->texH = ((lrt - ult) >> 2) + 1;
            }
            p++;
            break;
        }

        case G_SETTIMG:
            g->texImg = resolve(g, w1);
            p++;
            break;

        case G_LOADTLUT:
            g->tlut = g->texImg;
            g->tlutCount = ((w1 >> 14) & 0x3FF) + 1;
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
