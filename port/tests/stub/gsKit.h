#ifndef STUB_GSKIT_H
#define STUB_GSKIT_H

#include <tamtypes.h>
#include <dmaKit.h>

typedef struct {
    int Mode;
    int Interlace;
    int Field;
    int Width;
    int Height;
    u8 ZBuffering;
    int PSMZ;
} GSGLOBAL;

typedef struct gsTexture {
    u32 Width;
    u32 Height;
    u8 PSM;
    u8 ClutPSM;
    u32 TBW;
    u32 *Mem;
    u32 *Clut;
    u32 Vram;
    u32 VramClut;
    u32 Filter;
    u8 ClutStorageMode;
    u8 Delayed;
} GSTEXTURE;

#define GS_PSM_CT32 0x00
#define GS_PSM_CT24 0x01
#define GS_PSM_CT16 0x02
#define GS_PSM_T8   0x13
#define GS_PSM_T4   0x14

#define GS_FILTER_NEAREST 0x00
#define GS_FILTER_LINEAR  0x01

#define GSKIT_ALLOC_SYSBUFFER  0x00
#define GSKIT_ALLOC_USERBUFFER 0x01

#define GS_MODE_NTSC 0
#define GS_INTERLACED 0
#define GS_FIELD 0
#define GS_ONESHOT 0
#define GS_SETTING_OFF 0x00
#define GS_SETTING_ON 0x01

#define GS_SETREG_RGBAQ(r, g, b, a, q) \
    ((u64)(r) | ((u64)(g) << 8) | ((u64)(b) << 16) | ((u64)(a) << 24) | ((u64)(q) << 32))

GSGLOBAL *gsKit_init_global(void);
void gsKit_init_screen(GSGLOBAL *g);
void gsKit_mode_switch(GSGLOBAL *g, int mode);
void gsKit_clear(GSGLOBAL *g, u64 color);
void gsKit_queue_exec(GSGLOBAL *g);
void gsKit_sync_flip(GSGLOBAL *g);
void gsKit_deinit_global(GSGLOBAL *g);

void gsKit_prim_triangle_3d(GSGLOBAL *g,
                            float x1, float y1, int iz1,
                            float x2, float y2, int iz2,
                            float x3, float y3, int iz3, u64 color);

void gsKit_prim_triangle_goraud_texture_3d(GSGLOBAL *g, GSTEXTURE *Texture,
                            float x1, float y1, int iz1, float u1, float v1,
                            float x2, float y2, int iz2, float u2, float v2,
                            float x3, float y3, int iz3, float u3, float v3,
                            u64 color1, u64 color2, u64 color3);

int gsKit_texture_size(int width, int height, u32 psm);
u32 gsKit_vram_alloc(GSGLOBAL *g, u32 bytes, u8 type);
void gsKit_texture_upload(GSGLOBAL *g, GSTEXTURE *Texture);

#endif // STUB_GSKIT_H
