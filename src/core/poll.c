#include "poll.h"
#include "process.h"
#include "../platform/proc_platform.h"

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#include <sys/event.h>
#include <mach/mach_time.h>
#else
#endif
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "fsm.h"

// double buffer — poll writes, UI reads, no mutex needed
static Snapshot       buffers[2];
static atomic_int     write_idx = 0;

// per-pid history slots — linear scan is fine at MAX_PROCESSES scale
static ProcessHistory histories[MAX_PROCESSES];
static int            history_count = 0;

static atomic_int     fail_count  = 0;
static int            kq          = -1;

// GCD queues — mapped to efficiency cores via QoS class
static dispatch_queue_t event_queue = NULL;  // blocks on kevent()
static dispatch_queue_t poll_queue  = NULL;  // does proc_pidinfo() work

// --- history helpers ---

static ProcessHistory *history_find_or_create(int pid) {
    for (int i = 0; i < history_count; i++)
        if (histories[i].pid == pid && histories[i].active)
            return &histories[i];
    if (history_count >= MAX_PROCESSES) return NULL;
    ProcessHistory *h = &histories[history_count++];
    memset(h, 0, sizeof(ProcessHistory));
    h->pid    = pid;
    h->active = true;
    return h;
}

static void history_push(int pid, float cpu_pct) {
    ProcessHistory *h = history_find_or_create(pid);
    if (!h) return;
    h->cpu_history[h->history_head] = cpu_pct;
    h->history_head = (h->history_head + 1) % HISTORY_LEN;
}

static void history_mark_dead(int pid) {
    for (int i = 0; i < history_count; i++)
        if (histories[i].pid == pid) histories[i].active = false;
}

// --- delta CPU ---

// previous ticks per pid for delta — parallel array to the write buffer
static uint64_t prev_ticks[MAX_PROCESSES];
static int      prev_pids[MAX_PROCESSES];
static int      prev_count   = 0;
static double   prev_time    = 0.0;

static uint64_t find_prev_ticks(int pid) {
    for (int i = 0; i < prev_count; i++)
        if (prev_pids[i] == pid) return prev_ticks[i];
    return 0; // first time seeing this pid — delta will be 0
}

static void compute_delta_cpu(Snapshot *snap) {
    double now     = snap->timestamp;
    double elapsed = now - prev_time;
    if (elapsed <= 0.0) elapsed = 0.001; // guard division by zero

    for (int i = 0; i < snap->count; i++) {
        ProcessInfo *p    = &snap->procs[i];
        uint64_t     prev = find_prev_ticks(p->pid);
        uint64_t     curr = p->cpu_ticks;

        // ticks are nanoseconds on macOS — convert delta to seconds
        double delta_sec = (double)(curr - prev) / 1e9;
        p->cpu_percent   = (float)(delta_sec / elapsed * 100.0);
        if (p->cpu_percent > 100.0f) p->cpu_percent = 100.0f;

        history_push(p->pid, p->cpu_percent);

        // update prev arrays for next snapshot
        prev_pids[i]   = p->pid;
        prev_ticks[i]  = curr;
    }

    prev_count = snap->count;
    prev_time  = now;
}

// --- kqueue registration ---

static void register_pid(int pid) {
    struct kevent ev;
    EV_SET(&ev, pid, EVFILT_PROC, EV_ADD | EV_ONESHOT,
           NOTE_EXIT | NOTE_FORK | NOTE_EXEC, 0, NULL);
    kevent(kq, &ev, 1, NULL, 0, NULL); // errors silently — pid may already be gone
}

static void register_all_pids(const Snapshot *snap) {
    for (int i = 0; i < snap->count; i++)
        register_pid(snap->procs[i].pid);
}

// --- core snapshot + swap ---

static void do_snapshot(void) {
    int wi = atomic_load(&write_idx);
    Snapshot *buf = &buffers[wi];

    int result = proc_get_snapshot(buf);
    if (result < 0) {
    if (atomic_fetch_add(&fail_count, 1) >= 2)
        fsm_transition(EVT_SNAPSHOT_FAIL);
        return;
    }

    atomic_store(&fail_count, 0);
    fsm_transition(EVT_SNAPSHOT_OK);
    compute_delta_cpu(buf);
    register_all_pids(buf); // register any new pids that appeared

    atomic_store(&write_idx, 1 - wi); // atomic swap — UI now reads new buffer

}

// --- kqueue event loop — runs on event_queue (efficiency cores) ---

static void event_loop(void) {
    struct kevent ev;
    while (true) {
        // blocking wait — zero CPU until OS fires an event
        int n = kevent(kq, NULL, 0, &ev, 1, NULL);
        if (n <= 0) continue;

        if (ev.filter == EVFILT_PROC) {
            if (ev.fflags & NOTE_FORK) {
                // child spawned — register the child pid too
                register_pid((int)ev.data);
            }
            if (ev.fflags & NOTE_EXIT) {
                history_mark_dead((int)ev.ident);
            }
            // any proc event triggers a fresh snapshot on the poll queue
            dispatch_async(poll_queue, ^{ do_snapshot(); });
        }
    }
}

// --- public API ---

void poll_init(void) {
    memset(buffers,   0, sizeof(buffers));
    memset(histories, 0, sizeof(histories));
    memset(prev_ticks, 0, sizeof(prev_ticks));
    memset(prev_pids,  0, sizeof(prev_pids));

    kq = kqueue();
    if (kq < 0) { fsm_transition(EVT_SNAPSHOT_FAIL); return; }

    // efficiency core queues
    dispatch_queue_attr_t bg_attr = dispatch_queue_attr_make_with_qos_class(
        DISPATCH_QUEUE_SERIAL, QOS_CLASS_BACKGROUND, 0
    );
    dispatch_queue_attr_t ut_attr = dispatch_queue_attr_make_with_qos_class(
        DISPATCH_QUEUE_SERIAL, QOS_CLASS_UTILITY, 0
    );

    event_queue = dispatch_queue_create("com.pulsos.events", bg_attr);
    poll_queue  = dispatch_queue_create("com.pulsos.poll",   ut_attr);

    // take first snapshot synchronously so UI has data immediately
    do_snapshot();

    // start the kqueue event loop on the background queue
    dispatch_async(event_queue, ^{ event_loop(); });
}

void poll_shutdown(void) {
    if (kq >= 0) close(kq);
    if (event_queue) dispatch_release(event_queue);
    if (poll_queue)  dispatch_release(poll_queue);
}

const Snapshot *poll_read(void) {
    // UI reads the buffer poll is NOT currently writing to
    return &buffers[1 - atomic_load(&write_idx)];
}

const ProcessHistory *poll_history(int pid) {
    for (int i = 0; i < history_count; i++)
        if (histories[i].pid == pid && histories[i].active)
            return &histories[i];
    return NULL;
}

AppState poll_state(void) { return fsm_state(); }