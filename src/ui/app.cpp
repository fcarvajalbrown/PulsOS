#include "app.h"
#include "process_list.h"
#include "detail_panel.h"
#include "treemap.h"
#include "../core/poll.h"
#include "../core/process.h"
#include "../gpu/metal_context.h"

#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <stdio.h>

#define WIN_W      1280
#define WIN_H      800
#define WIN_TITLE  "PulsOS"
#define TARGET_FPS 60
#define FRAME_MS   (1000 / TARGET_FPS)
#define LIST_W     340
#define DETAIL_H   220

static SDL_Window   *window       = NULL;
static SDL_GLContext gl_ctx       = NULL;
static MetalContext *metal        = NULL;
static bool          running      = true;
static int           selected_pid = -1;

static int init_sdl(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "pulsos: SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    window = SDL_CreateWindow(WIN_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "pulsos: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return -1;
    }
    gl_ctx = SDL_GL_CreateContext(window);
    if (!gl_ctx) {
        fprintf(stderr, "pulsos: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return -1;
    }
    SDL_GL_MakeCurrent(window, gl_ctx);
    SDL_GL_SetSwapInterval(1);
    return 0;
}

static int init_imgui(void) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGuiStyle &style       = ImGui::GetStyle();
    style.WindowRounding    = 4.0f;
    style.FrameRounding     = 3.0f;
    style.ItemSpacing.y     = 4.0f;
    style.ScrollbarRounding = 3.0f;

    if (!ImGui_ImplSDL2_InitForOpenGL(window, gl_ctx)) return -1;
    if (!ImGui_ImplOpenGL3_Init("#version 330 core"))   return -1;
    return 0;
}

static void fsm_on_process_deselected(void) {
    selected_pid = -1;
}

static void render_frame(int w, int h) {
    AppState state = poll_state();

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
            if (ImGui::Button("Retry", ImVec2(60, 0)))
                poll_init();
            break;

        case STATE_RUNNING:
        case STATE_SELECTED: {
            float list_h = (state == STATE_SELECTED) ? (float)h - DETAIL_H : (float)h;

            ImGui::BeginChild("##list", ImVec2(LIST_W, list_h), false);
            ui_process_list(&selected_pid);
            ImGui::EndChild();

            ImGui::SameLine(0, 0);

            float map_w = (float)w - LIST_W;
            float map_h = (state == STATE_SELECTED) ? (float)h - DETAIL_H : (float)h;

            ImGui::BeginChild("##treemap", ImVec2(map_w, map_h), false);
            ui_treemap(metal, &selected_pid);
            ImGui::EndChild();

            if (state == STATE_SELECTED && selected_pid != -1) {
                ImGui::SetCursorPos(ImVec2(0, (float)h - DETAIL_H));
                ImGui::BeginChild("##detail", ImVec2((float)w, DETAIL_H), true);
                ui_detail_panel(selected_pid);
                ImGui::EndChild();
            }

            if (selected_pid != -1) {
                const Snapshot *snap = poll_read();
                bool found = false;
                for (int i = 0; i < snap->count; i++)
                    if (snap->procs[i].pid == selected_pid) { found = true; break; }
                if (!found) fsm_on_process_deselected();
            }
            break;
        }
    }

    ImGui::End();
}

int app_run(void) {
    if (init_sdl()   != 0) return -1;
    if (init_imgui() != 0) return -1;

    metal = metal_init();
    if (!metal)
        fprintf(stderr, "pulsos: Metal init failed — treemap CPU fallback active\n");

    poll_init();

    while (running) {
        Uint32 frame_start = SDL_GetTicks();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        render_frame(w, h);

        ImGui::Render();

        glViewport(0, 0, w, h);
        glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);

        Uint32 elapsed = SDL_GetTicks() - frame_start;
        if (elapsed < FRAME_MS) SDL_Delay(FRAME_MS - elapsed);
    }

    poll_shutdown();
    metal_shutdown(metal);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}