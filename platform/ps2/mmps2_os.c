#include "mmps2_os.h"

#include <string.h>
#include <kernel.h>

extern void *_gp;

#define MMPS2_MAX_QUEUES  128
#define MMPS2_MAX_THREADS 64

/* Event registrations (osSetEventMesg). */
static OSMesgQueue *s_evtQueue[OS_NUM_EVENTS];
static OSMesg       s_evtMsg[OS_NUM_EVENTS];

/* Free-running retrace counter, used for osGetTime/osGetCount. */
static volatile u64 s_retraceCount;

/* ------------------------------------------------------------------ */
/* Parallel bookkeeping tables                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    OSMesgQueue *mq;
    s32          sema;
} Mmps2QueueInfo;

static Mmps2QueueInfo s_queues[MMPS2_MAX_QUEUES];
static s32            s_numQueues;

static Mmps2QueueInfo *queue_info(OSMesgQueue *mq)
{
    s32 i;
    for (i = 0; i < s_numQueues; i++)
        if (s_queues[i].mq == mq)
            return &s_queues[i];
    return NULL;
}

typedef struct {
    OSThread *t;
    void     *entry;
    void     *arg;
    void     *stack;
    s32       stackSize;
    s32       eeTid;
} Mmps2ThreadInfo;

static Mmps2ThreadInfo s_threads[MMPS2_MAX_THREADS];
static s32             s_numThreads;

static Mmps2ThreadInfo *thread_info(OSThread *t)
{
    s32 i;
    for (i = 0; i < s_numThreads; i++)
        if (s_threads[i].t == t)
            return &s_threads[i];
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Message queues                                                      */
/* ------------------------------------------------------------------ */

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count)
{
    Mmps2QueueInfo *info;
    ee_sema_t s;

    memset(mq, 0, sizeof(*mq));
    mq->msg = msg;
    mq->msgCount = count;

    memset(&s, 0, sizeof(s));
    s.init_count = 0;
    s.max_count = count;

    info = &s_queues[s_numQueues++];
    info->mq = mq;
    info->sema = CreateSema(&s);
}

static s32 mq_full(const OSMesgQueue *mq)
{
    return mq->validCount >= mq->msgCount;
}

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags)
{
    Mmps2QueueInfo *info = queue_info(mq);

    if (mq_full(mq)) {
        if (flags == OS_MESG_NOBLOCK)
            return -1;
        while (mq_full(mq))
            ;
    }

    mq->msg[(mq->first + mq->validCount) % mq->msgCount] = msg;
    mq->validCount++;
    if (info != NULL && info->sema >= 0)
        iSignalSema(info->sema);
    return 0;
}

