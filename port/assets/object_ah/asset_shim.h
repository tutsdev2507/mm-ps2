#ifndef MM_PS2_ASSET_SHIM_H
#define MM_PS2_ASSET_SHIM_H

typedef signed char s8;
typedef unsigned char u8;
typedef signed short int s16;
typedef unsigned short int u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long int s64;
typedef unsigned long long int u64;
typedef float f32;

typedef struct {
    s16 x;
    s16 y;
    s16 z;
} Vec3s;

typedef struct {
    short ob[3];
    unsigned short flag;
    short tc[2];
    unsigned char cn[4];
} Vtx_t;

typedef struct {
    short ob[3];
    unsigned short flag;
    short tc[2];
    signed char n[3];
    unsigned char a;
} Vtx_tn;

typedef union {
    Vtx_t v;
    Vtx_tn n;
    long long int force_structure_alignment;
} Vtx;

typedef union {
    struct {
        u32 w0;
        u32 w1;
    } words;
    long long int force_structure_alignment;
} Gfx;

typedef long int Mtx_t[4][4];
typedef union {
    Mtx_t m;
    struct {
        u16 intPart[4][4];
        u16 fracPart[4][4];
    };
    long long int force_structure_alignment;
} Mtx;

typedef struct {
    u16 x;
    u16 y;
    u16 z;
} JointIndex;

typedef struct {
    s16 frameCount;
} AnimationHeaderCommon;

typedef struct {
    AnimationHeaderCommon common;
    s16* frameData;
    JointIndex* jointIndices;
    u16 staticIndexMax;
} AnimationHeader;

typedef struct {
    Vec3s jointPos;
    u8 child;
    u8 sibling;
    Gfx* dList;
} StandardLimb;

typedef struct {
    void** segment;
    u8 limbCount;
} SkeletonHeader;

typedef struct {
    SkeletonHeader sh;
    u8 dListCount;
} FlexSkeletonHeader;

#endif
