// Minimal F3DEX2 (F3DZEX2) GBI subset for the PS2 port.
// Mirrors the opcode/format definitions used by the decomp (include/PR/gbi.h)
// but self-contained so the port does not depend on N64 libultra headers.
#ifndef MMPS2_GBI_H
#define MMPS2_GBI_H

#include <tamtypes.h>
#include <stdint.h>

typedef struct {
    u32 w0;
    u32 w1;
} Gfx;

// F3DEX2 vertex (16 bytes): position, flag/index, texcoord, color-or-normal.
typedef struct {
    s16 ob[3];
    u16 flag;
    s16 tc[2];
    u8 cn[4];
} Vtx;

// F3DEX2 matrix: integer + fraction halves, each 4x4 of 16-bit lanes.
typedef struct {
    u16 intPart[4][4];
    u16 fracPart[4][4];
} Mtx;

// F3DEX2 viewport: scale/translate carry 2 fractional bits.
typedef struct {
    s16 vscale[4];
    s16 vtrans[4];
} Vp;

#define BOWTIE_VAL          0

#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & ((1u << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w) ((u32)(((u32)(v) >> (s)) & ((1u << (w)) - 1)))

// --- F3DEX_GBI_2 opcodes (as emitted by gsSP*) ---
#define G_VTX               0x01
#define G_MODIFYVTX         0x02
#define G_CULLDL            0x03
#define G_BRANCH_Z          0x04
#define G_TRI1              0x05
#define G_TRI2              0x06
#define G_QUAD              0x07
#define G_LINE3D            0x08

#define G_NOOP              0xC0
#define G_SETCIMG           0xFF
#define G_SETZIMG           0xFE
#define G_SETTIMG           0xFD
#define G_SETCOMBINE        0xFC
#define G_SETENVCOLOR       0xFB
#define G_SETPRIMCOLOR      0xFA
#define G_SETBLENDCOLOR     0xF9
#define G_SETFOGCOLOR       0xF8
#define G_SETFILLCOLOR      0xF7
#define G_FILLRECT          0xF6
#define G_SETTILE           0xF5
#define G_LOADTILE          0xF4
#define G_LOADBLOCK         0xF3
#define G_SETTILESIZE       0xF2
#define G_LOADTLUT          0xF0
#define G_RDPSETOTHERMODE   0xEF
#define G_SETPRIMDEPTH      0xEE
#define G_SETSCISSOR        0xED
#define G_SETCONVERT        0xEC
#define G_SETKEYR           0xEB
#define G_SETKEYGB          0xEA
#define G_RDPFULLSYNC       0xE9
#define G_RDPTILESYNC       0xE8
#define G_RDPPIPESYNC       0xE7
#define G_RDPLOADSYNC       0xE6
#define G_TEXRECTFLIP       0xE5
#define G_TEXRECT           0xE4

#define G_LOAD_UCODE        0xDD
#define G_MOVEMEM           0xDC
#define G_MOVEWORD          0xDB
#define G_MTX               0xDA
#define G_GEOMETRYMODE      0xD9
#define G_POPMTX            0xD8
#define G_TEXTURE           0xD7
#define G_DMA_IO            0xD6
#define G_SPNOOP            0xE0
#define G_RDPHALF_1         0xE1
#define G_SETOTHERMODE_L    0xE2
#define G_SETOTHERMODE_H    0xE3
#define G_DL                0xDE
#define G_ENDDL             0xDF
#define G_RDPHALF_2         0xF1

// --- G_MTX flags (F3DEX_GBI_2) ---
#define G_MTX_MODELVIEW     0x00
#define G_MTX_PROJECTION    0x04
#define G_MTX_MUL           0x00
#define G_MTX_LOAD          0x02
#define G_MTX_NOPUSH        0x00
#define G_MTX_PUSH          0x01

// --- G_DL flags ---
#define G_DL_PUSH           0
#define G_DL_NOPUSH         1

// --- G_MOVEMEM indices ---
#define G_MV_VIEWPORT       8
#define G_MV_LIGHT          10

// --- G_MOVEWORD indices ---
#define G_MW_MATRIX         0x00
#define G_MW_NUMLIGHT       0x02
#define G_MW_CLIP           0x04
#define G_MW_SEGMENT        0x06
#define G_MW_FOG            0x08
#define G_MW_PERSPNORM      0x0E

