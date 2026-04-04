#include "treemap.h"
#include "../core/poll.h"
#include "../core/process.h"

#include <cimgui.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// working arrays — static to avoid stack blowout
static TreemapNode nodes[MAX_PROCESSES];
static float       colors[MAX_PROCESSES * 4]; // rgba per node
static int         node_count = 0;

// --- squarified treemap ---

typedef struct { float x, y, w, h; } Rect;

// worst aspect ratio in a candidate row — drives commit decision
static float worst_ratio(float *vals, int n, float strip_w) {
    float s = 0;
    for (int i = 0; i < n; i++) s += vals[i];
    if (s == 0 || strip_w == 0) return 1e9f;
    float rmax = vals[0]; // pre-sorted descending
    float rmin = vals[n - 1];
    float a = (strip_w * strip_w * rmax) / (s * s);
    float b = (s * s) / (strip_w * strip_w * rmin);
    return fmaxf(a, b);
}

// layout one committed row into rect, write to nodes[], advance out_idx
static void layout_row(float *vals, int *pids, int n,
                        Rect rect, float total, int *out_idx) {
    float s = 0;
    for (int i = 0; i < n; i++) s += vals[i];

    // row is horizontal if rect is wider than tall, vertical otherwise
    bool horiz = rect.w >= rect.h;
    float strip = horiz ? (s / total) * rect.h : (s / total) * rect.w;
    float pos   = horiz ? rect.x : rect.y;

    for (int i = 0; i < n; i++) {
        float frac = (s > 0) ? vals[i] / s : 0;
        TreemapNode *nd = &nodes[(*out_idx)++];
        if (horiz) {
            nd->x = pos;
            nd->y = rect.y;
            nd->w = strip;
            nd->h = frac * rect.h;
            pos  += nd->h; // advance along height
        } else {
            nd->x = rect.x;
            nd->y = pos;
            nd->w = frac * rect.w;
            nd->h = strip;
            pos  += nd->w;
        }
        nd->pid         = pids[i];
        nd->cpu_percent = 0; // filled after layout from snapshot
        nd->mem_bytes   = vals[i];
    }

    // shrink rect by the committed strip
    // (caller advances rect for next row)
}

// squarify — recursive subdivision
static void squarify(float *vals, int *pids, int n,
                     Rect rect, int *out_idx) {
    if (n == 0) return;

    float total = 0;
    for (int i = 0; i < n; i++) total += vals[i];
    if (total == 0) return;

    float strip_w = fminf(rect.w, rect.h); // shorter side drives aspect ratio
    int   row_end = 0;
    float row_vals[MAX_PROCESSES];

    for (int i = 0; i < n; i++) {
        row_vals[row_end] = vals[i];
        float curr_worst = worst_ratio(row_vals, row_end + 1, strip_w);
        float prev_worst = (row_end > 0)
            ? worst_ratio(row_vals, row_end, strip_w)
            : 1e9f;

        if (row_end > 0 && curr_worst > prev_worst) {
            // adding this element made things worse — commit current row
            float row_sum = 0;
            for (int j = 0; j < row_end; j++) row_sum += row_vals[j];
            float strip = (row_sum / total) * strip_w;

            layout_row(row_vals, pids, row_end, rect, total, out_idx);

            // shrink rect by the committed strip
            Rect next = rect;
            if (rect.w >= rect.h) { next.y += strip; next.h -= strip; }
            else                  { next.x += strip; next.w -= strip; }

            squarify(vals + i, pids + i, n - i, next, out_idx);
            return;
        }
        row_end++;
    }

    // commit whatever remains
    layout_row(row_vals, pids, row_end, rect, total, out_idx);
}

// sort descending by mem for squarify input (largest first)
static int cmp_mem_desc(const void *a, const void *b) {
    const ProcessInfo *pa = (const ProcessInfo *)a;
    const ProcessInfo *pb = (const ProcessInfo *)b;
    return (pb->mem_rss > pa->mem_rss) - (pb->mem_rss < pa->mem_rss);
}

// cpu fallback color when Metal is unavailable — simple lerp blue→red
static ImU32 cpu_color_cpu(float pct) {
    float t = pct / 100.0f;
    return igColorConvertFloat4ToU32((ImVec4){t, 0.2f, 1.0f - t, 1.0f});
}

