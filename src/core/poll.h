#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include "process.h"
#include "fsm.h"

void              poll_init(void);
void              poll_shutdown(void);
const Snapshot   *poll_read(void);
const ProcessHistory *poll_history(int pid);
AppState          poll_state(void);

#ifdef __cplusplus
}
#endif