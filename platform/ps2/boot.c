/*
 * P0+P1 bring-up: boot the PS2, install a 60 Hz retrace (VSync) source,
 * run a message-queue driven "graph" thread and flip framebuffers each
 * frame. This exercises the libultra shim (mmps2_os) that the decompiled
 * game code will run on.
 */

#include <gsKit.h>
#include <kernel.h>

#include "mmps2_os.h"

static GSGLOBAL *g_gs;
static OSMesgQueue g_viQueue;
static OSMesg g_viMsgs[8];

static OSThread g_graphThread;
static u8 g_graphStack[MMPS2_DEFAULT_STACK_SIZE] __attribute__((aligned(16)));

static int vsync_handler(int id)
{
    (void)id;
    mmps2_os_retrace();
    return 0;
}

static void graph_entry(void *arg)
{
    OSMesg msg;
    u32 frame = 0;

    (void)arg;

    for (;;) {
        osRecvMesg(&g_viQueue, &msg, OS_MESG_BLOCK);

        u8 r = (u8)(frame * 3);
        u8 g = (u8)(frame * 2);
        u8 b = (u8)(frame);

        gsKit_clear(g_gs, GS_SETREG_RGBAQ(r, g, b, 0, 0));
        gsKit_queue_exec(g_gs);
        gsKit_sync_flip(g_gs);

        frame++;
    }
}

int main(void)
{
    mmps2_os_init();

    g_gs = gsKit_init_global();
    gsKit_init_screen(g_gs);
    gsKit_mode_switch(g_gs, GS_ONESHOT);

    gsKit_clear(g_gs, GS_SETREG_RGBAQ(0x10, 0x10, 0x40, 0, 0));
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
