// F3DEX2 display-list interpreter that emits gsKit primitives.
#ifndef MMPS2_GFX_H
#define MMPS2_GFX_H

#include <tamtypes.h>
#include <gsKit.h>

#include "gbi.h"

#define MMPS2_VTX_MAX 64
#define MMPS2_MTX_STACK 8
#define MMPS2_SEG_MAX 16
#define MMPS2_TEX_CACHE 32

// A decoded texture uploaded to GS VRAM once and reused across frames.
typedef struct {
    u8 valid;
    const void *img;
    const void *tlut;
    u8 fmt;
    u8 siz;
    u32 w, h;
    u8 *mem;          // EE-side decoded pixels (gsKit_texture_size bytes)
    GSTEXTURE tex;    // uploaded gsKit texture
} Mmps2TexEnt;

typedef struct {
    Vtx vtx[MMPS2_VTX_MAX];
    int nvtx;

    // Screen-space result of transforming vtx[i], captured when G_VTX loads the
    // vertex (the N64 RSP transforms a vertex on load). A flexible skeleton's
    // display list swaps limb matrices between vertex batches, then draws one
    // triangle group mixing those batches, so each vertex keeps the matrix it
    // was loaded under rather than the matrix active at draw time.
    float sx[MMPS2_VTX_MAX];
    float sy[MMPS2_VTX_MAX];
    int sz[MMPS2_VTX_MAX];

    Mtx proj[MMPS2_MTX_STACK];
    int projTop;
    Mtx mv[MMPS2_MTX_STACK];
    int mvTop;

    Vp vp;

    // Loaded segment bases (indexed by the 4-bit segment in an address).
    u32 seg[MMPS2_SEG_MAX];
    u16 perspNorm;

    u8 r, g, b, a;    // G_SETPRIMCOLOR
    u8 er, eg, eb, ea; // G_SETENVCOLOR
    u32 geom;

    // Texture / combiner / othermode state (recorded for later use).
    u32 othermode_h;
    u32 othermode_l;
    u32 combine_hi;
    u32 combine_lo;
    u32 tile[8];
    u32 tileSize[8];

    // Per-tile decode (indexed by tile number 0..7). The render tile is 0.
    u32 tileFmtT[8];   // G_SETTILE image format
    u32 tileSizT[8];   // G_SETTILE image size
    s32 tileLineT[8];  // G_SETTILE line (64-bit words per row)
    s32 tileTmemT[8];  // G_SETTILE tmem address
    s32 tileULS[8], tileULT[8], tileLRS[8], tileLRT[8]; // G_SETTILESIZE

    // Decoded texture state (captured from the command stream).
    const void *texImg[8]; // per-tile G_SETTIMG image pointer
    const void *texImgLoad;// last G_SETTIMG (used by the next LOADTLUT/LOADBLOCK)
    u32 tlutLoadCount;     // G_LOADTLUT entry count for texImgLoad
    const void *tlut;      // G_LOADTLUT palette pointer (for the render tile)
    u32 tlutCount;         // G_LOADTLUT entry count
    u32 tileFmt;           // render-tile G_SETTILE image format
    u32 tileSiz;           // render-tile G_SETTILE image size
    u32 texW, texH;        // render-tile dimensions in texels
    u32 blkTexels;         // texels in the last G_LOADBLOCK (load tile)
    int tlutType;          // G_TT_* (from othermode H)
    int filter;            // GS_FILTER_*

    Mmps2TexEnt texCache[MMPS2_TEX_CACHE];
    int texCacheCount;
    int texLevel;
    int texOn;
    s32 texScale;          // G_TEXTURE S scale (16.16)
    s32 texScaleT;         // G_TEXTURE T scale (16.16)

    int width;
    int height;
    GSGLOBAL *gs;
} Mmps2Gfx;

void mmps2_gfx_init(Mmps2Gfx *g, GSGLOBAL *gs);
void mmps2_gfx_run(Mmps2Gfx *g, const Gfx *dl);

// Matrix helpers (row-major, N64 F3DEX2 convention: Mtx[row][col]).
void mmps2_mtx_identity(Mtx *m);
void mmps2_mtx_multiply(Mtx *dst, const Mtx *a, const Mtx *b);
void mmps2_mtx_ortho(Mtx *m, float l, float r, float b, float t, float n, float f);
void mmps2_mtx_perspective(Mtx *m, float fovy, float aspect, float near, float far);
void mmps2_mtx_translate(Mtx *m, float x, float y, float z);
void mmps2_mtx_rotate_x(Mtx *m, float rad);
void mmps2_mtx_rotate_y(Mtx *m, float rad);

// Pose the interpreter from outside (skeleton limbs) and set the camera.
void mmps2_gfx_set_modelview(Mmps2Gfx *g, const Mtx *m);
void mmps2_gfx_set_projection(Mmps2Gfx *g, const Mtx *m);

// Decode an N64 texture into a 32-bit RGBA (PS2 PSMCT32, A8B8G8R8) buffer.
// `dst` must hold w*h u32 values; `pal` is an RGBA16 palette for CI formats.
void mmps2_tex_decode(u32 *dst, u32 fmt, u32 siz, u32 w, u32 h,
                      const void *src, const u16 *pal);

// A colour as the RDP combiner works on it (8 bits per channel).
typedef struct {
    u8 r, g, b, a;
} Mmps2Rgba;

// Evaluate the recorded single/two-cycle combiner for the given per-vertex
// inputs, returning the final shade colour. Texel inputs are per-pixel on real
// hardware, so callers pass a representative texel (e.g. opaque white) when they
// only need the vertex-side factor; the texture itself is still sampled by the
// GS during rasterisation.
Mmps2Rgba mmps2_combine(const Mmps2Gfx *g, Mmps2Rgba texel, Mmps2Rgba shade,
                        Mmps2Rgba prim, Mmps2Rgba env);

// True if the active combiner reads TEXEL0/TEXEL1 (i.e. texturing contributes).
int mmps2_combiner_uses_texture(const Mmps2Gfx *g);

#endif // MMPS2_GFX_H
