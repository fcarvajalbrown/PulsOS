#include "treemap.h"
#include "../core/poll.h"
#include "../core/process.h"

#include "imgui.h"
#include <stdio.h>

static TreemapNode nodes[MAX_PROCESSES];
static float       colors[MAX_PROCESSES * 4];

// HSV rank color fallback when Metal is unavailable
static ImU32 rank_color(int rank, int total, float cpu_pct) {
    float hue = (float)rank / (float)(total > 1 ? total : 1);
    float sat = 0.45f + 0.4f * (cpu_pct / 100.0f);
    float val = 0.80f;
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(hue, sat, val, r, g, b);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1.0f));
}

void ui_treemap(MetalContext *metal, int *selected_pid) {
    const Snapshot *snap = poll_read();
    if (!snap || snap->count == 0) { ImGui::Text("No data."); return; }

    int node_count = treemap_build(snap, nodes, MAX_PROCESSES);

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
        if (pw < 1 || ph < 1) continue;

        ImU32 fill;
        if (has_metal) {
            int ci = i * 4;
            fill = ImGui::ColorConvertFloat4ToU32(
                ImVec4(colors[ci], colors[ci+1], colors[ci+2], colors[ci+3]));
        } else {
            fill = rank_color(i, node_count, nd->cpu_percent);
        }

        if (*selected_pid == nd->pid)
            fill = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 0, 1));

        dl->AddRectFilled(ImVec2(px, py), ImVec2(px+pw, py+ph), fill, 0, 0);
        if (pw > 2 && ph > 2)
            dl->AddRect(ImVec2(px, py), ImVec2(px+pw, py+ph),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.5f)), 0, 0, 1.0f);

        if (pw > 30 && ph > 14) {
            // find name for this pid
            for (int j = 0; j < snap->count; j++) {
                if (snap->procs[j].pid == nd->pid) {
                    dl->AddText(ImVec2(px+3, py+3), 0xFFFFFFFF, snap->procs[j].name, NULL);
                    break;
                }
            }
        }
    }

    ImGui::SetCursorScreenPos(canvas_pos);
    ImGui::InvisibleButton("##treemap_click", canvas_size, 0);
    if (ImGui::IsItemClicked(0)) {
        ImVec2 mouse = ImGui::GetMousePos();
        float nx = (mouse.x - canvas_pos.x) / canvas_size.x;
        float ny = (mouse.y - canvas_pos.y) / canvas_size.y;
        *selected_pid = treemap_hit(nodes, node_count, nx, ny);
    }
}