/*
 * Platform runtime: owns the gsKit device and the port's graphics context.
 *
 * This is the only translation unit that sees BOTH <gsKit.h> and the port's
 * gfx/gbi headers, so the shims that must also see <ultra64.h> (rsp_shim,
 * hw_shim, vi_gs) reach it through opaque declarations instead of pulling in
 * port/src/gfx/gfx.h -- which would re-declare Gfx/Vtx/Mtx and clash with the
 * decomp's libultra GBI types.
 */

#include <gsKit.h>

#include "gfx/gfx.h"

GSGLOBAL *g_gs;

static Mmps2Gfx s_gfx;
static int s_inited;

static void runtime_ensure(void)
{
    if (s_inited)
        return;
    s_inited = 1;

    g_gs = gsKit_init_global();
    if (g_gs == NULL)
        return;

    g_gs->Mode = GS_MODE_NTSC;
    g_gs->Interlace = GS_INTERLACED;
    g_gs->Field = GS_FIELD;
    g_gs->Width = 640;
    g_gs->Height = 448;

    gsKit_init_screen(g_gs);
    gsKit_mode_switch(g_gs, GS_ONESHOT);

    mmps2_gfx_init(&s_gfx, g_gs);
}

void mmps2_runtime_init(void)
{
    runtime_ensure();
}

/* Called from rsp_shim's osSpTaskStartGo: one master display list == one
 * frame.  Clear, interpret, push the GIF, wait for retrace. */
void mmps2_run_dl(void *dl)
{
    runtime_ensure();
    if (g_gs == NULL || dl == NULL)
        return;

    gsKit_clear(g_gs, GS_SETREG_RGBAQ(0x10, 0x10, 0x20, 0, 0));
    mmps2_gfx_run(&s_gfx, (const Gfx *)dl);
    gsKit_queue_exec(g_gs);
    gsKit_sync_flip(g_gs);
}
