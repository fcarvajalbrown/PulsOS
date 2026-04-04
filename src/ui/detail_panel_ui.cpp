#include "detail_panel.h"
#include "implot_wrapper.h"
#include "../core/poll.h"
#include "../core/process.h"

#include "imgui.h"

void ui_detail_panel(int pid) {
    const Snapshot       *snap = poll_read();
    const ProcessInfo    *info = detail_find(snap, pid);
    const ProcessHistory *hist = poll_history(pid);

    if (!info) { ImGui::Text("Process gone."); return; }

    ImGui::Text("PID: %d",     info->pid);   ImGui::SameLine(0, 20);
    ImGui::Text("PPID: %d",    info->ppid);  ImGui::SameLine(0, 20);
    ImGui::Text("CPU: %.1f%%", info->cpu_percent); ImGui::SameLine(0, 20);
    ImGui::Text("RSS: %.1f MB", (float)info->mem_rss / (1024*1024)); ImGui::SameLine(0, 20);
    ImGui::Text("VMS: %.1f MB", (float)info->mem_vms / (1024*1024));

    ImGui::Text("%s", info->path[0] ? info->path : "(path unavailable)");
    ImGui::Separator();

    if (hist)
        implot_sparkline("##cpu_hist", hist->cpu_history,
                         HISTORY_LEN, hist->history_head, -1, -1);
}