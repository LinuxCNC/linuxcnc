//
// liblinuxcnc-ui: a library to control linuxcnc
//
// Copyright (C) 2014 Sebastian Kuzminsky
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the
// Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
// Boston, MA  02110-1301, USA.
//

#ifndef LIBLINUXCNC_UI
#define LIBLINUXCNC_UI

typedef struct lui lui_t;


// types for EMC_TASK_SET_STATE
// these must match EMC_TASK_STATE_ENUM from emc/nml_intf/emc.hh
typedef enum {
    lui_task_state_estop = 1,
    lui_task_state_estop_reset = 2,
    lui_task_state_off = 3,
    lui_task_state_on = 4
} lui_task_state_t;

// these must match EMC_TASK_MODE_ENUM from emc/nml_intf/emc.hh
typedef enum {
    lui_task_mode_manual = 1,
    lui_task_mode_auto = 2,
    lui_task_mode_mdi = 3,
} lui_task_mode_t;

// these must match EMC_TASK_EXEC_ENUM
typedef enum {
    lui_task_exec_error = 1,
    lui_task_exec_done = 2,
    lui_task_exec_waiting_for_motion = 3,
    lui_task_exec_waiting_for_motion_queue = 4,
    lui_task_exec_waiting_for_io = 5,
    lui_task_exec_waiting_for_motion_and_io = 7,
    lui_task_exec_waiting_for_delay = 8,
    lui_task_exec_waiting_for_system_cmd = 9,
    lui_task_exec_waiting_for_spindle_oriented = 10
} lui_exec_state_t;

// these must match EMC_TASK_INTERP_ENUM
typedef enum {
    lui_task_interp_idle = 1,
    lui_task_interp_reading = 2,
    lui_task_interp_paused = 3,
    lui_task_interp_waiting = 4
} lui_interp_state_t;

// these must match CANON_LINEAR_UNITS
typedef enum {
    lui_linear_units_inch = 1,
    lui_linear_units_mm = 2,
    // cm = 3,
} lui_linear_units_t;

// must match KINEMATICS_TYPE
typedef enum {
    lui_kinematics_identity = 1,
    lui_kinematics_forward_only,
    lui_kinematics_inverse_only,
    lui_kinematics_both,
} lui_kinematics_type_t;

// must match CANON_MOTION_MODE
typedef enum {
    lui_motion_mode_exact_stop = 1,
    lui_motion_mode_exact_path = 2,
    lui_motion_mode_continuous = 3,
} lui_motion_mode_t;

// must match (in memory layout) EmcPose
typedef struct {
    double x, y, z, a, b, c, u, v, w;
} lui_position_t;

typedef struct {
    int toolno;
    lui_position_t offset;
    double diameter, frontangle, backangle;
    int orientation;
} lui_tool_info_t;

#ifdef __cplusplus
extern "C" {
#endif

lui_t *lui_new(void);
void lui_free(lui_t *lui);

int lui_connect(lui_t *lui);
int lui_status_nml_update(lui_t *lui);
int lui_command_nml_wait_done(lui_t *lui);

int lui_estop(lui_t *lui);
int lui_estop_reset(lui_t *lui);
int lui_machine_on(lui_t *lui);
int lui_machine_off(lui_t *lui);

int lui_mode_manual(lui_t *lui);
int lui_mode_auto(lui_t *lui);
int lui_mode_mdi(lui_t *lui);

struct EMC_STAT *lui_get_status_nml(lui_t *lui);
struct RCS_CMD_CHANNEL *lui_get_command_channel_nml(lui_t *lui);

#include "liblinuxcnc-ui-getters.h"

#ifdef __cplusplus
}
#endif

#endif  // LIBLINUXCNC_UI
