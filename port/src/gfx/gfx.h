// F3DEX2 display-list interpreter that emits gsKit primitives.
#ifndef MMPS2_GFX_H
#define MMPS2_GFX_H

#include <tamtypes.h>
#include <gsKit.h>

#include "gbi.h"

#define MMPS2_VTX_MAX 64
#define MMPS2_MTX_STACK 8
#define MMPS2_SEG_MAX 16

typedef struct {
    Vtx vtx[MMPS2_VTX_MAX];
    int nvtx;

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
    u32 texImg;
    int texLevel;
    int texOn;
    s32 texScale;

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

#endif // MMPS2_GFX_H
