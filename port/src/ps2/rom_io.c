/*
 * ROM + log file access for the PS2 port.
 *
 * Uses only ps2sdk's fio* RPC layer (fileio.h): it needs nothing from newlib
 * having been initialised by a crt0, which this port does not use.  Sees NO
 * decomp headers (avoids the uintptr_t/wint_t clash from HANDOFF.md).
 */

/*
 * This TU uses ps2sdk's self-contained fio* RPC API (SIF RPC to the IOP
 * "mass" device) and nothing from newlib's FILE* stdio, so the ps2sdk's
 * guard that forbids mixing fio with newlib does not apply.  Defining
 * NEWLIB_PORT_AWARE is the documented escape hatch that silences that guard.
 */
#define NEWLIB_PORT_AWARE

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <fileio.h>

static int s_io_ready;

static void io_init(void)
{
    if (s_io_ready)
        return;
    s_io_ready = 1;
    SifInitRpc(0);
    fioInit();
}

/* Tried in order; the first that opens wins. */
static const char *const k_rom_paths[] = {
    "mass:/MMPS2/baserom.z64",
    "mass0:/MMPS2/baserom.z64",
    "mass:/MMPS2/baserom-decompressed.z64",
    "mass0:/MMPS2/baserom-decompressed.z64",
    "mass:/APPS/MMPS2/baserom.z64",
    "mass0:/APPS/MMPS2/baserom.z64",
    "mass:/baserom.z64",
    "mass0:/baserom.z64",
    "host:baserom.z64",
    NULL,
};

static const char *const k_log_paths[] = {
    "mass:/MMPS2/mmps2.log",
    "mass0:/MMPS2/mmps2.log",
    "mass:/mmps2.log",
    "mass0:/mmps2.log",
    NULL,
};

/* Last good log path (so each line does not retry every device). */
static const char *s_log_path;

void mmps2_log(const char *fmt, ...)
{
    char line[256];
    va_list ap;
    int n, fd = -1, i;

    io_init();

    va_start(ap, fmt);
    n = vsnprintf(line, sizeof(line) - 1, fmt, ap);
    va_end(ap);
    if (n <= 0)
        return;
    if (n > (int)sizeof(line) - 2)
        n = (int)sizeof(line) - 2;
    line[n++] = '\n';

    if (s_log_path) {
        fd = fioOpen(s_log_path, FIO_O_WRONLY | FIO_O_CREAT | FIO_O_APPEND);
    } else {
        for (i = 0; k_log_paths[i] && fd < 0; i++) {
            fd = fioOpen(k_log_paths[i],
                         FIO_O_WRONLY | FIO_O_CREAT | FIO_O_TRUNC);
            if (fd >= 0)
                s_log_path = k_log_paths[i];
        }
    }
    if (fd >= 0) {
        fioWrite(fd, line, n);
        fioClose(fd);
    }
}

static int s_fd = -1;
static int s_tried;

/* Returns 0 on success, -1 if no ROM could be opened. */
int mmps2_rom_open(void)
{
    int i;

    if (s_fd >= 0)
        return 0;
    if (s_tried)
        return -1; /* do not hammer USB on every DMA after a failure */
    s_tried = 1;
    io_init();
    for (i = 0; k_rom_paths[i]; i++) {
        s_fd = fioOpen(k_rom_paths[i], FIO_O_RDONLY);
        if (s_fd >= 0) {
            mmps2_log("rom: opened %s", k_rom_paths[i]);
            return 0;
        }
    }
    mmps2_log("rom: ERROR baserom.z64 not found (expected mass:/MMPS2/baserom.z64)");
    return -1;
}

/* SIF DMA wants cache-line aligned buffers; destinations from the game are
 * not, so read through an aligned bounce buffer. */
static u8 s_bounce[0x10000] __attribute__((aligned(64)));

/* Reads `size` bytes at ROM offset `off`; zero-fills any shortfall.
 * Returns bytes actually read. */
int mmps2_rom_read(unsigned int off, void *buf, unsigned int size)
{
    u8 *dst = (u8 *)buf;
    unsigned int done = 0;

    if (mmps2_rom_open() < 0 || fioLseek(s_fd, (int)off, 0) < 0) {
        memset(buf, 0, size);
        return 0;
    }
    while (done < size) {
        unsigned int chunk = size - done;
        int n;
        if (chunk > sizeof(s_bounce))
            chunk = sizeof(s_bounce);
        n = fioRead(s_fd, s_bounce, (int)chunk);
        if (n <= 0)
            break;
        memcpy(dst + done, s_bounce, (unsigned int)n);
        done += (unsigned int)n;
    }
    if (done < size) {
        mmps2_log("rom: short read off=%08x want=%u got=%u", off, size, done);
        memset(dst + done, 0, size - done);
    }
    return (int)done;
}
