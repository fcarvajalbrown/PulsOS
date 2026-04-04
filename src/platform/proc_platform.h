#pragma once

#include "../core/process.h"

// implemented per OS — CMake picks the right .c file
int proc_get_snapshot(Snapshot *out);