// --- main render ---

void ui_treemap(MetalContext *metal, int *selected_pid) {
    const Snapshot *snap = poll_read();
    if (!snap || snap->count == 0) { igText("No data."); return; }

    // sorted copy for squarify input
    static ProcessInfo sorted[MAX_PROCESSES];
    memcpy(sorted, snap->procs, sizeof(ProcessInfo) * snap->count);
    int count = snap->count;
    qsort(sorted, count, sizeof(ProcessInfo), cmp_mem_desc);

    // build input arrays for squarify
    static float vals[MAX_PROCESSES];
    static int   pids[MAX_PROCESSES];
    for (int i = 0; i < count; i++) {
        vals[i] = (float)sorted[i].mem_rss;
        pids[i] = sorted[i].pid;
    }

    // run squarified layout into nodes[]
    node_count = 0;
    Rect root  = {0, 0, 1, 1}; // normalized — scaled to pixels below
    squarify(vals, pids, count, root, &node_count);

    // fill cpu_percent into nodes from snapshot
    for (int i = 0; i < node_count; i++) {
        for (int j = 0; j < snap->count; j++) {
            if (snap->procs[j].pid == nodes[i].pid) {
                nodes[i].cpu_percent = snap->procs[j].cpu_percent;
                break;
            }
        }
    }

    // compute colors — Metal if available, CPU fallback otherwise
    bool has_metal = metal && metal_compute_colors(metal, nodes, node_count, colors);

    // get canvas size from ImGui
    ImVec2 canvas_pos  = igGetCursorScreenPos();
    ImVec2 canvas_size = igGetContentRegionAvail();
    if (canvas_size.x < 1 || canvas_size.y < 1) return;

    ImDrawList *dl = igGetWindowDrawList();

    for (int i = 0; i < node_count; i++) {
        TreemapNode *nd = &nodes[i];

        // denormalize to screen pixels
        float px = canvas_pos.x + nd->x * canvas_size.x;
        float py = canvas_pos.y + nd->y * canvas_size.y;
        float pw = nd->w * canvas_size.x;
        float ph = nd->h * canvas_size.y;

        // skip boxes too small to see or click
        if (pw < 2 || ph < 2) continue;

        ImU32 fill;
        if (has_metal) {
            // colors[] is float4 rgba from Metal kernel
            int ci = i * 4;
            fill = igColorConvertFloat4ToU32((ImVec4){
                colors[ci], colors[ci+1], colors[ci+2], colors[ci+3]
            });
        } else {
            fill = cpu_color_cpu(nd->cpu_percent);
        }

        // highlight selected
        if (*selected_pid == nd->pid)
            fill = igColorConvertFloat4ToU32((ImVec4){1, 1, 0, 1}); // yellow outline pid

        ImDrawList_AddRectFilled(dl,
            (ImVec2){px, py}, (ImVec2){px + pw, py + ph},
            fill, 0, 0);

        // 1px border
        ImDrawList_AddRect(dl,
            (ImVec2){px, py}, (ImVec2){px + pw, py + ph},
            igColorConvertFloat4ToU32((ImVec4){0,0,0,0.4f}), 0, 0, 1.0f);

        // label if box is big enough
        if (pw > 40 && ph > 16) {
            char label[64];
            snprintf(label, sizeof(label), "%s", sorted[i].name);
            ImDrawList_AddText_Vec2(dl, (ImVec2){px + 3, py + 3},
                0xFFFFFFFF, label, NULL);
        }
    }

    // invisible full-canvas button for click detection
    igSetCursorScreenPos(canvas_pos);
    igInvisibleButton("##treemap_click", canvas_size, 0);
    if (igIsItemClicked(0)) {
        ImVec2 mouse = igGetMousePos();
        float mx = (mouse.x - canvas_pos.x) / canvas_size.x;
        float my = (mouse.y - canvas_pos.y) / canvas_size.y;

        // hit test against normalized rects
        *selected_pid = -1;
        for (int i = 0; i < node_count; i++) {
            TreemapNode *nd = &nodes[i];
            if (mx >= nd->x && mx <= nd->x + nd->w &&
                my >= nd->y && my <= nd->y + nd->h) {
                *selected_pid = nd->pid;
                break;
            }
        }
    }
}