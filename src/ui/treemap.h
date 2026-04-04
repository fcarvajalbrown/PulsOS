#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/process.h"
#include "../gpu/metal_context.h"

// builds normalized treemap nodes from snapshot, returns count
int treemap_build(const Snapshot *snap, TreemapNode *out, int max);

// hit-test normalized coords, returns pid or -1
int treemap_hit(const TreemapNode *nodes, int count, float nx, float ny);

// called from app_ui.cpp
void ui_treemap(MetalContext *metal, int *selected_pid);

#ifdef __cplusplus
}
#endif