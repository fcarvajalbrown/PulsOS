#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include "../core/process.h"

// implemented per OS — CMake picks the right .c file
int proc_get_snapshot(Snapshot *out);

#ifdef __cplusplus
}
#endif