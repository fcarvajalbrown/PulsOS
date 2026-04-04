#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include "../gpu/metal_context.h"

void ui_treemap(MetalContext *metal, int *selected_pid);

#ifdef __cplusplus
}
#endif