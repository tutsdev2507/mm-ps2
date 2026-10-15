#include "mmps2_os.h"

#include <string.h>
#include <kernel.h>

extern void *_gp;

/* Event registrations (OSSetEventMesg) */
static OSMesgQueue *s_evtQueue[OS_NUM_EVENTS];
static OSMesg s_evtMsg[OS_NUM_EVENTS];

/* Free-running retrace counter, used for osGetTime/osGetCount. */
static volatile u64 s_retraceCount;

/* ------------------------------------------------------------------ */
/* Message queues                                                      */
/* ------------------------------------------------------------------ */

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count)
{
    ee_sema_t s;

    memset(mq, 0, sizeof(*mq));
    mq->msg = msg;
    mq->msgCount = count;

    memset(&s, 0, sizeof(s));
    s.init_count = 0;
    s.max_count = count;
    mq->sema = CreateSema(&s);
}

static s32 mq_full(const OSMesgQueue *mq)
{
    return mq->validCount >= mq->msgCount;
}

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags)
{
    if (mq_full(mq)) {
        if (flags == OS_MESG_NOBLOCK)
            return -1;
        /* Blocking send to a full queue is not exercised yet. */
        while (mq_full(mq))
            DelayThread(100);
    }

    mq->msg[(mq->first + mq->validCount) % mq->msgCount] = msg;
    mq->validCount++;
    iSignalSema(mq->sema);
    return 0;
}

s32 osJamMesg(OSMesgQueue *mq, OSMesg msg, s32 flags)
{
    if (mq_full(mq)) {
        if (flags == OS_MESG_NOBLOCK)
            return -1;
        while (mq_full(mq))
            DelayThread(100);
    }

    mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
    mq->msg[mq->first] = msg;
    mq->validCount++;
    iSignalSema(mq->sema);
    return 0;
}

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags)
{
    if (flags == OS_MESG_BLOCK) {
        WaitSema(mq->sema);
    } else {
        if (mq->validCount == 0)
            return -1;
        if (PollSema(mq->sema) < 0)
            return -1;
    }

    if (msg != NULL)
        *msg = mq->msg[mq->first];

    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;
    return 0;
}

void osSetEventMesg(OSEvent e, OSMesgQueue *mq, OSMesg msg)
{
    if (e < 0 || e >= OS_NUM_EVENTS)
        return;
    s_evtQueue[e] = mq;
    s_evtMsg[e] = msg;
}

/* ------------------------------------------------------------------ */
/* Threads                                                             */
/* ------------------------------------------------------------------ */

static void thread_trampoline(void *arg)
{
    OSThread *t = (OSThread *)arg;

    t->state = OS_STATE_RUNNING;
    ((void (*)(void *))t->entry)(t->arg);

    /* A libultra thread that returns simply stops. */
    t->state = OS_STATE_STOPPED;
    ExitThread();
}

/* libultra: lower number = higher priority. ps2sdk: higher = higher. */
static int map_priority(OSPri p)
{
    if (p < 0)
        p = 0;
    if (p > 127)
        p = 127;
    return 127 - p;
}

void osCreateThread(OSThread *t, OSId id, void *entry, void *arg, void *sp, OSPri p)
{
    ee_thread_t th;

    memset(t, 0, sizeof(*t));
    t->id = id;
    t->priority = p;
    t->entry = entry;
    t->arg = arg;
    t->stack = sp;
    t->stackSize = MMPS2_DEFAULT_STACK_SIZE;
    t->state = OS_STATE_STOPPED;
    t->eeTid = -1;

    memset(&th, 0, sizeof(th));
    th.func = (void *)thread_trampoline;
    th.stack = (void *)((u8 *)sp - MMPS2_DEFAULT_STACK_SIZE);
    th.stack_size = MMPS2_DEFAULT_STACK_SIZE;
    th.gp_reg = &_gp;
    th.initial_priority = map_priority(p);
    t->eeTid = CreateThread(&th);
    if (t->eeTid > 0)
        t->state = OS_STATE_RUNNABLE;
}

void osStartThread(OSThread *t)
{
    if (t->eeTid < 0)
        return;
    t->state = OS_STATE_RUNNABLE;
    StartThread(t->eeTid, t);
}

void osDestroyThread(OSThread *t)
{
    if (t != NULL && t->eeTid >= 0) {
        DeleteThread(t->eeTid);
        t->eeTid = -1;
    }
    ExitThread();
}

OSId osGetThreadId(OSThread *t)
{
    return t->id;
}

void osSetThreadPri(OSThread *t, OSPri p)
{
    t->priority = p;
    if (t->eeTid >= 0)
        ChangeThreadPriority(t->eeTid, map_priority(p));
}

OSPri osGetThreadPri(OSThread *t)
{
    return t->priority;
}

/* ------------------------------------------------------------------ */
/* Time                                                                */
/* ------------------------------------------------------------------ */

u64 osGetTime(void)
{
    /* ~16683 microseconds per NTSC retrace. */
    return s_retraceCount * 16683ull;
}

u32 osGetCount(void)
{
    return (u32)s_retraceCount;
}

/* ------------------------------------------------------------------ */
/* Platform hooks                                                      */
/* ------------------------------------------------------------------ */

void mmps2_os_init(void)
{
    memset(s_evtQueue, 0, sizeof(s_evtQueue));
    memset(s_evtMsg, 0, sizeof(s_evtMsg));
    s_retraceCount = 0;
}

void mmps2_os_retrace(void)
{
    s_retraceCount++;

    if (s_evtQueue[OS_EVENT_VI] != NULL)
        osSendMesg(s_evtQueue[OS_EVENT_VI], s_evtMsg[OS_EVENT_VI], OS_MESG_NOBLOCK);

    if (s_evtQueue[OS_EVENT_COUNTER] != NULL)
        osSendMesg(s_evtQueue[OS_EVENT_COUNTER], s_evtMsg[OS_EVENT_COUNTER], OS_MESG_NOBLOCK);
}
