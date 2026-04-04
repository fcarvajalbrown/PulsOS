#include "fsm.h"
#include <stdatomic.h>
#include <stddef.h>

typedef struct {
    AppState from;
    FsmEvent event;
    AppState to;
} Transition;

static const Transition table[] = {
    { STATE_LOADING,  EVT_SNAPSHOT_OK,    STATE_RUNNING  },
    { STATE_LOADING,  EVT_SNAPSHOT_FAIL,  STATE_ERROR    },
    { STATE_RUNNING,  EVT_PID_SELECTED,   STATE_SELECTED },
    { STATE_RUNNING,  EVT_SNAPSHOT_FAIL,  STATE_ERROR    },
    { STATE_SELECTED, EVT_PID_DESELECTED, STATE_RUNNING  },
    { STATE_SELECTED, EVT_PID_DIED,       STATE_RUNNING  },
    { STATE_SELECTED, EVT_SNAPSHOT_FAIL,  STATE_ERROR    },
    { STATE_ERROR,    EVT_RETRY,          STATE_LOADING  },
};

#define TABLE_LEN (int)(sizeof(table) / sizeof(table[0]))

static _Atomic AppState current = STATE_LOADING;

void fsm_init(void) {
    atomic_store(&current, STATE_LOADING);
}

void fsm_transition(FsmEvent event) {
    AppState from = atomic_load(&current);
    for (int i = 0; i < TABLE_LEN; i++) {
        if (table[i].from == from && table[i].event == event) {
            atomic_store(&current, table[i].to);
            return;
        }
    }
    // illegal transition — ignore silently
}

AppState fsm_state(void) {
    return atomic_load(&current);
}