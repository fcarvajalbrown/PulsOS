#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// plain C interface — implot internals stay in the .cpp file
void implot_sparkline(const char *label, const float *values, int count,
                      int offset, float width, float height);

#ifdef __cplusplus
}
#endif