#include "app.h"
#include "process_list.h"
#include "detail_panel.h"
#include "treemap.h"
#include "../core/poll.h"
#include "../core/process.h"
#include "../gpu/metal_context.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <cimgui.h>
#include <cimgui_impl.h>
#include <stdio.h>
#include <stdbool.h>

// window config
#define WIN_W        1280
#define WIN_H        800
#define WIN_TITLE    "PulsOS"
#define TARGET_FPS   60
#define FRAME_MS     (1000 / TARGET_FPS)

// layout — left panel width, bottom panel height
#define LIST_W       340
#define DETAIL_H     220

static SDL_Window   *window   = NULL;
static SDL_GLContext gl_ctx   = NULL;
static MetalContext *metal    = NULL;
static bool          running  = true;
static int           selected_pid = -1;

// --- init ---

static int init_sdl(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "pulsos: SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    // OpenGL 3.3 core — ImGui requires it
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    window = SDL_CreateWindow(
        WIN_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
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
    SDL_GL_SetSwapInterval(1); // vsync — no need to spin faster than display rate
    return 0;
}

static int init_imgui(void) {
    igCreateContext(NULL);

    ImGuiIO *io = igGetIO_Nil();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // dark theme — easier on the eyes for a monitor tool
    igStyleColorsDark(NULL);

    // tighten default padding slightly for a denser process list
    ImGuiStyle *style = igGetStyle();
    style->WindowRounding   = 4.0f;
    style->FrameRounding    = 3.0f;
    style->ItemSpacing.y    = 4.0f;
    style->ScrollbarRounding = 3.0f;

    if (!ImGui_ImplSDL2_InitForOpenGL(window, gl_ctx)) return -1;
    if (!ImGui_ImplOpenGL3_Init("#version 330 core"))   return -1;
    return 0;
}

// --- FSM transition helpers ---

static void fsm_on_process_selected(int pid) {
    selected_pid = pid;
    // STATE_SELECTED is set in poll layer on next read — handled in render
}

static void fsm_on_process_deselected(void) {
    selected_pid = -1;
}

// --- layout ---

static void render_frame(int w, int h) {
    AppState state = poll_state();

    // full-window background — no title bar, no padding
    ImGuiWindowFlags bg_flags =
        ImGuiWindowFlags_NoTitleBar    |
        ImGuiWindowFlags_NoResize      |
        ImGuiWindowFlags_NoMove        |
        ImGuiWindowFlags_NoScrollbar   |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    igSetNextWindowPos((ImVec2){0, 0}, ImGuiCond_Always, (ImVec2){0, 0});
    igSetNextWindowSize((ImVec2){(float)w, (float)h}, ImGuiCond_Always);
    igBegin("##root", NULL, bg_flags);

    switch (state) {
        case STATE_LOADING:
            igSetCursorPos((ImVec2){(float)w * 0.5f - 60, (float)h * 0.5f});
            igText("Loading processes...");
            break;

        case STATE_ERROR:
            igSetCursorPos((ImVec2){(float)w * 0.5f - 80, (float)h * 0.5f});
            igTextColored((ImVec4){1,0.3f,0.3f,1}, "Error reading process list.");
            igSameLine(0, 10);
            if (igButton("Retry", (ImVec2){60, 0})) {
                poll_init(); // re-init poll layer
            }
            break;

        case STATE_RUNNING:
        case STATE_SELECTED: {
            // left panel — process list
            float list_h = (state == STATE_SELECTED)
                ? (float)h - DETAIL_H
                : (float)h;

            igBeginChild_Str("##list", (ImVec2){LIST_W, list_h}, false, 0);
            ui_process_list(&selected_pid);
            igEndChild();

            igSameLine(0, 0);

            // right panel — treemap
            float map_w = (float)w - LIST_W;
            float map_h = (state == STATE_SELECTED)
                ? (float)h - DETAIL_H
                : (float)h;

            igBeginChild_Str("##treemap", (ImVec2){map_w, map_h}, false, 0);
            ui_treemap(metal, &selected_pid);
            igEndChild();

            // bottom panel — detail (only when a process is selected)
            if (state == STATE_SELECTED && selected_pid != -1) {
                igSetCursorPos((ImVec2){0, (float)h - DETAIL_H});
                igBeginChild_Str("##detail",
                    (ImVec2){(float)w, DETAIL_H}, true, 0);
                ui_detail_panel(selected_pid);
                igEndChild();
            }

            // deselect if selected process is gone
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

    igEnd();
}

// --- main loop ---

int app_run(void) {
    if (init_sdl()    != 0) return -1;
    if (init_imgui()  != 0) return -1;

    metal = metal_init();
    if (!metal) {
        fprintf(stderr, "pulsos: Metal init failed — treemap colors unavailable\n");
        // non-fatal: treemap.c will fall back to CPU color mapping
    }

    poll_init();

    while (running) {
        Uint32 frame_start = SDL_GetTicks();

        // event pump
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        // get current window size — handles resize
        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        // ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        igNewFrame();

        render_frame(w, h);

        igRender();

        // clear + present
        glViewport(0, 0, w, h);
        glClearColor(0.08f, 0.08f, 0.08f, 1.0f); // near-black background
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
        SDL_GL_SwapWindow(window);

        // frame cap — vsync handles it but this is a safety net
        Uint32 elapsed = SDL_GetTicks() - frame_start;
        if (elapsed < FRAME_MS) SDL_Delay(FRAME_MS - elapsed);
    }

    // cleanup
    poll_shutdown();
    metal_shutdown(metal);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    igDestroyContext(NULL);
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}