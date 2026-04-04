#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef enum {
    STATE_LOADING,   // waiting for first snapshot
    STATE_RUNNING,   // normal operation
    STATE_SELECTED,  // user selected a pid, detail panel open
    STATE_ERROR      // snapshot failed 3x
} AppState;

typedef enum {
    EVT_SNAPSHOT_OK,    // poll got a clean snapshot
    EVT_SNAPSHOT_FAIL,  // poll failed — 3 consecutive fires → ERROR
    EVT_PID_SELECTED,   // user clicked a process
    EVT_PID_DESELECTED, // user clicked same process again to deselect
    EVT_PID_DIED,       // selected pid vanished from snapshot
    EVT_RETRY,          // user pressed retry in error screen
} FsmEvent;

void      fsm_init(void);
void      fsm_transition(FsmEvent event);
AppState  fsm_state(void);

#ifdef __cplusplus
}
#endif