// mm-ps2 M0 harness: boot the EE, initialise gsKit, present frames.
// This proves the PS2 toolchain + gsKit pipeline before the game code is linked in.

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>
#include <dmaKit.h>
#include <gsKit.h>
#include <gsToolkit.h>

int main(int argc, char *argv[])
{
    SifInitRpc(0);

    GSGLOBAL *gsGlobal = gsKit_init_global();
    gsGlobal->Mode = GS_MODE_NTSC;
    gsGlobal->Interlace = GS_INTERLACED;
    gsGlobal->Field = GS_FIELD;
    gsGlobal->Width = 640;
    gsGlobal->Height = 448;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GSKIT_ONESHOT);

    int frame = 0;
    while (frame < 600) {
        u8 v = (u8)((frame * 2) & 0xFF);
        gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(v, 0x20, 0x80 - (v >> 1), 0x80, 0x00));
        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);
        frame++;
    }

    gsKit_deinit_global(gsGlobal);
    SleepThread();
    return 0;
}