// --- G_MODIFYVTX "where" ---
#define G_MWO_POINT_RGBA    0x10
#define G_MWO_POINT_ST      0x14
#define G_MWO_POINT_XYSCREEN 0x18
#define G_MWO_POINT_ZSCREEN 0x1C

// --- G_GEOMETRYMODE bits (F3DEX_GBI_2 values) ---
#define G_ZBUFFER           0x00000001
#define G_SHADE             0x00000004
#define G_TEXTURE_ENABLE    0x00000000
#define G_SHADING_SMOOTH    0x00200000
#define G_CULL_FRONT        0x00000200
#define G_CULL_BACK         0x00000400
#define G_CULL_BOTH         0x00000600
#define G_FOG               0x00010000
#define G_LIGHTING          0x00020000
#define G_TEXTURE_GEN       0x00040000
#define G_TEXTURE_GEN_LINEAR 0x00080000

// --- Image formats / sizes (N64) ---
#define G_IM_FMT_RGBA 0
#define G_IM_FMT_YUV  1
#define G_IM_FMT_CI   2
#define G_IM_FMT_IA   3
#define G_IM_FMT_I    4

#define G_IM_SIZ_4b   0
#define G_IM_SIZ_8b   1
#define G_IM_SIZ_16b  2
#define G_IM_SIZ_32b  3

// --- Tiles / othermode bitfields we care about ---
#define G_TX_RENDERTILE  0
#define G_TX_LOADTILE    7

#define G_MDSFT_TEXTLUT  14
#define G_MDSFT_TEXTFILT 12
#define G_TT_NONE        0
#define G_TT_RGBA16      2
#define G_TT_IA16        3

// --- Othermode sub-fields (H) ---
#define G_MDSFT_CYCLETYPE   20
#define G_CYC_1CYCLE        0
#define G_CYC_2CYCLE        1
#define G_CYC_COPY          2
#define G_CYC_FILL          3

// --- Othermode sub-fields (L) ---
#define G_MDSFT_ALPHACOMPARE 0
#define G_AC_NONE            0
#define G_AC_THRESHOLD       1
#define G_AC_DITHER          3

// --- Color combiner MUX selectors (CC / AC) ---
#define G_CCMUX_COMBINED        0
#define G_CCMUX_TEXEL0          1
#define G_CCMUX_TEXEL1          2
#define G_CCMUX_PRIMITIVE       3
#define G_CCMUX_SHADE           4
#define G_CCMUX_ENVIRONMENT     5
#define G_CCMUX_CENTER          6
#define G_CCMUX_SCALE           6
#define G_CCMUX_COMBINED_ALPHA  7
#define G_CCMUX_TEXEL0_ALPHA    8
#define G_CCMUX_TEXEL1_ALPHA    9
#define G_CCMUX_PRIMITIVE_ALPHA 10
#define G_CCMUX_SHADE_ALPHA     11
#define G_CCMUX_ENV_ALPHA       12
#define G_CCMUX_LOD_FRACTION    13
#define G_CCMUX_PRIM_LOD_FRAC   14
#define G_CCMUX_NOISE           7
#define G_CCMUX_K4              7
#define G_CCMUX_K5              15
#define G_CCMUX_1               6
#define G_CCMUX_0               31

#define G_ACMUX_COMBINED        0
#define G_ACMUX_TEXEL0          1
#define G_ACMUX_TEXEL1          2
#define G_ACMUX_PRIMITIVE       3
#define G_ACMUX_SHADE           4
#define G_ACMUX_ENVIRONMENT     5
#define G_ACMUX_LOD_FRACTION    0
#define G_ACMUX_PRIM_LOD_FRAC   6
#define G_ACMUX_1               6
#define G_ACMUX_0               7

#define G_ON   1
#define G_OFF  0

// --- Asset-side types (layout-compatible with the N64 libultra originals) so
// the vendored extracted assets can be referenced from the port. ---
typedef struct {
    s16 x, y, z;
} Vec3s;

typedef struct {
    u16 x, y, z;
} JointIndex;

typedef struct {
    s16 frameCount;
} AnimationHeaderCommon;

typedef struct {
    AnimationHeaderCommon common;
    s16 *frameData;
    JointIndex *jointIndices;
    u16 staticIndexMax;
} AnimationHeader;

typedef struct {
    Vec3s jointPos;
    u8 child;
    u8 sibling;
    Gfx *dList;
} StandardLimb;

typedef struct {
    void **segment;
    u8 limbCount;
} SkeletonHeader;

