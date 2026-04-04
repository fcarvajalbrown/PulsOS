#include "treemap.h"
#include "../core/poll.h"
#include "../core/process.h"

#include "imgui.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static TreemapNode nodes[MAX_PROCESSES];
static float       colors[MAX_PROCESSES * 4];
static int         node_count = 0;

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
                        Rect rect, float total, int *out_idx) {
    float s = 0;
    for (int i = 0; i < n; i++) s += vals[i];

    bool horiz = rect.w >= rect.h;
    float strip = horiz ? (s / total) * rect.h : (s / total) * rect.w;
    float pos   = horiz ? rect.x : rect.y;

    for (int i = 0; i < n; i++) {
        float frac = (s > 0) ? vals[i] / s : 0;
        TreemapNode *nd = &nodes[(*out_idx)++];
        if (horiz) {
            nd->x = pos; nd->y = rect.y; nd->w = strip; nd->h = frac * rect.h;
            pos += nd->h;
        } else {
            nd->x = rect.x; nd->y = pos; nd->w = frac * rect.w; nd->h = strip;
            pos += nd->w;
        }
        nd->pid         = pids[i];
        nd->cpu_percent = 0;
        nd->mem_bytes   = vals[i];
    }
    (void)strip;
}

static void squarify(float *vals, int *pids, int n,
                     Rect rect, int *out_idx) {
    if (n == 0) return;
    float total = 0;
    for (int i = 0; i < n; i++) total += vals[i];
    if (total == 0) return;

    float strip_w = fminf(rect.w, rect.h);
    int   row_end = 0;
    float row_vals[MAX_PROCESSES];

    for (int i = 0; i < n; i++) {
        row_vals[row_end] = vals[i];
        float curr_worst = worst_ratio(row_vals, row_end + 1, strip_w);
        float prev_worst = (row_end > 0)
            ? worst_ratio(row_vals, row_end, strip_w) : 1e9f;

        if (row_end > 0 && curr_worst > prev_worst) {
            float row_sum = 0;
            for (int j = 0; j < row_end; j++) row_sum += row_vals[j];
            float strip = (row_sum / total) * strip_w;

            layout_row(row_vals, pids, row_end, rect, total, out_idx);

            Rect next = rect;
            if (rect.w >= rect.h) { next.y += strip; next.h -= strip; }
            else                  { next.x += strip; next.w -= strip; }

            squarify(vals + i, pids + i, n - i, next, out_idx);
            return;
        }
        row_end++;
    }
    layout_row(row_vals, pids, row_end, rect, total, out_idx);
}

static int cmp_mem_desc(const void *a, const void *b) {
    const ProcessInfo *pa = (const ProcessInfo *)a;
    const ProcessInfo *pb = (const ProcessInfo *)b;
    return (pb->mem_rss > pa->mem_rss) - (pb->mem_rss < pa->mem_rss);
}

static ImU32 cpu_color_cpu(float pct) {
    float t = pct / 100.0f;
    return ImGui::ColorConvertFloat4ToU32(ImVec4(t, 0.2f, 1.0f - t, 1.0f));
}

void ui_treemap(MetalContext *metal, int *selected_pid) {
    const Snapshot *snap = poll_read();
    if (!snap || snap->count == 0) { ImGui::Text("No data."); return; }

    static ProcessInfo sorted[MAX_PROCESSES];
    memcpy(sorted, snap->procs, sizeof(ProcessInfo) * snap->count);
    int count = snap->count;
    qsort(sorted, count, sizeof(ProcessInfo), cmp_mem_desc);

    static float vals[MAX_PROCESSES];
    static int   pids[MAX_PROCESSES];
    for (int i = 0; i < count; i++) {
        vals[i] = (float)sorted[i].mem_rss;
        pids[i] = sorted[i].pid;
    }

    node_count = 0;
    Rect root  = {0, 0, 1, 1};
    squarify(vals, pids, count, root, &node_count);

    for (int i = 0; i < node_count; i++)
        for (int j = 0; j < snap->count; j++)
            if (snap->procs[j].pid == nodes[i].pid) {
                nodes[i].cpu_percent = snap->procs[j].cpu_percent;
                break;
            }

    bool has_metal = metal && metal_compute_colors(metal, nodes, node_count, colors);

    ImVec2 canvas_pos  = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    if (canvas_size.x < 1 || canvas_size.y < 1) return;

    ImDrawList *dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < node_count; i++) {
        TreemapNode *nd = &nodes[i];
        float px = canvas_pos.x + nd->x * canvas_size.x;
        float py = canvas_pos.y + nd->y * canvas_size.y;
        float pw = nd->w * canvas_size.x;
        float ph = nd->h * canvas_size.y;
        if (pw < 2 || ph < 2) continue;

        ImU32 fill;
        if (has_metal) {
            int ci = i * 4;
            fill = ImGui::ColorConvertFloat4ToU32(
                ImVec4(colors[ci], colors[ci+1], colors[ci+2], colors[ci+3]));
        } else {
            fill = cpu_color_cpu(nd->cpu_percent);
        }

        if (*selected_pid == nd->pid)
            fill = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 0, 1));

        dl->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + ph), fill, 0, 0);
        dl->AddRect(ImVec2(px, py), ImVec2(px + pw, py + ph),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.4f)), 0, 0, 1.0f);

        if (pw > 40 && ph > 16) {
            char label[64];
            snprintf(label, sizeof(label), "%s", sorted[i].name);
            dl->AddText(ImVec2(px + 3, py + 3), 0xFFFFFFFF, label, NULL);
        }
    }

    ImGui::SetCursorScreenPos(canvas_pos);
    ImGui::InvisibleButton("##treemap_click", canvas_size, 0);
    if (ImGui::IsItemClicked(0)) {
        ImVec2 mouse = ImGui::GetMousePos();
        float mx = (mouse.x - canvas_pos.x) / canvas_size.x;
        float my = (mouse.y - canvas_pos.y) / canvas_size.y;
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