s32 osJamMesg(OSMesgQueue *mq, OSMesg msg, s32 flags)
{
    Mmps2QueueInfo *info = queue_info(mq);

    if (mq_full(mq)) {
        if (flags == OS_MESG_NOBLOCK)
            return -1;
        while (mq_full(mq))
            ;
    }

    mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
    mq->msg[mq->first] = msg;
    mq->validCount++;
    if (info != NULL && info->sema >= 0)
        iSignalSema(info->sema);
    return 0;
}

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags)
{
    Mmps2QueueInfo *info = queue_info(mq);
    s32 sema = (info != NULL) ? info->sema : -1;

    if (flags == OS_MESG_BLOCK) {
        if (sema >= 0)
            WaitSema(sema);
        else
            while (mq->validCount == 0)
                ;
    } else {
        if (mq->validCount == 0)
            return -1;
        if (sema >= 0 && PollSema(sema) < 0)
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
    Mmps2ThreadInfo *info = (Mmps2ThreadInfo *)arg;
    OSThread *t = info->t;

    t->state = OS_STATE_RUNNING;
    ((void (*)(void *))info->entry)(info->arg);

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

/*
 * Stack resolution.
 *
 * libultra is given only the *top* of a thread's stack; the real bounds live
 * in the game's StackEntry, which StackCheck_Init() registered just before
 * osCreateThread() was called with the same top.  Walking that list lets us
 * run each EE thread inside exactly the region the game reserved, so
 * StackCheck_GetState() keeps reporting reality.
 *
 * Threads created without a StackEntry (osCreatePiManager) fall back to a
 * private pool so the EE stack can never spill into a neighbouring object.
 */
#include "libu64/stackcheck.h"

extern StackEntry *sStackInfoListStart;

#define MMPS2_FALLBACK_STACK 0x4000
#define MMPS2_FALLBACK_SLOTS 12

static u8 s_fallbackStacks[MMPS2_FALLBACK_SLOTS][MMPS2_FALLBACK_STACK]
    __attribute__((aligned(16)));
static s32 s_fallbackUsed;

static u8 *alloc_fallback_stack(void)
{
    if (s_fallbackUsed >= MMPS2_FALLBACK_SLOTS)
        return NULL;
    return s_fallbackStacks[s_fallbackUsed++];
}

/*
 * Pick the storage the EE thread will actually run on.
 *
 * The game's own stacks are tiny by PS2 standards (the main thread gets
 * 0x900 bytes) and our shims push far deeper, so a thread always runs on a
 * 16 KiB slot from the private pool.  When the game registered a StackEntry
 * for this stack the entry is repointed at the slot, so StackCheck keeps
 * measuring the memory that is really in use instead of an untouched
 * placeholder.
 */
static int resolve_stack(void *sp, StackEntry **entryOut, void **bottomOut, s32 *sizeOut)
{
    StackEntry *e;
    u8 *slot;

    *entryOut = NULL;

    for (e = sStackInfoListStart; e != NULL; e = e->next) {
        if (e->tail == sp && e->head != NULL) {
            *entryOut = e;
            break;
        }
    }

    slot = alloc_fallback_stack();
    if (slot == NULL)
        return 0;

    *bottomOut = slot;
    *sizeOut = MMPS2_FALLBACK_STACK;

    if (e != NULL) {
        u32 *p = (u32 *)slot;
        u32 *end = (u32 *)(slot + MMPS2_FALLBACK_STACK);

        while (p < end)
            *p++ = e->initValue;
        e->head = slot;
        e->tail = slot + MMPS2_FALLBACK_STACK;
    }
    return 1;
}

void osCreateThread(OSThread *t, OSId id, void *entry, void *arg, void *sp, OSPri p)
{
    Mmps2ThreadInfo *info;
    ee_thread_t th;
    StackEntry *se = NULL;
    void *bottom;
    s32 size;

    t->id = id;
    t->priority = p;
    t->state = OS_STATE_STOPPED;
    t->flags = 0;

    info = &s_threads[s_numThreads++];
    info->t = t;
    info->entry = entry;
    info->arg = arg;
    info->stack = sp;
    info->eeTid = -1;

    if (!resolve_stack(sp, &se, &bottom, &size)) {
        info->stackSize = 0;
        t->state = OS_STATE_STOPPED;
        return;
    }

    info->stack = bottom;
    info->stackSize = size;

    memset(&th, 0, sizeof(th));
    th.func = (void *)thread_trampoline;
    th.stack = bottom;
    th.stack_size = (u32)size;
    th.gp_reg = &_gp;
    th.initial_priority = map_priority(p);
    info->eeTid = CreateThread(&th);
    if (info->eeTid > 0)
        t->state = OS_STATE_RUNNABLE;
}

void osStartThread(OSThread *t)
{
    Mmps2ThreadInfo *info = thread_info(t);
    if (info == NULL || info->eeTid < 0)
        return;
    t->state = OS_STATE_RUNNABLE;
    StartThread(info->eeTid, info);
}

void osDestroyThread(OSThread *t)
{
    Mmps2ThreadInfo *info = (t != NULL) ? thread_info(t) : NULL;
    if (info != NULL && info->eeTid >= 0) {
        DeleteThread(info->eeTid);
        info->eeTid = -1;
    }
    if (t != NULL)
        t->state = OS_STATE_STOPPED;
}

OSId osGetThreadId(OSThread *t)
{
    return t->id;
}

/* libultra passes a NULL thread to mean "the running thread" (used by
 * osCreatePiManager and DmaMgr_ProcessRequest); dereferencing it would fault.
 * Resolve it through the EE thread id -> OSThread table, and fall back to a
 * throwaway object so a mismatch can never turn into a bad write. */
static OSThread sFallbackThread;

static OSThread *cur_thread(OSThread *t)
{
    s32 i;
    int tid;

    if (t != NULL)
        return t;

    tid = GetThreadId();
    for (i = 0; i < s_numThreads; i++) {
        if (s_threads[i].eeTid >= 0 && s_threads[i].eeTid == tid)
            return s_threads[i].t;
    }
    return &sFallbackThread;
}

void osSetThreadPri(OSThread *t, OSPri p)
{
    t = cur_thread(t);
    Mmps2ThreadInfo *info = thread_info(t);
    t->priority = p;
    if (info != NULL && info->eeTid >= 0)
        ChangeThreadPriority(info->eeTid, map_priority(p));
}

OSPri osGetThreadPri(OSThread *t)
{
    return cur_thread(t)->priority;
}

void osYieldThread(void)
{
    RotateThreadReadyQueue(GetThreadId());
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
    s_numQueues = 0;
    s_numThreads = 0;
}

void mmps2_os_retrace(void)
{
    s_retraceCount++;

    if (s_evtQueue[OS_EVENT_VI] != NULL)
        osSendMesg(s_evtQueue[OS_EVENT_VI], s_evtMsg[OS_EVENT_VI], OS_MESG_NOBLOCK);

    if (s_evtQueue[OS_EVENT_COUNTER] != NULL)
        osSendMesg(s_evtQueue[OS_EVENT_COUNTER], s_evtMsg[OS_EVENT_COUNTER], OS_MESG_NOBLOCK);
}
