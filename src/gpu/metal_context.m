#include "metal_context.h"

#include <Foundation/Foundation.h>
#include <Metal/Metal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// all Metal state in one place
struct MetalContext {
    id<MTLDevice>              device;
    id<MTLCommandQueue>        queue;
    id<MTLComputePipelineState> color_pipeline;
    id<MTLBuffer>              node_buf;   // CPU writes TreemapNode[] here
    id<MTLBuffer>              color_buf;  // GPU writes float4[] here, CPU reads back
    int                        buf_capacity; // current allocated node slots
};

// grow or create shared MTLBuffers to fit node_count
static bool ensure_buffers(MetalContext *ctx, int node_count) {
    if (node_count <= ctx->buf_capacity) return true;

    // release old buffers if they exist
    if (ctx->node_buf)  [ctx->node_buf  release];
    if (ctx->color_buf) [ctx->color_buf release];

    NSUInteger node_bytes  = sizeof(TreemapNode) * node_count;
    NSUInteger color_bytes = sizeof(float) * 4 * node_count; // float4 per node

    // MTLResourceStorageModeShared — same physical memory for CPU and GPU on M4
    ctx->node_buf = [ctx->device
        newBufferWithLength:node_bytes
        options:MTLResourceStorageModeShared];

    ctx->color_buf = [ctx->device
        newBufferWithLength:color_bytes
        options:MTLResourceStorageModeShared];

    if (!ctx->node_buf || !ctx->color_buf) return false;

    ctx->buf_capacity = node_count;
    return true;
}

MetalContext *metal_init(void) {
    MetalContext *ctx = calloc(1, sizeof(MetalContext));
    if (!ctx) return NULL;

    // default GPU — the M4's unified memory GPU
    ctx->device = MTLCreateSystemDefaultDevice();
    if (!ctx->device) {
        fprintf(stderr, "pulsos: Metal not available\n");
        free(ctx);
        return NULL;
    }

    ctx->queue = [ctx->device newCommandQueue];
    if (!ctx->queue) {
        fprintf(stderr, "pulsos: failed to create Metal command queue\n");
        free(ctx);
        return NULL;
    }

    // load the precompiled .metallib from the path baked in at build time
    NSString *lib_path = [NSString stringWithUTF8String:METAL_LIB_PATH];
    NSError  *err      = nil;

    id<MTLLibrary> lib = [ctx->device
        newLibraryWithURL:[NSURL fileURLWithPath:lib_path]
        error:&err];

    if (!lib) {
        fprintf(stderr, "pulsos: failed to load metallib: %s\n",
                [[err localizedDescription] UTF8String]);
        free(ctx);
        return NULL;
    }

    id<MTLFunction> fn = [lib newFunctionWithName:@"color_map"];
    if (!fn) {
        fprintf(stderr, "pulsos: color_map kernel not found in metallib\n");
        free(ctx);
        return NULL;
    }

    ctx->color_pipeline = [ctx->device
        newComputePipelineStateWithFunction:fn
        error:&err];

    [fn  release];
    [lib release];

    if (!ctx->color_pipeline) {
        fprintf(stderr, "pulsos: failed to create pipeline: %s\n",
                [[err localizedDescription] UTF8String]);
        free(ctx);
        return NULL;
    }

    return ctx;
}

void metal_shutdown(MetalContext *ctx) {
    if (!ctx) return;
    if (ctx->node_buf)       [ctx->node_buf       release];
    if (ctx->color_buf)      [ctx->color_buf      release];
    if (ctx->color_pipeline) [ctx->color_pipeline release];
    if (ctx->queue)          [ctx->queue          release];
    if (ctx->device)         [ctx->device         release];
    free(ctx);
}

bool metal_compute_colors(MetalContext *ctx,
                           const TreemapNode *nodes, int node_count,
                           float *out_colors) {
    if (!ctx || !nodes || node_count <= 0) return false;

    if (!ensure_buffers(ctx, node_count)) return false;

    // zero-copy write — CPU writes directly into the shared MTLBuffer contents
    memcpy([ctx->node_buf contents], nodes, sizeof(TreemapNode) * node_count);

    id<MTLCommandBuffer>        cmd = [ctx->queue commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];

    [enc setComputePipelineState:ctx->color_pipeline];
    [enc setBuffer:ctx->node_buf  offset:0 atIndex:0];
    [enc setBuffer:ctx->color_buf offset:0 atIndex:1];

    // dispatch one thread per treemap node
    NSUInteger thread_width = ctx->color_pipeline.maxTotalThreadsPerThreadgroup;
    if ((NSUInteger)node_count < thread_width)
        thread_width = node_count;

    MTLSize grid  = MTLSizeMake(node_count, 1, 1);
    MTLSize group = MTLSizeMake(thread_width, 1, 1);

    [enc dispatchThreads:grid threadsPerThreadgroup:group];
    [enc endEncoding];

    // synchronous wait — acceptable at ~60fps for ≤1024 nodes, completes in µs
    [cmd commit];
    [cmd waitUntilCompleted];

    // read colors back — still zero-copy, color_buf contents is the same memory
    memcpy(out_colors, [ctx->color_buf contents], sizeof(float) * 4 * node_count);

    return true;
}