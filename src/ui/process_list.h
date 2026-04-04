#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/process.h"

typedef struct {
    int  col; // 0=PID 1=CPU 2=MEM 3=Name
    bool asc;
} SortState;

int  process_list_sort(const Snapshot *snap, ProcessInfo *out, int max, SortState *s);
void process_list_set_sort(SortState *s, int col);
int  process_kill(int pid, int sig); // sends sig to pid, returns 0 on success

void ui_process_list(int *selected_pid);

#ifdef __cplusplus
}
#endif