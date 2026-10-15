// mm-ps2 M2.5 demo: render an actor extracted from Majora's Mask (object_ah)
// through the port's F3DEX2 GBI interpreter (port/src/gfx). The skeleton is
// posed here (static rest pose) and the engine builds the same per-limb matrix
// table the actor's skeletal-animation code does (segment 0x0D), so the limb
// display lists load their own matrices exactly as they do in the game.
//
// This is still a *renderer* demo: the game needs the libultra shim (M3), the
// asset loader and the graph/audio threads before anything from Majora's Mask
// itself can run.

#include <stdint.h>

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <gsKit.h>
#include <gsToolkit.h>

#include "gfx/gfx.h"
#include "object_ah.h"

#define FRAMES 100000
#define MAX_LIMB 16

static Mtx s_proj __attribute__((aligned(16)));
static Mtx s_view __attribute__((aligned(16)));
static Mtx s_rot __attribute__((aligned(16)));

// The engine-provided limb matrix table (segment 0x0D). SkelAnime_DrawFlexLod
// allocates `dListCount` matrices, one slot per limb that has a display list,
// filled in draw order, and each limb's display list loads its own (and its
// blend partner's) matrix straight out of it.
static Mtx s_limbMtx[MAX_LIMB] __attribute__((aligned(16)));
static int s_slotOf[64];

// Fill s_limbMtx in the same order SkelAnime_DrawFlexLod does: a pre-order walk
// (child before sibling), one slot per limb with a display list. The root limb
// translates by the joint table (identity at rest); the rest translate by
// their joint position.
static void build_limbs(void **seg, int index, const Mtx *parent, int depth, int *slot)
{
    const StandardLimb *limb;
    Mtx local, tmp;

    if (index == 0xFF || index < 0 || index >= 64 || depth > 32)
        return;

    limb = (const StandardLimb *)seg[index];

    if (depth == 0)
        mmps2_mtx_identity(&tmp);
    else
        mmps2_mtx_translate(&tmp, (float)limb->jointPos.x,
                            (float)limb->jointPos.y, (float)limb->jointPos.z);
    mmps2_mtx_multiply(&local, parent, &tmp);

    s_slotOf[index] = -1;
    if (limb->dList != NULL && *slot < MAX_LIMB) {
        s_slotOf[index] = *slot;
        s_limbMtx[*slot] = local;
        (*slot)++;
    }

    if (limb->child != 0xFF)
        build_limbs(seg, limb->child, &local, depth + 1, slot);
    if (limb->sibling != 0xFF)
        build_limbs(seg, limb->sibling, parent, depth + 1, slot);
}

// Draw every limb. As the actor's skeletal-animation code does, load the limb's
// matrix first (gSPMatrix) and then run its display list.
static void draw_limbs(Mmps2Gfx *gfx, void **seg, int index, int depth)
{
    const StandardLimb *limb;

    if (index == 0xFF || index < 0 || index >= 64 || depth > 32)
        return;

    limb = (const StandardLimb *)seg[index];

    if (limb->dList != NULL && s_slotOf[index] >= 0) {
        mmps2_gfx_set_modelview(gfx, &s_limbMtx[s_slotOf[index]]);
        mmps2_gfx_run(gfx, limb->dList);
    }

    if (limb->child != 0xFF)
        draw_limbs(gfx, seg, limb->child, depth + 1);
    if (limb->sibling != 0xFF)
        draw_limbs(gfx, seg, limb->sibling, depth + 1);
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
        int slot = 0;

        mmps2_mtx_rotate_y(&s_rot, (float)frame * 0.01f);
        mmps2_mtx_translate(&s_view, 0.0f, -1300.0f, -6000.0f);
        mmps2_mtx_multiply(&s_view, &s_view, &s_rot);

        gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x20, 0x30, 0x50, 0x80, 0));

        build_limbs(limbs, 0, &s_view, 0, &slot);
        gfx.seg[0x0D] = (u32)(uintptr_t)s_limbMtx;
        draw_limbs(&gfx, limbs, 0, 0);

        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);
    }

    gsKit_deinit_global(gsGlobal);
    return 0;
}
