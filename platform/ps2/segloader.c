#include "mmps2_os.h"
#include <string.h>

/*
 * Segment loader.
 *
 * The port links only the code image; assets (objects, scenes, archives)
 * are pulled out of the ROM image on demand.  The dmadata table
 * (dmadata_ps2.c) translates VROM -> ROM, and pi_dma.c services the reads,
 * so for now this only owns the bookkeeping the game asks for by name.
 *
 * The previous revision declared `typedef struct OSViMode OSViMode;` here,
 * which clashed with the real (anonymous-struct) typedef in include/PR/os_vi.h
 * and was the compile error at the crash checkpoint.
 */

typedef struct {
    u32 vromStart;
    u32 vromEnd;
} SegRange;

static int s_ready;

int segloader_init(const char *mapfile)
{
    (void)mapfile;
    s_ready = 1;
    return 0;
}

int segloader_ready(void)
{
    return s_ready;
}
