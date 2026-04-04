#pragma once

#include "../core/process.h"

#include <Metal/Metal.h>
#include <stdbool.h>

// opaque context — all Metal objects live here
typedef struct MetalContext MetalContext;

MetalContext *metal_init(void);
void          metal_shutdown(MetalContext *ctx);

// upload treemap nodes to shared MTLBuffer and run color kernel
// out_colors must point to a float4 array of at least node_count elements
bool metal_compute_colors(MetalContext *ctx,
                          const TreemapNode *nodes, int node_count,
                          float *out_colors);