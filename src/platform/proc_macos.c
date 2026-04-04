#include "proc_platform.h"
#include "../core/process.h"

#include <libproc.h>
#include <mach/mach_time.h>
#include <string.h>
#include <stdio.h>

// fills a Snapshot from the current macOS process list via libproc
int proc_get_snapshot(Snapshot *out) {
    // get current time in seconds for snapshot timestamp
    mach_timebase_info_data_t tb;
    mach_timebase_info(&tb);
    uint64_t raw = mach_absolute_time();
    out->timestamp = (double)(raw * tb.numer / tb.denom) / 1e9;

    // enumerate all pids — buffer sized for MAX_PROCESSES
    int pids[MAX_PROCESSES];
    int bytes = proc_listallpids(pids, sizeof(pids));
    if (bytes <= 0) return -1;

    int count = bytes / sizeof(int);
    out->count = 0;

    for (int i = 0; i < count && out->count < MAX_PROCESSES; i++) {
        int pid = pids[i];
        if (pid == 0) continue; // skip kernel_task

        ProcessInfo *p = &out->procs[out->count];
        memset(p, 0, sizeof(ProcessInfo));
        p->pid   = pid;
        p->alive = true;

        // task info — cpu ticks + memory
        struct proc_taskinfo ti;
        int r = proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &ti, sizeof(ti));
        if (r <= 0) continue; // process may have exited between listallpids and here

        p->cpu_ticks = ti.pti_total_user + ti.pti_total_system; // nanoseconds
        p->mem_rss   = ti.pti_resident_size;
        p->mem_vms   = ti.pti_virtual_size;

        // bsd info — name + ppid
        struct proc_bsdinfo bi;
        r = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bi, sizeof(bi));
        if (r > 0) {
            strncpy(p->name, bi.pbi_name[0] ? bi.pbi_name : bi.pbi_comm, 255);
            p->ppid = bi.pbi_ppid;
        }

        // full executable path
        proc_pidpath(pid, p->path, sizeof(p->path));

        out->count++;
    }

    return out->count;
}