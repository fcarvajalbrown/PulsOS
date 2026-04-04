#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../gpu/metal_context.h"

int  app_ui_init(void *sdl_window, void *gl_ctx);
void app_ui_shutdown(void);
void app_ui_set_metal(MetalContext *metal);
void app_ui_process_event(const void *sdl_event);
void app_ui_new_frame(void);
void app_ui_render(int w, int h, int *selected_pid);
void app_ui_present(const void *sdl_window);

#ifdef __cplusplus
}
#endif