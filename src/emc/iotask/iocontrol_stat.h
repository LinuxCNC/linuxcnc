/********************************************************************
* Description: iocontrol_stat.h
*   Plain C structs for iocontrol internal state tracking.
*   Replaces the NML-derived EMC_IO_STAT class hierarchy.
*
* License: GPL Version 2
********************************************************************/
#ifndef IOCONTROL_STAT_H
#define IOCONTROL_STAT_H

#include <stdint.h>

// IO abort reason codes (shared with milltask)
enum EMC_IO_ABORT_REASON_ENUM {
    EMC_ABORT_TASK_EXEC_ERROR = 1,
    EMC_ABORT_AUX_ESTOP = 2,
    EMC_ABORT_MOTION_OR_IO_RCS_ERROR = 3,
    EMC_ABORT_TASK_STATE_OFF = 4,
    EMC_ABORT_TASK_STATE_ESTOP_RESET = 5,
    EMC_ABORT_TASK_STATE_ESTOP = 6,
    EMC_ABORT_TASK_STATE_NOT_ON = 7,
    EMC_ABORT_TASK_ABORT = 8,
    EMC_ABORT_INTERPRETER_ERROR = 9,
    EMC_ABORT_INTERPRETER_ERROR_MDI = 10,
    EMC_ABORT_USER = 100
};

typedef struct {
    int pocketPrepped;      // tool number prepped, -1 = none, 0 = unload
    int toolInSpindle;      // tool number in spindle, 0 = empty
    int toolFromPocket;     // pocket the current tool came from
} iocontrol_tool_stat_t;

typedef struct {
    int mist;               // 0 off, 1 on
    int flood;              // 0 off, 1 on
} iocontrol_coolant_stat_t;

typedef struct {
    int estop;              // non-zero means estopped
} iocontrol_aux_stat_t;

typedef struct {
    int on;                 // 0 off, 1 on
    int level;              // 0 low, 1 okay
} iocontrol_lube_stat_t;

typedef struct {
    uint32_t heartbeat;
    int reason;
    int fault;
    iocontrol_tool_stat_t tool;
    iocontrol_coolant_stat_t coolant;
    iocontrol_aux_stat_t aux;
    iocontrol_lube_stat_t lube;
} iocontrol_stat_t;

#endif /* IOCONTROL_STAT_H */
