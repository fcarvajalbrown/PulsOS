#include "detail_panel.h"
#include "implot_wrapper.h"
#include "../core/poll.h"
#include "../core/process.h"

#include <cimgui.h>
#include <stdio.h>

void ui_detail_panel(int pid) {
    const Snapshot       *snap = poll_read();
    const ProcessHistory *hist = poll_history(pid);

    // find the process in current snapshot
    const ProcessInfo *info = NULL;
    for (int i = 0; i < snap->count; i++)
        if (snap->procs[i].pid == pid) { info = &snap->procs[i]; break; }

    if (!info) { igText("Process gone."); return; }

    // basic info row
    igText("PID: %d", info->pid);
    igSameLine(0, 20);
    igText("PPID: %d", info->ppid);
    igSameLine(0, 20);
    igText("CPU: %.1f%%", info->cpu_percent);
    igSameLine(0, 20);
    igText("RSS: %.1f MB", (float)info->mem_rss / (1024*1024));
    igSameLine(0, 20);
    igText("VMS: %.1f MB", (float)info->mem_vms / (1024*1024));

    igText("%s", info->path[0] ? info->path : "(path unavailable)");

    igSeparator();

    // cpu sparkline via C++ wrapper — keeps implot out of C compilation
    if (hist)
        implot_sparkline("##cpu_hist", hist->cpu_history,
                         HISTORY_LEN, hist->history_head, -1, -1);
}