#include "app_ui.h"
#include "process_list.h"
#include "detail_panel.h"
#include "treemap.h"
#include "../core/poll.h"
#include "../core/fsm.h"
#include "../core/process.h"

#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <implot.h>

#define DETAIL_H 220
#define LIST_W   340

static MetalContext *s_metal = NULL;

int app_ui_init(void *sdl_window, void *gl_ctx) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGuiStyle &style    = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding  = 3.0f;
    style.ItemSpacing.y  = 4.0f;
    style.ScrollbarRounding = 3.0f;

    if (!ImGui_ImplSDL2_InitForOpenGL((SDL_Window *)sdl_window, gl_ctx)) return -1;
    if (!ImGui_ImplOpenGL3_Init("#version 330 core"))                     return -1;

    int lw, lh, fw, fh;
    SDL_GetWindowSize((SDL_Window *)sdl_window, &lw, &lh);
    SDL_GL_GetDrawableSize((SDL_Window *)sdl_window, &fw, &fh);
    float dpi_scale = (float)fw / (float)lw;

    ImFontConfig cfg;
    cfg.SizePixels = 14.0f * dpi_scale;
    io.Fonts->AddFontDefault();
    io.Fonts->AddFontDefault(&cfg);
    io.Fonts->Build();
    io.FontDefault         = io.Fonts->Fonts[1];
    io.FontGlobalScale     = 1.0f / dpi_scale;
    io.DisplayFramebufferScale = ImVec2(dpi_scale, dpi_scale);

    return 0;
}

void app_ui_shutdown(void) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void app_ui_set_metal(MetalContext *metal) {
    s_metal = metal;
}

void app_ui_process_event(const void *sdl_event) {
    ImGui_ImplSDL2_ProcessEvent((const SDL_Event *)sdl_event);
}

void app_ui_new_frame(void) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void app_ui_render(int w, int h, int *selected_pid) {
    AppState state = fsm_state();

    ImGuiWindowFlags bg_flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoScrollbar         |
        ImGuiWindowFlags_NoSavedSettings     |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h), ImGuiCond_Always);
    ImGui::Begin("##root", NULL, bg_flags);

    switch (state) {
        case STATE_LOADING:
            ImGui::SetCursorPos(ImVec2((float)w * 0.5f - 60, (float)h * 0.5f));
            ImGui::Text("Loading processes...");
            break;

        case STATE_ERROR:
            ImGui::SetCursorPos(ImVec2((float)w * 0.5f - 80, (float)h * 0.5f));
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Error reading process list.");
            ImGui::SameLine(0, 10);
            if (ImGui::Button("Retry", ImVec2(60, 0))) {
                fsm_transition(EVT_RETRY);
                poll_init();
            }
            break;

        case STATE_RUNNING:
        case STATE_SELECTED: {
            bool has_detail = (state == STATE_SELECTED && *selected_pid != -1);
            float list_h    = has_detail ? (float)h - DETAIL_H : (float)h;
            float map_h     = list_h;

            ImGui::BeginChild("##list", ImVec2(LIST_W, list_h), false);
            ui_process_list(selected_pid);
            ImGui::EndChild();

            ImGui::SameLine(0, 0);

            ImGui::BeginChild("##treemap", ImVec2(0, map_h), false);
            ui_treemap(s_metal, selected_pid);
            ImGui::EndChild();

            if (has_detail) {
                ImGui::SetCursorPos(ImVec2(0, (float)h - DETAIL_H));
                ImGui::BeginChild("##detail", ImVec2((float)w, DETAIL_H), true);
                ui_detail_panel(*selected_pid);
                ImGui::EndChild();
            }

            // deselect if process no longer exists
            if (*selected_pid != -1) {
                const Snapshot *snap = poll_read();
                bool found = false;
                for (int i = 0; i < snap->count; i++)
                    if (snap->procs[i].pid == *selected_pid) { found = true; break; }
                if (!found) {
                    *selected_pid = -1;
                    fsm_transition(EVT_PID_DIED);
                }
            }
            break;
        }
    }

    ImGui::End();
}

void app_ui_present(const void *sdl_window) {
    ImGui::Render();
    int fw, fh;
    SDL_GL_GetDrawableSize((SDL_Window *)sdl_window, &fw, &fh);
    glViewport(0, 0, fw, fh);
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow((SDL_Window *)sdl_window);
}