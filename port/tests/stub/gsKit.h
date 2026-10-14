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

#endif // STUB_GSKIT_H
