// mm-ps2 M2.5 demo: render an actor extracted from Majora's Mask (object_ah)
// through the port's F3DEX2 GBI interpreter (port/src/gfx). The skeleton is
// posed here (static T-pose) and each limb's display list is run with an
// externally-computed modelview, so the actor's own per-limb matrix table
// (segment 0x0D) is bypassed.
//
// This is still a *renderer* demo: the game needs the libultra shim (M3), the
// asset loader and the graph/audio threads before anything from Majora's Mask
// itself can run.

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <gsKit.h>
#include <gsToolkit.h>

#include "gfx/gfx.h"
#include "object_ah.h"

#define FRAMES 100000

static Mtx s_proj __attribute__((aligned(16)));
static Mtx s_view __attribute__((aligned(16)));
static Mtx s_rot __attribute__((aligned(16)));
static Mtx s_tmp __attribute__((aligned(16)));

// Walk the limb tree, accumulating jointPos translations, and run each limb's
// display list with the resulting bone matrix (times the camera view).
static void draw_limb(Mmps2Gfx *gfx, void **seg, int index, const Mtx *parent, int depth)
{
    const StandardLimb *limb;
    Mtx local, tmp, mv;

    if (index == 0xFF || depth > 32)
        return;

    limb = (const StandardLimb *)seg[index];

    mmps2_mtx_translate(&tmp, (float)limb->jointPos.x,
                        (float)limb->jointPos.y, (float)limb->jointPos.z);
    mmps2_mtx_multiply(&local, parent, &tmp);

    if (limb->dList != NULL) {
        mmps2_mtx_multiply(&mv, &s_view, &local);
        mmps2_gfx_set_modelview(gfx, &mv);
        mmps2_gfx_run(gfx, limb->dList);
    }

    if (limb->child != 0xFF)
        draw_limb(gfx, seg, limb->child, &local, depth + 1);
    if (limb->sibling != 0xFF)
        draw_limb(gfx, seg, limb->sibling, parent, depth + 1);
}

int main(int argc, char *argv[])
{
    GSGLOBAL *gsGlobal;
    Mmps2Gfx gfx;
    void **limbs;
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

    mmps2_mtx_perspective(&s_proj, 45.0f * 3.14159265f / 180.0f,
                          640.0f / 448.0f, 10.0f, 20000.0f);
    mmps2_gfx_set_projection(&gfx, &s_proj);

    limbs = object_ah_Skel_009E70.sh.segment;

    for (frame = 0; frame < FRAMES; frame++) {
        Mtx ident;

        mmps2_mtx_identity(&ident);
        mmps2_mtx_rotate_y(&s_rot, (float)frame * 0.01f);
        mmps2_mtx_translate(&s_view, 0.0f, -1300.0f, -6000.0f);
        mmps2_mtx_multiply(&s_view, &s_view, &s_rot);

        gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x20, 0x30, 0x50, 0x80, 0));

        draw_limb(&gfx, limbs, 0, &ident, 0);

        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);
    }

    gsKit_deinit_global(gsGlobal);
    return 0;
}
