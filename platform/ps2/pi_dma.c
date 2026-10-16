#include "mmps2_os.h"
#include <string.h>

/* Implemented in port/src/ps2/rom_io.c (ps2sdk-only translation unit). */
extern int mmps2_rom_open(void);
extern int mmps2_rom_read(unsigned int off, void *buf, unsigned int size);
extern void mmps2_log(const char *fmt, ...);

OSPiHandle* osCartRomInit(void)
{
    static OSPiHandle h;
    memset(&h, 0, sizeof(h));
    h.type = 1;
    mmps2_rom_open();
    return &h;
}

s32 osEPiStartDma(OSPiHandle* pihandle, OSIoMesg* mb, s32 direction)
{
    (void)pihandle;

    if (mb && mb->dramAddr && mb->size > 0 && direction == OS_READ) {
        /* devAddr is a ROM offset; strip the N64 cart-domain base if present. */
        unsigned int off = (unsigned int)mb->devAddr;
        if (off >= 0x10000000u)
            off &= 0x0FFFFFFFu;

        mmps2_log("dma rom=%08x -> ram=%08x size=%u", off, (unsigned int)(uintptr_t)mb->dramAddr, (unsigned int)mb->size);
        mmps2_rom_read(off, (void*)mb->dramAddr, (unsigned int)mb->size);
        osInvalDCache(mb->dramAddr, mb->size);
    }
    if (mb && mb->hdr.retQueue)
        osSendMesg(mb->hdr.retQueue, mb->dramAddr, OS_MESG_NOBLOCK);
    return 0;
}
