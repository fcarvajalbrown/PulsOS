#include <metal_stdlib>
using namespace metal;

struct TreemapNode {
    float x, y, w, h;
    float cpu_percent;
    float mem_bytes;
    int   pid;
};

kernel void color_map(
    device const TreemapNode *nodes [[buffer(0)]],
    device float4            *colors [[buffer(1)]],
    uint id [[thread_position_in_grid]]
) {
    float t = clamp(nodes[id].cpu_percent / 100.0, 0.0, 1.0);

    float3 cold = float3(0.20, 0.40, 1.00);
    float3 warm = float3(1.00, 0.85, 0.00);
    float3 hot  = float3(1.00, 0.10, 0.10);

    float3 color;
    if (t < 0.5)
        color = mix(cold, warm, t * 2.0);
    else
        color = mix(warm, hot, (t - 0.5) * 2.0);

    float mem_scale = clamp(nodes[id].mem_bytes / (256.0 * 1024.0 * 1024.0), 0.3, 1.0);
    color *= mem_scale;

    colors[id] = float4(color, 1.0);
}