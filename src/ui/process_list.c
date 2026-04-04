#include "process_list.h"
#include "../core/poll.h"

#include <string.h>
#include <stdlib.h>

static SortState g_sort = {1, false}; // default: CPU descending

static int cmp_cpu(const void *a, const void *b) {
    const ProcessInfo *pa = (const ProcessInfo *)a;
    const ProcessInfo *pb = (const ProcessInfo *)b;
    return g_sort.asc
        ? (pa->cpu_percent > pb->cpu_percent) - (pa->cpu_percent < pb->cpu_percent)
        : (pb->cpu_percent > pa->cpu_percent) - (pb->cpu_percent < pa->cpu_percent);
}

static int cmp_mem(const void *a, const void *b) {
    const ProcessInfo *pa = (const ProcessInfo *)a;
    const ProcessInfo *pb = (const ProcessInfo *)b;
    return g_sort.asc
        ? (pa->mem_rss > pb->mem_rss) - (pa->mem_rss < pb->mem_rss)
        : (pb->mem_rss > pa->mem_rss) - (pb->mem_rss < pa->mem_rss);
}

static int cmp_pid(const void *a, const void *b) {
    const ProcessInfo *pa = (const ProcessInfo *)a;
    const ProcessInfo *pb = (const ProcessInfo *)b;
    return g_sort.asc ? pa->pid - pb->pid : pb->pid - pa->pid;
}

static int cmp_name(const void *a, const void *b) {
    const ProcessInfo *pa = (const ProcessInfo *)a;
    const ProcessInfo *pb = (const ProcessInfo *)b;
    return g_sort.asc
        ? strncmp(pa->name, pb->name, 255)
        : strncmp(pb->name, pa->name, 255);
}

void process_list_set_sort(SortState *s, int col) {
    if (s->col == col) s->asc = !s->asc;
    else { s->col = col; s->asc = false; }
    g_sort = *s;
}

int process_list_sort(const Snapshot *snap, ProcessInfo *out, int max, SortState *s) {
    int count = snap->count < max ? snap->count : max;
    memcpy(out, snap->procs, sizeof(ProcessInfo) * count);
    g_sort = *s;
    switch (s->col) {
        case 0: qsort(out, count, sizeof(ProcessInfo), cmp_pid);  break;
        case 1: qsort(out, count, sizeof(ProcessInfo), cmp_cpu);  break;
        case 2: qsort(out, count, sizeof(ProcessInfo), cmp_mem);  break;
        case 3: qsort(out, count, sizeof(ProcessInfo), cmp_name); break;
    }
    return count;
}