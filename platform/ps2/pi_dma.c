#include "mmps2_os.h"
#include <string.h>

extern OSPiHandle CartHandle;
OSPiHandle* osCartRomInit(void)
{
    static OSPiHandle h;
    memset(&h, 0, sizeof(h));
    h.type = 1;
    return &h;
}

s32 osEPiStartDma(OSPiHandle* pihandle, OSIoMesg* mb, s32 direction)
{
    (void)pihandle; (void)direction;
    if (mb && mb->dramAddr && mb->size > 0) {
        osInvalDCache(mb->dramAddr, mb->size);
    }
    if (mb && mb->hdr.retQueue)
        osSendMesg(mb->hdr.retQueue, mb->dramAddr, OS_MESG_NOBLOCK);
    return 0;
}
