#ifndef MMPS2_OS_H
#define MMPS2_OS_H

/*
 * PS2 port OS layer.
 *
 * The game ships with its own libultra headers (include/PR/*). We must use
 * those exact struct layouts (OSThread/OSMesgQueue) so the rest of the game
 * code links and agrees on ABI. This header therefore includes ultra64.h and
 * only declares the platform-private hooks. All private bookkeeping the shim
 * needs (EE thread id, semaphore handles, thread entry/arg/stack) is kept in
 * parallel tables inside mmps2_os.c instead of adding fields to the game
 * structs.
 */

#include "ultra64.h"

/* Default stack size assumed when only a stack top is supplied. */
#define MMPS2_DEFAULT_STACK_SIZE (16 * 1024)

/* Called once before any queue/thread is used. */
void mmps2_os_init(void);

/* Called from the VSync (retrace) interrupt each frame. Dispatches the
 * registered OS_EVENT_VI / OS_EVENT_COUNTER messages. */
void mmps2_os_retrace(void);

#endif /* MMPS2_OS_H */
