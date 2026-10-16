#include "mmps2_os.h"

/*
 * Hardware / boot shims for the PS2 port.
 *
 * libultra's osInitialize() is a macro that expands to __osInitialize_common().
 * We provide that symbol instead of fighting the macro.  The decomp also links
 * its own src/libultra/os/initialize.c, which would touch N64 PI/RDRAM
 * registers that do not exist on the EE; the platform objects are listed first
 * on the link line and ld keeps the first definition of each symbol, so this
 * body wins.
 *
 * The N64 hardware init (PIF, RDRAM sizing, CIC detection) has no equivalent
 * here, so we publish plausible values for the globals the game reads
 * afterwards and bring the gsKit device up instead.
 */

/* runtime.c / vi_gs.c */
extern void mmps2_runtime_init(void);
extern void mmps2_log(const char *fmt, ...);

/* osTvType/osResetType/osMemSize/osRomBase are declared s32 in
 * include/PR/os_system.h; do not redeclare them here with another type. */

#define MMPS2_RAM_SIZE 0x02000000u /* 32 MiB of EE main RAM */

/* Publish a plausible machine configuration to the game. */
static void mmps2_publish_system(void)
{
    osTvType = OS_TV_NTSC;
    osResetType = 0;                       /* cold reset */
    osMemSize = MMPS2_RAM_SIZE;
    osRomBase = NULL;
}

void __osInitialize_common(void)
{
    mmps2_log("boot: __osInitialize_common enter");
    mmps2_os_init();
    mmps2_publish_system();
    mmps2_runtime_init();
    mmps2_log("boot: runtime/gsKit up");
}

/*
 * PIF/CIC probing: CIC_ADDRESS_1/2 are N64 physical addresses that are
 * unmapped on the EE and would raise a TLB exception on the first read, so
 * the probe is a no-op and the cached values stay zero.
 */
void CIC6105_Init(void)
{
}

/* getmemsize.c probes RDRAM by XORing it in place; on the EE that would scribble
 * over the linked image, so the size is simply reported. */
u32 osGetMemSize(void)
{
    return MMPS2_RAM_SIZE;
}

/* libultra interrupt critical sections.  The EE status register is the direct
 * equivalent of the VR4300 SR: save it, `di`, restore later. */
OSIntMask __osDisableInt(void)
{
    OSIntMask status;

    __asm__ volatile("mfc0 %0, $12" : "=r"(status));
    __asm__ volatile("di" ::: "memory");
    __asm__ volatile("sync" ::: "memory");
    return status;
}

void __osRestoreInt(OSIntMask mask)
{
    __asm__ volatile("mtc0 %0, $12" :: "r"(mask) : "memory");
    __asm__ volatile("sync" ::: "memory");
}

/* ps2_stubs.c provides an `unsigned char osSetIntMask[16]` placeholder; the
 * game (and osCreatePiManager) calls it as a function, so give it a body. */
OSIntMask osSetIntMask(OSIntMask im)
{
    return im;
}
