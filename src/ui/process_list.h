#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/process.h"

// sort state — owned by process_list.c
typedef struct {
    int  col; // 0=PID 1=CPU 2=MEM 3=Name
    bool asc;
} SortState;

// sorts a local copy of snap into out[], returns count
int  process_list_sort(const Snapshot *snap, ProcessInfo *out, int max, SortState *s);
void process_list_set_sort(SortState *s, int col);

// called from app_ui.cpp
void ui_process_list(int *selected_pid);

#ifdef __cplusplus
}
#endif