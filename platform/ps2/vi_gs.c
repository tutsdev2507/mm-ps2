#include "mmps2_os.h"
#include <gsKit.h>
#include <string.h>

/*
 * Video / retrace shim.
 *
 * The N64 VI is emulated by the gsKit retrace handler: every vertical sync
 * bumps the counter in mmps2_os.c and dispatches OS_EVENT_VI messages, which
 * is exactly what the game's irqmgr waits on.
 *
 * osViSetEvent() is the registration point the game actually uses
 * (src/boot/irqmgr.c) -- it must reach osSetEventMesg(OS_EVENT_VI, ...) or
 * the graph thread never wakes up.
 */

extern GSGLOBAL *g_gs;

static OSMesgQueue g_viQueue;
static OSMesg g_viMsgs[8];
static int s_viReady;

static int mmps2_vsync(int field)
{
    (void)field;
    mmps2_os_retrace();
    return 0;
}

void mmps2_vi_start(void)
{
    if (s_viReady)
        return;
    s_viReady = 1;
    osCreateMesgQueue(&g_viQueue, g_viMsgs, 8);
    if (g_gs != NULL)
        gsKit_add_vsync_handler(mmps2_vsync);
}

void osCreateViManager(OSPri pri)
{
    (void)pri;
    mmps2_vi_start();
}

void osViSetMode(OSViMode *mode)
{
    (void)mode;
}

void osViSetEvent(OSMesgQueue *mq, OSMesg msg, u32 retraceCount)
{
    (void)retraceCount;
    osSetEventMesg(OS_EVENT_VI, mq, msg);
}

void osViSwapBuffer(void *frameBufPtr)
{
    (void)frameBufPtr;
    /* Presentation is driven by mmps2_run_dl() (one gsKit flip per master
     * display list).  The N64 framebuffer pointer itself is meaningless here
     * because the port renders straight into gsKit's GIF buffers. */
}

void osViBlack(u8 active)
{
    (void)active;
}

void osViSetSpecialFeatures(u32 func)
{
    (void)func;
}