typedef struct {
    SkeletonHeader sh;
    u8 dListCount;
} FlexSkeletonHeader;

// --- Authoring macros (subset; produce the same words as the decomp's
// F3DEX_GBI_2 macros, which also define F3DEX_GBI, so triangle indices are
// packed with the flag-railroads and *2 scaling). They expand to compound
// literals so a display list can be built at runtime (matrix/vertex pointers
// are only known then).
#define gsSPMatrix(m, p)                                                        \
    ((Gfx){                                                                     \
        (_SHIFTL(G_MTX, 24, 8) | _SHIFTL(((sizeof(Mtx)) - 1) / 8, 19, 5) |      \
         _SHIFTL(((p) ^ G_MTX_PUSH), 0, 8)),                                    \
        (u32)(uintptr_t)(m)                                                     \
    })

#define gsSPVertex(v, n, v0)                                                    \
    ((Gfx){                                                                     \
        (_SHIFTL(G_VTX, 24, 8) | _SHIFTL((n), 12, 8) | _SHIFTL((v0) + (n), 1, 7)), \
        (u32)(uintptr_t)(v)                                                     \
    })

#define __gsSP1Triangle_w1(v0, v1, v2)                                          \
    (_SHIFTL((v0) * 2, 16, 8) | _SHIFTL((v1) * 2, 8, 8) | _SHIFTL((v2) * 2, 0, 8))

#define __gsSP1Triangle_w1f(v0, v1, v2, flag)                                   \
    (((flag) == 0) ? __gsSP1Triangle_w1(v0, v1, v2) :                           \
     ((flag) == 1) ? __gsSP1Triangle_w1(v1, v2, v0) :                           \
                     __gsSP1Triangle_w1(v2, v0, v1))

#define gsSP1Triangle(v0, v1, v2, flag)                                         \
    ((Gfx){                                                                     \
        (_SHIFTL(G_TRI1, 24, 8) | __gsSP1Triangle_w1f(v0, v1, v2, flag)),       \
        0                                                                       \
    })

#define gsSP2Triangles(v00, v01, v02, flag0, v10, v11, v12, flag1)              \
    ((Gfx){                                                                     \
        (_SHIFTL(G_TRI2, 24, 8) | __gsSP1Triangle_w1f(v00, v01, v02, flag0)),   \
        __gsSP1Triangle_w1f(v10, v11, v12, flag1)                              \
    })

#define gsSP1Quadrangle(v0, v1, v2, v3, flag)                                   \
    ((Gfx){                                                                     \
        (_SHIFTL(G_QUAD, 24, 8) | __gsSP1Quadrangle_w1f(v0, v1, v2, v3, flag)), \
        __gsSP1Quadrangle_w2f(v0, v1, v2, v3, flag)                            \
    })

#define __gsSP1Quadrangle_w1f(v0, v1, v2, v3, flag)                             \
    (((flag) == 0) ? __gsSP1Triangle_w1(v0, v1, v2) :                           \
     ((flag) == 1) ? __gsSP1Triangle_w1(v1, v2, v3) :                           \
     ((flag) == 2) ? __gsSP1Triangle_w1(v2, v3, v0) :                           \
                     __gsSP1Triangle_w1(v3, v0, v1))

#define __gsSP1Quadrangle_w2f(v0, v1, v2, v3, flag)                             \
    (((flag) == 0) ? __gsSP1Triangle_w1(v0, v2, v3) :                           \
     ((flag) == 1) ? __gsSP1Triangle_w1(v1, v3, v0) :                           \
     ((flag) == 2) ? __gsSP1Triangle_w1(v2, v0, v1) :                           \
                     __gsSP1Triangle_w1(v3, v1, v2))

#define gsSPDisplayList(dl)                                                     \
    ((Gfx){                                                                     \
        (_SHIFTL(G_DL, 24, 8) | _SHIFTL(G_DL_PUSH, 16, 8)),                     \
        (u32)(uintptr_t)(dl)                                                    \
    })

#define gsSPEndDisplayList()                                                    \
    ((Gfx){                                                                     \
        (_SHIFTL(G_ENDDL, 24, 8)),                                              \
        0                                                                       \
    })

#define gsSPPopMatrix(n)                                                        \
    ((Gfx){                                                                     \
        (_SHIFTL(G_POPMTX, 24, 8)),                                             \
        (u32)(n)                                                                \
    })

