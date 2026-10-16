#include "mmps2_os.h"

#include <kernel.h>

/*
 * N64 cache operations map onto the EE cache flush primitive. The N64 has
 * separate I/D caches addressed by virtual range; the R5900 has a single
 * combined cache, so all variants funnel into FlushCache(0). The code is
 * cache-coherent enough for a port that never runs self-modifying code.
 */

void osInvalICache(void *addr, size_t len)
{
    (void)addr;
    (void)len;
    FlushCache(0);
}

void osInvalDCache(void *addr, size_t len)
{
    (void)addr;
    (void)len;
    FlushCache(0);
}

void osWritebackDCache(void *addr, s32 len)
{
    (void)addr;
    (void)len;
    FlushCache(0);
}

void osWritebackDCacheAll(void)
{
    FlushCache(0);
}

void osMapTLBRdb(void)
{
}

void osUnmapTLBAll(void)
{
}

uintptr_t osVirtualToPhysical(void *addr)
{
    /* EE KSEG0/KSEG1 already alias DRAM, so the virtual address is usable
     * as a physical descriptor for our own DMA-like helpers. */
    return (uintptr_t)addr;
}
