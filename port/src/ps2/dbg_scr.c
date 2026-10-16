/*
 * On-screen boot debug for the PS2 port.
 *
 * Uses libdebug's init_scr/scr_printf, which draw with the EE kernel's
 * built-in debug font straight into a framebuffer.  It needs NO IOP modules,
 * NO mass: device and NO file I/O -- so it works even when the ROM/log
 * loader cannot (which is exactly the "black screen, no log" case).
 *
 * Port-context translation unit (compiled without decomp headers).
 */
#include <stdio.h>
#include <stdarg.h>

/* libdebug (EE). Declared directly so we do not depend on <debug.h>. */
extern void init_scr(void);
extern void scr_printf(const char *, ...) __attribute__((format(printf, 1, 2)));
extern void scr_clear(void);
extern void scr_setbgcolor(unsigned int color);
extern void scr_setfontcolor(unsigned int color);
extern void scr_setXY(int x, int y);

static int s_inited;

void mmps2_scr_init(void)
{
    if (s_inited)
        return;
    s_inited = 1;
    init_scr();
    scr_setbgcolor(0x000000);
    scr_setfontcolor(0x00FF44); /* green-ish */
    scr_clear();
    scr_setXY(0, 0);
}

/* Print one line to the screen. Safe to call from any stage. */
void mmps2_scr(const char *fmt, ...)
{
    char buf[256];
    va_list ap;

    mmps2_scr_init();

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    scr_printf("%s\n", buf);
}
