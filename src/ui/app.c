#include "app.h"
#include "app_ui.h"
#include "../core/poll.h"
#include "../gpu/metal_context.h"
#include "../core/fsm.h"

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>

#define WIN_W      1280
#define WIN_H      800
#define WIN_TITLE  "PulsOS"
#define TARGET_FPS 60
#define FRAME_MS   (1000 / TARGET_FPS)

static SDL_Window   *window = NULL;
static SDL_GLContext gl_ctx = NULL;
static MetalContext *metal  = NULL;
static int selected_pid     = -1;

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

int app_run(void) {
    if (init_sdl()              != 0) return -1;
    if (app_ui_init(window, gl_ctx) != 0) return -1;

    metal = metal_init();
    if (!metal)
        fprintf(stderr, "pulsos: Metal init failed — treemap CPU fallback active\n");

    app_ui_set_metal(metal);
    fsm_init();
    poll_init();

    bool running = true;
    while (running) {
        Uint32 frame_start = SDL_GetTicks();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            app_ui_process_event(&ev);
            if (ev.type == SDL_QUIT)                              running = false;
            if (ev.type == SDL_KEYDOWN &&
                ev.key.keysym.sym == SDLK_ESCAPE)                running = false;
        }

        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        app_ui_new_frame();
        app_ui_render(w, h, &selected_pid);
        app_ui_present(window);

        Uint32 elapsed = SDL_GetTicks() - frame_start;
        if (elapsed < FRAME_MS) SDL_Delay(FRAME_MS - elapsed);
    }

    poll_shutdown();
    metal_shutdown(metal);
    app_ui_shutdown();
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}