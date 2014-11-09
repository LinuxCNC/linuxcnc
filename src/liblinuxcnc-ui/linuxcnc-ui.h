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

// This enum describes the modes of motion(9), the motion controller.
// These modes can not be directly controlled by the UI, the are handled by
// Task.  The UI can control the Task mode (Manual/MDI/Auto), which
// indirectly controls the Motion mode.  This enum is here so that the UI
// can make sense of the "traj mode" reported by lui_get_traj_mode().
// This must match EMC_TRAJ_MODE_ENUM.
typedef enum {
    lui_traj_mode_free = 1,     // joint-mode jogging
    lui_traj_mode_coord = 2,    // mdi & auto (running c-code programs)
    lui_traj_mode_teleop = 3    // world-mode jogging
} lui_traj_mode_t;

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

typedef enum {
    lui_command_wait_mode_received = 0,
    lui_command_wait_mode_done,
} lui_command_wait_mode_t;

#ifdef __cplusplus
extern "C" {
#endif

lui_t *lui_new(void);
void lui_free(lui_t *lui);
void lui_set_command_wait_mode(lui_t *lui, lui_command_wait_mode_t wait_mode);
void lui_set_receive_timeout(lui_t *lui, struct timeval timeout);
void lui_set_done_timeout(lui_t *lui, struct timeval timeout);

int lui_connect(lui_t *lui);
int lui_status_nml_update(lui_t *lui);

int lui_estop(lui_t *lui);
int lui_estop_reset(lui_t *lui);
int lui_machine_on(lui_t *lui);
int lui_machine_off(lui_t *lui);

int lui_mode_manual(lui_t *lui);
int lui_mode_auto(lui_t *lui);
int lui_mode_mdi(lui_t *lui);

int lui_coolant_mist_on(lui_t *lui);
int lui_coolant_mist_off(lui_t *lui);
int lui_coolant_flood_on(lui_t *lui);
int lui_coolant_flood_off(lui_t *lui);

int lui_lube_on(lui_t *lui);
int lui_lube_off(lui_t *lui);

// control the type of jogging in manual mode
int lui_jog_mode_teleop(lui_t *lui);
int lui_jog_mode_joint(lui_t *lui);

int lui_program_open(lui_t *lui, const char *file);
int lui_program_run(lui_t *lui, int line);
int lui_program_step(lui_t *lui);
int lui_program_pause(lui_t *lui);
int lui_program_resume(lui_t *lui);

int lui_send_mdi_command(lui_t *lui, const char *mdi);

struct EMC_STAT *lui_get_status_nml(lui_t *lui);
struct RCS_CMD_CHANNEL *lui_get_command_channel_nml(lui_t *lui);

#include "liblinuxcnc-ui-getters.h"

#ifdef __cplusplus
}
#endif

#endif  // LIBLINUXCNC_UI
