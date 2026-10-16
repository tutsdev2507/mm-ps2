/*
 * ROM + log file access for the PS2 port.
 *
 * Uses ps2sdk's fileXio RPC layer (fileXio_rpc.h), NOT the legacy fio API.
 * The `mass:` device is registered on the IOP's *iomanX*, which only fileXio
 * (or the POSIX newlib glue that routes to fileXio) can reach -- the old fio
 * talks to plain ioman and silently fails to open mass:.  fileXio needs the
 * iomanX/fileXio IOP modules present; launchers like uLaunchELF/OPL have
 * already loaded them.
 *
 * Sees NO decomp headers (avoids the uintptr_t/wint_t clash from HANDOFF.md),
 * so it is a port-context translation unit.
 */

/*
 * This TU uses fileXio* directly and nothing from newlib's FILE* stdio, so
 * the ps2sdk guard that forbids mixing fileXio with newlib does not apply.
 * NEWLIB_PORT_AWARE is the documented escape hatch that silences that guard.
 */
#define NEWLIB_PORT_AWARE

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <fileXio_rpc.h>

extern void mmps2_scr(const char *fmt, ...);
extern void mmps2_boot_color(int r, int g, int b);

static int s_io_ready;

static void io_init(void)
{
    if (s_io_ready)
        return;
    s_io_ready = 1;
    SifInitRpc(0);
    fileXioInit();
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
        fd = fileXioOpen(s_log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    } else {
        for (i = 0; k_log_paths[i] && fd < 0; i++) {
            fd = fileXioOpen(k_log_paths[i],
                             O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0)
                s_log_path = k_log_paths[i];
        }
    }
    if (fd >= 0) {
        fileXioWrite(fd, line, n);
        fileXioClose(fd);
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
        s_fd = fileXioOpen(k_rom_paths[i], O_RDONLY, 0);
        if (s_fd >= 0) {
            mmps2_log("rom: opened %s", k_rom_paths[i]);
            mmps2_scr("rom: opened %s", k_rom_paths[i]);
            /* GREEN: ROM found. */
            mmps2_boot_color(0, 0xC0, 0);
            return 0;
        }
    }
    mmps2_log("rom: ERROR baserom.z64 not found (expected mass:/MMPS2/baserom.z64)");
    mmps2_scr("rom: ERROR baserom.z64 not found");
    /* RED: no ROM on the drive. */
    mmps2_boot_color(0xC0, 0, 0);
    return -1;
}

/* fileXio DMA wants cache-line aligned buffers; destinations from the game
 * are not, so read through an aligned bounce buffer. */
static u8 s_bounce[0x10000] __attribute__((aligned(64)));

/* Reads `size` bytes at ROM offset `off`; zero-fills any shortfall.
 * Returns bytes actually read. */
int mmps2_rom_read(unsigned int off, void *buf, unsigned int size)
{
    u8 *dst = (u8 *)buf;
    unsigned int done = 0;

    if (mmps2_rom_open() < 0 || fileXioLseek(s_fd, (int)off, SEEK_SET) < 0) {
        memset(buf, 0, size);
        return 0;
    }
    while (done < size) {
        unsigned int chunk = size - done;
        int n;
        if (chunk > sizeof(s_bounce))
            chunk = sizeof(s_bounce);
        n = fileXioRead(s_fd, s_bounce, (int)chunk);
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
