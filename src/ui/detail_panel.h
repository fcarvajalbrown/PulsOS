#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/process.h"

// finds process info for pid in snap, returns NULL if gone
const ProcessInfo *detail_find(const Snapshot *snap, int pid);

// called from app_ui.cpp
void ui_detail_panel(int pid);

#ifdef __cplusplus
}
#endif