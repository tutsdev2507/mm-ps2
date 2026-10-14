// mm-ps2 M2 demo: boot the EE, initialise gsKit, and render an animated 3D
// cube through the port's F3DEX2 GBI interpreter (port/src/gfx). This exercises
// the full transform chain real display lists need: modelview (rotation) ->
// projection (perspective) -> viewport -> perspective divide -> GS triangle.
//
// It is a *renderer* demo, not the game yet: the game still needs the libultra
// shim (M3), the asset loader and the graph/audio threads before anything from
// Majora's Mask itself can run.

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <gsKit.h>
#include <gsToolkit.h>

#include "gfx/gfx.h"

#define CUBE_FRAMES 100000

static Vtx s_cube[8] __attribute__((aligned(16)));
static Mtx s_proj __attribute__((aligned(16)));
static Mtx s_mv __attribute__((aligned(16)));
static Mtx s_rotX __attribute__((aligned(16)));
static Mtx s_rotY __attribute__((aligned(16)));
static Mtx s_tmp __attribute__((aligned(16)));
static Gfx s_dl[64];

// Six faces, each two triangles, with a distinct colour. No culling is enabled
// so winding does not matter for visibility; the Z-buffer resolves overlaps.
static const int s_faces[6][4] = {
    { 4, 5, 6, 7 }, // +z
    { 1, 0, 3, 2 }, // -z
    { 0, 4, 7, 3 }, // -x
    { 5, 1, 2, 6 }, // +x
    { 7, 6, 2, 3 }, // +y
    { 0, 1, 5, 4 }, // -y
};
static const u8 s_facecol[6][3] = {
    { 0xFF, 0x40, 0x40 }, { 0x40, 0xFF, 0x40 }, { 0x40, 0x40, 0xFF },
    { 0xFF, 0xFF, 0x40 }, { 0xFF, 0x40, 0xFF }, { 0x40, 0xFF, 0xFF },
};

static void build_vbo(void)
{
    static const s16 v[8][3] = {
        { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 },
        { -1, -1, 1 },  { 1, -1, 1 },  { 1, 1, 1 },  { -1, 1, 1 },
    };
    int i;
    for (i = 0; i < 8; i++) {
        s_cube[i].ob[0] = v[i][0];
        s_cube[i].ob[1] = v[i][1];
        s_cube[i].ob[2] = v[i][2];
        s_cube[i].flag = 0;
        s_cube[i].tc[0] = s_cube[i].tc[1] = 0;
        s_cube[i].cn[0] = s_cube[i].cn[1] = s_cube[i].cn[2] = 0xFF;
        s_cube[i].cn[3] = 0x00;
    }
}

static void build_scene(void)
{
    int i = 0, f;

    // Camera: 45 degree perspective, cube sits at z = -4 in view space.
    mmps2_mtx_perspective(&s_proj, 45.0f * 3.14159265f / 180.0f,
                          640.0f / 448.0f, 1.0f, 100.0f);

    s_dl[i++] = gsSPMatrix(&s_proj, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    s_dl[i++] = gsSPMatrix(&s_mv, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    s_dl[i++] = gsSPVertex(s_cube, 8, 0);
    for (f = 0; f < 6; f++) {
        const int *q = s_faces[f];
        s_dl[i++] = gsDPSetPrimColor(0, 0, s_facecol[f][0], s_facecol[f][1],
                                     s_facecol[f][2], 0xFF);
        s_dl[i++] = gsSP1Triangle(q[0], q[1], q[2], 0);
        s_dl[i++] = gsSP1Triangle(q[0], q[2], q[3], 0);
    }
    s_dl[i++] = gsSPEndDisplayList();
}

// view = translate(0,0,-4) * rotateX * rotateY
static void update_modelview(float t)
{
    mmps2_mtx_rotate_x(&s_rotX, t * 0.7f);
    mmps2_mtx_rotate_y(&s_rotY, t * 1.1f);
    mmps2_mtx_multiply(&s_tmp, &s_rotX, &s_rotY);
    mmps2_mtx_translate(&s_mv, 0.0f, 0.0f, -4.0f);
    mmps2_mtx_multiply(&s_mv, &s_mv, &s_tmp);
}

int main(int argc, char *argv[])
{
    GSGLOBAL *gsGlobal;
    Mmps2Gfx gfx;
    int frame;

    (void)argc;
    (void)argv;

    SifInitRpc(0);

    gsGlobal = gsKit_init_global();
    gsGlobal->Mode = GS_MODE_NTSC;
    gsGlobal->Interlace = GS_INTERLACED;
    gsGlobal->Field = GS_FIELD;
    gsGlobal->Width = 640;
    gsGlobal->Height = 448;
    gsGlobal->ZBuffering = GS_SETTING_ON;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_ONESHOT);

    mmps2_gfx_init(&gfx, gsGlobal);
    build_vbo();
    build_scene();

    for (frame = 0; frame < CUBE_FRAMES; frame++) {
        update_modelview(frame * 0.03f);
        gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x10, 0x10, 0x20, 0x80, 0x00));
        mmps2_gfx_run(&gfx, s_dl);
        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);
    }

    gsKit_deinit_global(gsGlobal);
    SleepThread();
    return 0;
}
