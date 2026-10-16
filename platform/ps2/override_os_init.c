#include "mmps2_os.h"
#include "ultra64.h"

extern void mmps2_os_init(void);

void __real_osInitialize(void);
void __wrap_osInitialize(void)
{
    mmps2_os_init();
    osTvType = OS_TV_NTSC;
    osResetType = 0;
    osMemSize = 32 * 1024 * 1024;
    osRomBase = NULL;
}
