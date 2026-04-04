#pragma once

#include "../core/process.h"
#include <stdbool.h>

// opaque — Metal internals stay in the .m file, never leak into C
typedef struct MetalContext MetalContext;

MetalContext *metal_init(void);
void          metal_shutdown(MetalContext *ctx);

bool metal_compute_colors(MetalContext *ctx,
                          const TreemapNode *nodes, int node_count,
                          float *out_colors);