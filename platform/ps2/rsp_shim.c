/*
 * RSP shim: the N64's Reality Signal Processor has no equivalent on the EE,
 * so a completed RSP task is executed directly by the port's F3DEX2 display
 * list interpreter (port/src/gfx).
 *
 * This file must NOT include port/src/gfx/gfx.h: that header redefines the
 * GBI types (Gfx/Vtx/Mtx) that <ultra64.h> already declared, which is a hard
 * typedef clash.  The graphics context lives in runtime.c (which only knows
 * the port headers) and is reached through opaque C declarations; C does not
 * mangle names, so the signatures only have to be call-compatible.
 */

#include "mmps2_os.h"

/* runtime.c */
extern void mmps2_run_dl(void *dl);
extern void mmps2_log(const char *fmt, ...);
extern void mmps2_scr(const char *fmt, ...);

#ifndef OS_YIELD
#define OS_YIELD 1
#endif

static int s_yielded;

void osSpTaskLoad(OSTask *task)
{
    (void)task;
}

void osSpTaskStartGo(OSTask *task)
{
    static unsigned s_frames;
    s_yielded = 0;
    if (task == NULL)
        return;
    if (s_frames < 5 || (s_frames % 600) == 0)
        mmps2_log("gfx: task #%u type=%u dl=%08x", s_frames, (unsigned)task->t.type, (unsigned)(uintptr_t)task->t.data_ptr);
    if (s_frames == 0)
        mmps2_scr("gfx: first display list running");
    s_frames++;
    mmps2_run_dl((void *)(uintptr_t)task->t.data_ptr);
}

OSYieldResult osSpTaskYielded(OSTask *task)
{
    (void)task;
    return s_yielded ? (OSYieldResult)OS_YIELD : 0;
}

void osSpTaskYield(void)
{
    s_yielded = 1;
}

OSYieldResult osSpTaskGetYieldResult(OSTask *task)
{
    (void)task;
    return s_yielded ? (OSYieldResult)OS_YIELD : 0;
}
