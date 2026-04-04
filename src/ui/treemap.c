#include "treemap.h"
#include "../core/poll.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float x, y, w, h; } Rect;

static float worst_ratio(float *vals, int n, float strip_w) {
    float s = 0;
    for (int i = 0; i < n; i++) s += vals[i];
    if (s == 0 || strip_w == 0) return 1e9f;
    float rmax = vals[0];
    float rmin = vals[n - 1];
    float a = (strip_w * strip_w * rmax) / (s * s);
    float b = (s * s) / (strip_w * strip_w * rmin);
    return fmaxf(a, b);
}

static void layout_row(float *vals, int *pids, int n,
                        Rect rect, float total, int *out_idx,
                        TreemapNode *out) {
    float s = 0;
    for (int i = 0; i < n; i++) s += vals[i];

    bool horiz     = rect.w >= rect.h;
    float short_s  = horiz ? rect.h : rect.w;
    float strip    = (total > 0) ? (s / total) * short_s : 0;
    float pos      = horiz ? rect.y : rect.x;

    for (int i = 0; i < n; i++) {
        float frac      = (s > 0) ? vals[i] / s : 0;
        TreemapNode *nd = &out[(*out_idx)++];
        if (horiz) {
            nd->x = rect.x; nd->y = pos; nd->w = strip; nd->h = frac * rect.h;
            pos += nd->h;
        } else {
            nd->x = pos; nd->y = rect.y; nd->w = frac * rect.w; nd->h = strip;
            pos += nd->w;
        }
        nd->pid         = pids[i];
        nd->cpu_percent = 0;
        nd->mem_bytes   = vals[i];
    }
    (void)strip;
}

static void squarify(float *vals, int *pids, int n,
                     Rect rect, int *out_idx, TreemapNode *out) {
    if (n == 0) return;
    float total = 0;
    for (int i = 0; i < n; i++) total += vals[i];
    if (total == 0) return;

    float strip_w = fminf(rect.w, rect.h);
    int   row_end = 0;
    float row_vals[MAX_PROCESSES];

    for (int i = 0; i < n; i++) {
        row_vals[row_end] = vals[i];
        float curr_worst  = worst_ratio(row_vals, row_end + 1, strip_w);
        float prev_worst  = (row_end > 0)
            ? worst_ratio(row_vals, row_end, strip_w) : 1e9f;

        if (row_end > 0 && curr_worst > prev_worst) {
            float row_sum = 0;
            for (int j = 0; j < row_end; j++) row_sum += row_vals[j];
            float short_s = fminf(rect.w, rect.h);
            float strip2  = (total > 0) ? (row_sum / total) * short_s : 0;

            layout_row(row_vals, pids, row_end, rect, total, out_idx, out);

            Rect next = rect;
            if (rect.w >= rect.h) { next.x += strip2; next.w -= strip2; }
            else                  { next.y += strip2; next.h -= strip2; }

            squarify(vals + i, pids + i, n - i, next, out_idx, out);
            return;
        }
        row_end++;
    }
    layout_row(row_vals, pids, row_end, rect, total, out_idx, out);
}

static int cmp_mem_desc(const void *a, const void *b) {
    const ProcessInfo *pa = (const ProcessInfo *)a;
    const ProcessInfo *pb = (const ProcessInfo *)b;
    return (pb->mem_rss > pa->mem_rss) - (pb->mem_rss < pa->mem_rss);
}

int treemap_build(const Snapshot *snap, TreemapNode *out, int max) {
    static ProcessInfo sorted[MAX_PROCESSES];
    int count = snap->count < max ? snap->count : max;
    memcpy(sorted, snap->procs, sizeof(ProcessInfo) * count);
    qsort(sorted, count, sizeof(ProcessInfo), cmp_mem_desc);

    static float vals[MAX_PROCESSES];
    static int   pids[MAX_PROCESSES];
    for (int i = 0; i < count; i++) {
        vals[i] = (float)sorted[i].mem_rss;
        pids[i] = sorted[i].pid;
    }

    int out_idx = 0;
    Rect root   = {0, 0, 1, 1};
    squarify(vals, pids, count, root, &out_idx, out);

    // fill cpu_percent from snapshot
    for (int i = 0; i < out_idx; i++)
        for (int j = 0; j < snap->count; j++)
            if (snap->procs[j].pid == out[i].pid) {
                out[i].cpu_percent = snap->procs[j].cpu_percent;
                break;
            }

    return out_idx;
}

int treemap_hit(const TreemapNode *nodes, int count, float nx, float ny) {
    for (int i = 0; i < count; i++) {
        const TreemapNode *nd = &nodes[i];
        if (nx >= nd->x && nx <= nd->x + nd->w &&
            ny >= nd->y && ny <= nd->y + nd->h)
            return nd->pid;
    }
    return -1;
}