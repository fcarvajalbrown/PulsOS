#include "process_list.h"
#include "../core/poll.h"
#include "../core/process.h"

#include "imgui.h"
#include <stdio.h>

void ui_process_list(int *selected_pid) {
    const Snapshot *snap = poll_read();
    if (!snap || snap->count == 0) { ImGui::Text("No data yet."); return; }

    static ProcessInfo local[MAX_PROCESSES];
    static SortState   sort = {1, false};
    int count = process_list_sort(snap, local, MAX_PROCESSES, &sort);

    ImGuiTableFlags flags =
        ImGuiTableFlags_Borders       |
        ImGuiTableFlags_RowBg         |
        ImGuiTableFlags_ScrollY       |
        ImGuiTableFlags_SizingFixedFit;

    if (!ImGui::BeginTable("##procs", 4, flags, ImVec2(0, 0), 0)) return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("PID",  ImGuiTableColumnFlags_WidthFixed,   55, 0);
    ImGui::TableSetupColumn("CPU%", ImGuiTableColumnFlags_WidthFixed,   55, 0);
    ImGui::TableSetupColumn("MEM",  ImGuiTableColumnFlags_WidthFixed,   70, 0);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch,  0, 0);
    ImGui::TableHeadersRow();

    for (int c = 0; c < 4; c++) {
        ImGui::TableSetColumnIndex(c);
        if (ImGui::IsItemClicked(0))
            process_list_set_sort(&sort, c);
    }

    for (int i = 0; i < count; i++) {
        ProcessInfo *p = &local[i];
        ImGui::TableNextRow(0, 0);

        bool is_selected = (*selected_pid == p->pid);
        ImGui::TableSetColumnIndex(0); ImGui::Text("%d", p->pid);
        ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f", p->cpu_percent);
        ImGui::TableSetColumnIndex(2); ImGui::Text("%.0f MB", (float)p->mem_rss / (1024*1024));
        ImGui::TableSetColumnIndex(3);

        char label[270];
        snprintf(label, sizeof(label), "%s##%d", p->name, p->pid);
        if (ImGui::Selectable(label, is_selected,
                ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 0)))
            *selected_pid = is_selected ? -1 : p->pid;
    }

    ImGui::EndTable();
}