#define gsDPSetPrimColor(m, l, r, g, b, a)                                      \
    ((Gfx){                                                                     \
        (_SHIFTL(G_SETPRIMCOLOR, 24, 8) | _SHIFTL((m), 8, 8) | _SHIFTL((l), 0, 8)), \
        (_SHIFTL((r), 24, 8) | _SHIFTL((g), 16, 8) | _SHIFTL((b), 8, 8) |       \
         _SHIFTL((a), 0, 8))                                                    \
    })

#define gsDPSetEnvColor(r, g, b, a)                                             \
    ((Gfx){                                                                     \
        (_SHIFTL(G_SETENVCOLOR, 24, 8)),                                        \
        (_SHIFTL((r), 24, 8) | _SHIFTL((g), 16, 8) | _SHIFTL((b), 8, 8) |       \
         _SHIFTL((a), 0, 8))                                                    \
    })

#define gsSPViewport(v)                                                         \
    ((Gfx){                                                                     \
        (_SHIFTL(G_MOVEMEM, 24, 8) | _SHIFTL(((sizeof(Vp) - 1) / 8), 19, 5) |   \
         _SHIFTL(G_MV_VIEWPORT, 0, 8)),                                         \
        (u32)(uintptr_t)(v)                                                     \
    })

#define gsMoveWd(index, offset, data)                                           \
    ((Gfx){                                                                     \
        (_SHIFTL(G_MOVEWORD, 24, 8) | _SHIFTL((index), 16, 8) | _SHIFTL((offset), 0, 16)), \
        (u32)(data)                                                             \
    })

#define gsSPSegment(seg, base)      gsMoveWd(G_MW_SEGMENT, (seg) * 4, base)

// F3DEX_GBI(2): w0 low 24 bits = ~clear, w1 = set.
#define gsSPGeometryMode(c, s)                                                  \
    ((Gfx){                                                                     \
        (_SHIFTL(G_GEOMETRYMODE, 24, 8) | _SHIFTL(~(u32)(c), 0, 24)),           \
        (u32)(s)                                                                \
    })
#define gsSPSetGeometryMode(word)   gsSPGeometryMode(0, (word))
#define gsSPClearGeometryMode(word) gsSPGeometryMode((word), 0)

#define gsSPTexture(s, t, level, tile, on)                                      \
    ((Gfx){                                                                     \
        (_SHIFTL(G_TEXTURE, 24, 8) | _SHIFTL(BOWTIE_VAL, 16, 8) |               \
         _SHIFTL((level), 11, 3) | _SHIFTL((tile), 8, 3) | _SHIFTL((on), 1, 7)), \
        (_SHIFTL((s), 16, 16) | _SHIFTL((t), 0, 16))                            \
    })

// --- Color combiner authoring (same packing as the decomp's F3DEX2 macros) ---
#define __GCCc0w0(a, c, Aa, Ac) \
    (_SHIFTL((a), 20, 4) | _SHIFTL((c), 15, 5) | _SHIFTL((Aa), 12, 3) | _SHIFTL((Ac), 9, 3))
#define __GCCc1w0(a, c) (_SHIFTL((a), 5, 4) | _SHIFTL((c), 0, 5))
#define __GCCc0w1(b, d, Ab, Ad) \
    (_SHIFTL((b), 28, 4) | _SHIFTL((d), 15, 3) | _SHIFTL((Ab), 12, 3) | _SHIFTL((Ad), 9, 3))
#define __GCCc1w1(b, Aa, Ac, d, Ab, Ad)                                          \
    (_SHIFTL((b), 24, 4) | _SHIFTL((Aa), 21, 3) | _SHIFTL((Ac), 18, 3) |         \
     _SHIFTL((d), 6, 3) | _SHIFTL((Ab), 3, 3) | _SHIFTL((Ad), 0, 3))

#define gsDPSetCombineLERP(a0, b0, c0, d0, Aa0, Ab0, Ac0, Ad0,                   \
                           a1, b1, c1, d1, Aa1, Ab1, Ac1, Ad1)                   \
    ((Gfx){                                                                      \
        (_SHIFTL(G_SETCOMBINE, 24, 8) |                                          \
         ((__GCCc0w0(a0, c0, Aa0, Ac0) | __GCCc1w0(a1, c1)) & 0x00FFFFFFu)),     \
        (u32)(__GCCc0w1(b0, d0, Ab0, Ad0) |                                      \
              __GCCc1w1(b1, Aa1, Ac1, d1, Ab1, Ad1))                             \
    })

#endif // MMPS2_GBI_H
