#include "process_list.h"
#include "../core/poll.h"
#include "../core/process.h"

#include <cimgui.h>
#include <string.h>
#include <stdlib.h>

// sort state
static int  sort_col = 1; // default sort by cpu
static bool sort_asc = false;

static int cmp_cpu(const void *a, const void *b) {
    const ProcessInfo *pa = (const ProcessInfo *)a;
    const ProcessInfo *pb = (const ProcessInfo *)b;
    return sort_asc
        ? (pa->cpu_percent > pb->cpu_percent) - (pa->cpu_percent < pb->cpu_percent)
        : (pb->cpu_percent > pa->cpu_percent) - (pb->cpu_percent < pa->cpu_percent);
}

static int cmp_mem(const void *a, const void *b) {
    const ProcessInfo *pa = (const ProcessInfo *)a;
    const ProcessInfo *pb = (const ProcessInfo *)b;
    return sort_asc
        ? (pa->mem_rss > pb->mem_rss) - (pa->mem_rss < pb->mem_rss)
        : (pb->mem_rss > pa->mem_rss) - (pb->mem_rss < pa->mem_rss);
}

static int cmp_pid(const void *a, const void *b) {
    const ProcessInfo *pa = (const ProcessInfo *)a;
    const ProcessInfo *pb = (const ProcessInfo *)b;
    return sort_asc ? pa->pid - pb->pid : pb->pid - pa->pid;
}

static int cmp_name(const void *a, const void *b) {
    const ProcessInfo *pa = (const ProcessInfo *)a;
    const ProcessInfo *pb = (const ProcessInfo *)b;
    return sort_asc
        ? strncmp(pa->name, pb->name, 255)
        : strncmp(pb->name, pa->name, 255);
}

void ui_process_list(int *selected_pid) {
    const Snapshot *snap = poll_read();
    if (!snap || snap->count == 0) { igText("No data yet."); return; }

    // local copy so we can sort without touching the shared buffer
    static ProcessInfo local[MAX_PROCESSES];
    memcpy(local, snap->procs, sizeof(ProcessInfo) * snap->count);
    int count = snap->count;

    // sort
    switch (sort_col) {
        case 0: qsort(local, count, sizeof(ProcessInfo), cmp_pid);  break;
        case 1: qsort(local, count, sizeof(ProcessInfo), cmp_cpu);  break;
        case 2: qsort(local, count, sizeof(ProcessInfo), cmp_mem);  break;
        case 3: qsort(local, count, sizeof(ProcessInfo), cmp_name); break;
    }

    ImGuiTableFlags flags =
        ImGuiTableFlags_Borders      |
        ImGuiTableFlags_RowBg        |
        ImGuiTableFlags_ScrollY      |
        ImGuiTableFlags_SizingFixedFit;

    if (!igBeginTable("##procs", 4, flags, (ImVec2){0, 0}, 0)) return;

    // clickable column headers to sort
    igTableSetupScrollFreeze(0, 1);
    igTableSetupColumn("PID",  ImGuiTableColumnFlags_WidthFixed, 55,  0);
    igTableSetupColumn("CPU%", ImGuiTableColumnFlags_WidthFixed, 55,  0);
    igTableSetupColumn("MEM",  ImGuiTableColumnFlags_WidthFixed, 70,  0);
    igTableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0, 0);
    igTableHeadersRow();

    // handle header clicks for sorting
    for (int c = 0; c < 4; c++) {
        igTableSetColumnIndex(c);
        if (igIsItemClicked(0)) {
            if (sort_col == c) sort_asc = !sort_asc;
            else { sort_col = c; sort_asc = false; }
        }
    }

    for (int i = 0; i < count; i++) {
        ProcessInfo *p = &local[i];
        igTableNextRow(0, 0);

        bool is_selected = (*selected_pid == p->pid);
        igTableSetColumnIndex(0); igText("%d", p->pid);
        igTableSetColumnIndex(1); igText("%.1f", p->cpu_percent);
        igTableSetColumnIndex(2); igText("%.0f MB", (float)p->mem_rss / (1024*1024));
        igTableSetColumnIndex(3);

        // selectable spans the name cell, highlights the row
        char label[270];
        snprintf(label, sizeof(label), "%s##%d", p->name, p->pid);
        if (igSelectable_Bool(label, is_selected,
                ImGuiSelectableFlags_SpanAllColumns, (ImVec2){0, 0})) {
            *selected_pid = is_selected ? -1 : p->pid; // toggle
        }
    }

    igEndTable();
}