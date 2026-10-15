#ifndef MMPS2_OS_H
#define MMPS2_OS_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;
typedef float f32;

#define OS_MESG_NOBLOCK 0
#define OS_MESG_BLOCK 1

#define OS_NUM_EVENTS 15
#define OS_EVENT_COUNTER 3
#define OS_EVENT_VI 7

#define OS_STATE_STOPPED (1 << 0)
#define OS_STATE_RUNNABLE (1 << 1)
#define OS_STATE_RUNNING (1 << 2)
#define OS_STATE_WAITING (1 << 3)

typedef void *OSMesg;
typedef s32 OSEvent;
typedef s32 OSId;
typedef s32 OSPri;

typedef struct OSThread OSThread;
struct OSThread {
    OSThread *next;
    OSPri priority;
    OSThread **queue;
    OSThread *tlnext;
    u16 state;
    u16 flags;
    OSId id;
    s32 fp;
    void *thprof;
    /* platform-private fields (replace the N64 context on PS2) */
    void *entry;
    void *arg;
    void *stack;
    s32 stackSize;
    s32 eeTid;
};

typedef struct OSMesgQueue {
    OSThread *mtQueue;
    OSThread *fullQueue;
    s32 validCount;
    s32 first;
    s32 msgCount;
    OSMesg *msg;
    /* platform-private */
    s32 sema;
} OSMesgQueue;

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
s32 osJamMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
void osSetEventMesg(OSEvent e, OSMesgQueue *mq, OSMesg msg);

void osCreateThread(OSThread *t, OSId id, void *entry, void *arg, void *sp, OSPri p);
void osStartThread(OSThread *t);
void osDestroyThread(OSThread *t);
OSId osGetThreadId(OSThread *t);
void osSetThreadPri(OSThread *t, OSPri p);
OSPri osGetThreadPri(OSThread *t);

u64 osGetTime(void);
u32 osGetCount(void);

/* Default stack size assumed when only a stack top is supplied. */
#define MMPS2_DEFAULT_STACK_SIZE (16 * 1024)

/* Platform hooks ------------------------------------------------------- */

/* Called once before any queue/thread is used. No-op placeholder today. */
void mmps2_os_init(void);

/* Called from the VSync (retrace) interrupt each frame. Dispatches the
 * registered OS_EVENT_VI / OS_EVENT_COUNTER messages. */
void mmps2_os_retrace(void);

#endif /* MMPS2_OS_H */
