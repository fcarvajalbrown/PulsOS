#include "detail_panel.h"

const ProcessInfo *detail_find(const Snapshot *snap, int pid) {
    for (int i = 0; i < snap->count; i++)
        if (snap->procs[i].pid == pid) return &snap->procs[i];
    return NULL;
}