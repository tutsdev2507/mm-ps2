#include "mmps2_os.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

extern OSPiHandle CartHandle;
OSPiHandle* osCartRomInit(void)
{
    static OSPiHandle h;
    memset(&h, 0, sizeof(h));
    h.type = 1;
    return &h;
}

static int rom_fd = -1;

static int ensure_open(void)
{
    if (rom_fd >= 0)
        return 0;
    const char *paths[] = {
        "mass0:/MMPS2/baserom-decompressed.z64",
        "mass0:/MMPS2/baserom.z64",
        "mass:/MMPS2/baserom-decompressed.z64",
        "mass:/MMPS2/baserom.z64",
        "mass0:/baserom-decompressed.z64",
        "mass0:/baserom.z64",
        "mass:/baserom-decompressed.z64",
        "mass:/baserom.z64",
        "baserom-decompressed.z64",
        "baserom.z64",
        "rom.z64",
        "MMPS2/baserom.z64",
        "MMPS2/baserom-decompressed.z64",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        rom_fd = open(paths[i], O_RDONLY);
        if (rom_fd >= 0)
            return 0;
    }
    return -1;
}

s32 osEPiStartDma(OSPiHandle* pihandle, OSIoMesg* mb, s32 direction)
{
    (void)pihandle; (void)direction;
    if (mb && mb->dramAddr && mb->size > 0) {
        if (ensure_open() >= 0) {
            lseek(rom_fd, mb->devAddr, SEEK_SET);
            read(rom_fd, mb->dramAddr, mb->size);
        }
        osInvalDCache(mb->dramAddr, mb->size);
    }
    if (mb && mb->hdr.retQueue)
        osSendMesg(mb->hdr.retQueue, mb->dramAddr, OS_MESG_NOBLOCK);
    return 0;
}
