#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// max simultaneous tracked processes — fits entirely in M4 L2 cache
#define MAX_PROCESSES 1024
// 60 samples at variable kqueue intervals ≈ ~30s of history
#define HISTORY_LEN   60

// one process — filled by platform backend on every snapshot
typedef struct {
    int      pid;
    int      ppid;           // parent pid — for future process tree view
    char     name[256];      // short process name (basename)
    char     path[1024];     // full executable path
    float    cpu_percent;    // computed delta between two snapshots
    uint64_t cpu_ticks;      // raw cumulative ticks (user + system ns) for delta
    size_t   mem_rss;        // resident set size bytes — physical RAM used
    size_t   mem_vms;        // virtual memory size bytes — address space
    bool     alive;          // false = process exited since last snapshot
} ProcessInfo;

// full system snapshot — one per kqueue event, swapped via double buffer
typedef struct {
    ProcessInfo procs[MAX_PROCESSES];
    int         count;
    double      timestamp;   // mach_absolute_time converted to seconds
} Snapshot;

// per-pid history — ring buffer of cpu% samples for sparkline rendering
typedef struct {
    float cpu_history[HISTORY_LEN];
    int   history_head;      // index of next write position
    int   pid;               // which pid this history belongs to
    bool  active;            // slot in use
} ProcessHistory;

// app FSM — one source of truth for what the app is doing
typedef enum {
    STATE_LOADING,           // waiting for first kqueue snapshot
    STATE_RUNNING,           // normal operation, list visible
    STATE_SELECTED,          // user selected a pid, detail panel open
    STATE_ERROR              // kqueue or snapshot failed 3x
} AppState;

// treemap node — written by CPU squarified layout, read by Metal kernel
// layout: normalized coords (0..1), Metal maps to screen pixels
typedef struct {
    float    x, y, w, h;    // normalized rectangle
    float    cpu_percent;    // 0..100 — drives heat color in shader
    float    mem_bytes;      // raw bytes — drives box area
    int      pid;            // so click hit-testing maps back to a process
} TreemapNode;

#ifdef __cplusplus
}
#endif