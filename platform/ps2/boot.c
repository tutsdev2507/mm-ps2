/*
 * P0+P1+P2 bring-up.
 *
 * P0/P1: boot the PS2, install a 60 Hz retrace (VSync) source and run a
 *        message-queue driven "graph" thread that flips framebuffers.
 * P2:    the graph thread builds a real F3DEX2 master display list each frame
 *        and runs it through the port's GBI interpreter (port/src/gfx), which
 *        emits gsKit primitives. This is the same code path the decompiled
 *        game's graph thread will use once it is wired up.
 */

#include <gsKit.h>
#include <kernel.h>
#include <math.h>

#include "mmps2_os.h"
#include "gfx/gfx.h"

#define MMPS2_PI 3.14159265358979f

static GSGLOBAL *g_gs;
static Mmps2Gfx g_gfx;

static OSMesgQueue g_viQueue;
static OSMesg g_viMsgs[8];

static OSThread g_graphThread;
static u8 g_graphStack[MMPS2_DEFAULT_STACK_SIZE] __attribute__((aligned(16)));

static Vtx g_vtx[4] __attribute__((aligned(16)));
static Mtx g_proj __attribute__((aligned(16)));
static Mtx g_view __attribute__((aligned(16)));
static Mtx g_rot __attribute__((aligned(16)));

static int vsync_handler(int id)
{
    (void)id;
    mmps2_os_retrace();
    return 0;
}

static void init_quad(void)
{
    int i;
    for (i = 0; i < 4; i++) {
        g_vtx[i].flag = 0;
        g_vtx[i].tc[0] = g_vtx[i].tc[1] = 0;
        g_vtx[i].cn[0] = g_vtx[i].cn[1] = 0;
        g_vtx[i].cn[2] = g_vtx[i].cn[3] = 0;
    }

    g_vtx[0].ob[0] = -400; g_vtx[0].ob[1] = -400; g_vtx[0].ob[2] = 0;
    g_vtx[1].ob[0] = 400;  g_vtx[1].ob[1] = -400; g_vtx[1].ob[2] = 0;
    g_vtx[2].ob[0] = 400;  g_vtx[2].ob[1] = 400;  g_vtx[2].ob[2] = 0;
    g_vtx[3].ob[0] = -400; g_vtx[3].ob[1] = 400;  g_vtx[3].ob[2] = 0;
}

static void graph_frame(u32 frame)
{
    Gfx dl[16];
    int n = 0;
    u8 r = (u8)(128 + 127.0f * sinf((float)frame * 0.03f));
    u8 g = (u8)(128 + 127.0f * sinf((float)frame * 0.05f + 2.0f));
    u8 b = (u8)(128 + 127.0f * sinf((float)frame * 0.07f + 4.0f));

    mmps2_mtx_rotate_y(&g_rot, (float)frame * 0.02f);
    mmps2_mtx_translate(&g_view, 0.0f, 0.0f, -3000.0f);
    mmps2_mtx_multiply(&g_view, &g_view, &g_rot);

    dl[n++] = gsSPMatrix(&g_proj, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    dl[n++] = gsSPMatrix(&g_view, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    dl[n++] = gsSPClearGeometryMode(G_LIGHTING);
    dl[n++] = gsSPSetGeometryMode(G_SHADE | G_SHADING_SMOOTH);
    dl[n++] = gsDPSetPrimColor(0, 0, r, g, b, 0xFF);
    dl[n++] = gsSPVertex(g_vtx, 4, 0);
    dl[n++] = gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0);
    dl[n++] = gsSPEndDisplayList();

    gsKit_clear(g_gs, GS_SETREG_RGBAQ(0x10, 0x10, 0x20, 0, 0));
    mmps2_gfx_run(&g_gfx, dl);
    gsKit_queue_exec(g_gs);
    gsKit_sync_flip(g_gs);
}

static void graph_entry(void *arg)
{
    OSMesg msg;
    u32 frame = 0;

    (void)arg;

    for (;;) {
        osRecvMesg(&g_viQueue, &msg, OS_MESG_BLOCK);
        graph_frame(frame);
        frame++;
    }
}

int main(void)
{
    mmps2_os_init();

    g_gs = gsKit_init_global();
    g_gs->Mode = GS_MODE_NTSC;
    g_gs->Interlace = GS_INTERLACED;
    g_gs->Field = GS_FIELD;
    g_gs->Width = 640;
    g_gs->Height = 448;

    gsKit_init_screen(g_gs);
    gsKit_mode_switch(g_gs, GS_ONESHOT);

    mmps2_gfx_init(&g_gfx, g_gs);
    mmps2_mtx_perspective(&g_proj, 45.0f * MMPS2_PI / 180.0f,
                          640.0f / 448.0f, 10.0f, 20000.0f);
    mmps2_gfx_set_projection(&g_gfx, &g_proj);
    init_quad();

    gsKit_clear(g_gs, GS_SETREG_RGBAQ(0x10, 0x10, 0x20, 0, 0));
    gsKit_queue_exec(g_gs);
    gsKit_sync_flip(g_gs);

    osCreateMesgQueue(&g_viQueue, g_viMsgs, 8);
    osSetEventMesg(OS_EVENT_VI, &g_viQueue, (OSMesg)1);
    gsKit_add_vsync_handler(vsync_handler);

    osCreateThread(&g_graphThread, 4, graph_entry, NULL,
                   g_graphStack + sizeof(g_graphStack), 11);
    osStartThread(&g_graphThread);

    for (;;)
        SleepThread();

    return 0;
}
