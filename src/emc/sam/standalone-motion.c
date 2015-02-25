
//
// standalone-motion: a test wrapper for the LinuxCNC Motion component
// Copyright (C) 2014 Sebastian Kuzminsky <seb@highlab.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <float.h>
#include <stdlib.h>

#include "rtapi.h"
#include "uspace_common.h"

#include "mot_priv.h"
#include "motion_debug.h"
#include "emcmotcfg.h"


int num_joints = EMCMOT_MAX_JOINTS;

emcmot_joint_t *joints;

struct emcmot_debug_t emcmotDebug_storage;
struct emcmot_debug_t *emcmotDebug = &emcmotDebug_storage;

struct emcmot_command_t emcmotCommand_storage;
struct emcmot_command_t *emcmotCommand = &emcmotCommand_storage;

struct emcmot_status_t emcmotStatus_storage;
struct emcmot_status_t *emcmotStatus = &emcmotStatus_storage;

struct emcmot_config_t emcmotConfig_storage;
struct emcmot_config_t *emcmotConfig = &emcmotConfig_storage;

int kinType = 0;

emcmot_hal_data_t hal_data_storage;
emcmot_hal_data_t *emcmot_hal_data = &hal_data_storage;

int num_dio = 4;
int num_aio = 4;

long traj_period_nsec = 1000 * 1000;


void reportError(const char *fmt, ...) __attribute__((format(printf,1,2)));
void reportError(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}


void emcmot_config_change(void) { }


void init_joints(void) {
    int joint_num;
    emcmot_joint_t *joint;

    /* init per-joint stuff */
    for (joint_num = 0; joint_num < num_joints; joint_num++) {
	/* point to structure for this joint */
	joint = &joints[joint_num];

	/* init the config fields with some "reasonable" defaults" */

	joint->type = 0;
	joint->max_pos_limit = 1.0;
	joint->min_pos_limit = -1.0;
	joint->vel_limit = 1.0;
	joint->acc_limit = 1.0;
	joint->min_ferror = 0.01;
	joint->max_ferror = 1.0;
	joint->home_search_vel = 0.0;
	joint->home_latch_vel = 0.0;
	joint->home_final_vel = -1;
	joint->home_offset = 0.0;
	joint->home = 0.0;
	joint->home_flags = 0;
	joint->home_sequence = -1;
	joint->backlash = 0.0;

	joint->comp.entries = 0;
	joint->comp.entry = &(joint->comp.array[0]);
	/* the compensation code has -DBL_MAX at one end of the table
	   and +DBL_MAX at the other so _all_ commanded positions are
	   guaranteed to be covered by the table */
	joint->comp.array[0].nominal = -DBL_MAX;
	joint->comp.array[0].fwd_trim = 0.0;
	joint->comp.array[0].rev_trim = 0.0;
	joint->comp.array[0].fwd_slope = 0.0;
	joint->comp.array[0].rev_slope = 0.0;
	for (int n = 1 ; n < EMCMOT_COMP_SIZE+2 ; n++ ) {
	    joint->comp.array[n].nominal = DBL_MAX;
	    joint->comp.array[n].fwd_trim = 0.0;
	    joint->comp.array[n].rev_trim = 0.0;
	    joint->comp.array[n].fwd_slope = 0.0;
	    joint->comp.array[n].rev_slope = 0.0;
	}

	/* init status info */
	joint->flag = 0;
	joint->coarse_pos = 0.0;
	joint->pos_cmd = 0.0;
	joint->vel_cmd = 0.0;
	joint->backlash_corr = 0.0;
	joint->backlash_filt = 0.0;
	joint->backlash_vel = 0.0;
	joint->motor_pos_cmd = 0.0;
	joint->motor_pos_fb = 0.0;
	joint->pos_fb = 0.0;
	joint->ferror = 0.0;
	joint->ferror_limit = joint->min_ferror;
	joint->ferror_high_mark = 0.0;

	/* init internal info */
	cubicInit(&(joint->cubic));

	/* init misc other stuff in joint structure */
	joint->big_vel = 10.0 * joint->vel_limit;
	joint->home_state = 0;

	/* init joint flags (reduntant, since flag = 0 */

	SET_JOINT_ENABLE_FLAG(joint, 0);
	SET_JOINT_ACTIVE_FLAG(joint, 0);
	SET_JOINT_NHL_FLAG(joint, 0);
	SET_JOINT_PHL_FLAG(joint, 0);
	SET_JOINT_INPOS_FLAG(joint, 1);
	SET_JOINT_HOMING_FLAG(joint, 0);
	SET_JOINT_HOMED_FLAG(joint, 0);
	SET_JOINT_FERROR_FLAG(joint, 0);
	SET_JOINT_FAULT_FLAG(joint, 0);
	SET_JOINT_ERROR_FLAG(joint, 0);
    }
}


void *xalloc(size_t s) {
    void *p;
    p = calloc(1, s);
    if (p == NULL) {
        fprintf(stderr, "error allocating %lu bytes: %s\n", s, strerror(errno));
        exit(1);
    }
    return p;
}


void init_hal_joint_mock(joint_hal_t *j) {
    j->coarse_pos_cmd = xalloc(sizeof(hal_float_t));
    j->joint_vel_cmd = xalloc(sizeof(hal_float_t));
    j->backlash_corr = xalloc(sizeof(hal_float_t));
    j->backlash_filt = xalloc(sizeof(hal_float_t));
    j->backlash_vel = xalloc(sizeof(hal_float_t));
    j->motor_offset = xalloc(sizeof(hal_float_t));
    j->motor_pos_cmd = xalloc(sizeof(hal_float_t));
    j->motor_pos_fb = xalloc(sizeof(hal_float_t));
    j->joint_pos_cmd = xalloc(sizeof(hal_float_t));
    j->joint_pos_fb = xalloc(sizeof(hal_float_t));
    j->f_error = xalloc(sizeof(hal_float_t));
    j->f_error_lim = xalloc(sizeof(hal_float_t));

    j->free_pos_cmd = xalloc(sizeof(hal_float_t));
    j->free_vel_lim = xalloc(sizeof(hal_float_t));

    j->free_tp_enable = xalloc(sizeof(hal_bit_t));
    j->kb_jog_active = xalloc(sizeof(hal_bit_t));
    j->wheel_jog_active = xalloc(sizeof(hal_bit_t));

    j->active = xalloc(sizeof(hal_bit_t));
    j->in_position = xalloc(sizeof(hal_bit_t));
    j->error = xalloc(sizeof(hal_bit_t));
    j->phl = xalloc(sizeof(hal_bit_t));
    j->nhl = xalloc(sizeof(hal_bit_t));
    j->homing = xalloc(sizeof(hal_bit_t));
    j->homed = xalloc(sizeof(hal_bit_t));
    j->f_errored = xalloc(sizeof(hal_bit_t));
    j->faulted = xalloc(sizeof(hal_bit_t));
    j->pos_lim_sw = xalloc(sizeof(hal_bit_t));
    j->neg_lim_sw = xalloc(sizeof(hal_bit_t));
    j->home_sw = xalloc(sizeof(hal_bit_t));
    j->index_enable = xalloc(sizeof(hal_bit_t));
    j->amp_fault = xalloc(sizeof(hal_bit_t));
    j->amp_enable = xalloc(sizeof(hal_bit_t));
    j->home_state = xalloc(sizeof(hal_s32_t));

    j->unlock = xalloc(sizeof(hal_bit_t));
    j->is_unlocked = xalloc(sizeof(hal_bit_t));

    j->jog_counts = xalloc(sizeof(hal_s32_t));
    j->jog_enable = xalloc(sizeof(hal_bit_t));
    j->jog_scale = xalloc(sizeof(hal_float_t));
    j->jog_vel_mode = xalloc(sizeof(hal_bit_t));
}


void init_hal_mock(void) {
    emcmot_hal_data->probe_input = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->enable = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_index_enable = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_is_atspeed = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_revs = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->spindle_inhibit = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->adaptive_feed = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->feed_hold = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->feed_inhibit = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->motion_enabled = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->in_position = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->coord_mode = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->teleop_mode = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->coord_error = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->on_soft_limit = xalloc(sizeof(hal_bit_t));

    emcmot_hal_data->program_line = xalloc(sizeof(hal_s32_t));
    emcmot_hal_data->motion_type = xalloc(sizeof(hal_s32_t));
    emcmot_hal_data->current_vel = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->requested_vel = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->distance_to_go = xalloc(sizeof(hal_float_t));

    for (int i = 0; i < EMCMOT_MAX_DIO; i ++) {
        emcmot_hal_data->synch_do[i] = xalloc(sizeof(hal_bit_t));
        emcmot_hal_data->synch_di[i] = xalloc(sizeof(hal_bit_t));
    }

    for (int i = 0; i < EMCMOT_MAX_AIO; i ++) {
        emcmot_hal_data->analog_input[i] = xalloc(sizeof(hal_float_t));
        emcmot_hal_data->analog_output[i] = xalloc(sizeof(hal_float_t));
    }

    emcmot_hal_data->spindle_on = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_forward = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_reverse = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_incr_speed = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_decr_speed = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_brake = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_speed_out = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->spindle_speed_out_rps = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->spindle_speed_out_abs = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->spindle_speed_out_rps_abs = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->spindle_speed_cmd_rps = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->spindle_speed_in = xalloc(sizeof(hal_float_t));

    emcmot_hal_data->spindle_orient_angle = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->spindle_orient_mode = xalloc(sizeof(hal_s32_t));
    emcmot_hal_data->spindle_orient = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_locked = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_is_oriented = xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_orient_fault = xalloc(sizeof(hal_s32_t));

    emcmot_hal_data->tooloffset_x = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_y = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_z = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_a = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_b = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_c = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_u = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_v = xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_w = xalloc(sizeof(hal_float_t));

    for (int i = 0; i < EMCMOT_MAX_JOINTS; i ++) {
        init_hal_joint_mock(&emcmot_hal_data->joint[i]);
    }
}


void update_joint_pos_fb(void) {
    // following error is computed on the motor cmd & fb
    for (int i = 0; i < EMCMOT_MAX_JOINTS; i ++) {
        *emcmot_hal_data->joint[i].motor_pos_fb = *emcmot_hal_data->joint[i].motor_pos_cmd;
    }
}


void motion_set_num_axes(void) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_NUM_AXES;
    emcmotCommand->axis = 3;
}

void motion_set_vel(void) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_VEL;
    emcmotCommand->vel = 0.987;
}

void motion_set_vel_limit(void) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_VEL_LIMIT;
    emcmotCommand->vel = 10.0;
}

void motion_set_acc(void) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_ACC;
    emcmotCommand->acc = 5.123;
}

void motion_set_backlash(void) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_BACKLASH;
    emcmotCommand->axis = 0;
    emcmotCommand->backlash = 0;
}

void motion_set_position_limits(void) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_POSITION_LIMITS;
    emcmotCommand->axis = 0;
    emcmotCommand->minLimit = -10;
    emcmotCommand->maxLimit = 10;
}

void motion_set_min_ferror(void) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_MIN_FERROR;
    emcmotCommand->axis = 0;
    emcmotCommand->minFerror = 1.0;
}

void motion_set_max_ferror(void) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_MAX_FERROR;
    emcmotCommand->axis = 0;
    emcmotCommand->maxFerror = 1.0;
}

void motion_enable(void) {
    // turn on the enable pin in HAL
    *emcmot_hal_data->enable = 1;

    // send the Enable command
    emcmotCommand->head++;
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_ENABLE;
    emcmotCommand->tail++;
}

void motion_activate_joint(void) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_ACTIVATE_JOINT;
    emcmotCommand->axis = 0;
}

void motion_enable_amplifier(void) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_ENABLE_AMPLIFIER;
    emcmotCommand->axis = 0;
}

void motion_set_joint_vel_limit(void) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_JOINT_VEL_LIMIT;
    emcmotCommand->axis = 0;
    emcmotCommand->vel = 10.0;
}

void motion_set_joint_acc_limit(void) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_JOINT_ACC_LIMIT;
    emcmotCommand->axis = 0;
    emcmotCommand->acc = 10.0;
}

void motion_set_motor_offset(void) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_MOTOR_OFFSET;
    emcmotCommand->axis = 0;
    emcmotCommand->motor_offset = 0.0;
}

void motion_jog_incr(void) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_JOG_INCR;
    emcmotCommand->axis = 0;
    emcmotCommand->vel = 5.0;
    emcmotCommand->offset = 1.234;
}


typedef void (*fptr)(void);

int main(int argc, char *argv[]) {
    int pass = 0;

    int index;
    fptr activities[] = {
        // motion_set_num_axes,
        // motion_set_vel,
        motion_set_vel_limit,
        motion_set_acc,
        // motion_set_backlash,
        // motion_set_position_limits,
        motion_set_min_ferror,
        motion_set_max_ferror,
        motion_set_joint_vel_limit,
        motion_set_joint_acc_limit,
        motion_activate_joint,
        // motion_set_motor_offset,

        motion_enable,
        motion_enable_amplifier,
        motion_jog_incr,
        NULL
    };

    printf("standalone motion\n");

    joints = &emcmotDebug->joints[0];
    init_joints();

    kinType = kinematicsType();

    init_hal_mock();

    index = 0;

    do {
        if (
            (emcmotCommand->commandNum == emcmotStatus->commandNumEcho)
            && (activities[index] != NULL)
        ) {
            // Motion has caught up, do the next thing
            activities[index]();
            index++;
        }

        update_joint_pos_fb();

        emcmotCommandHandler(0, 1000*1000);
        emcmotController(0, 1000*1000);

        printf(
            "% 6d   %.6f   %.6f   %.6f\n",
            pass,
            *emcmot_hal_data->joint[0].joint_pos_cmd,
            *emcmot_hal_data->joint[1].joint_pos_cmd,
            *emcmot_hal_data->joint[2].joint_pos_cmd
        );

        pass ++;
    } while(
        (activities[index] != NULL)
        || (emcmotStatus->current_vel > 0.000001)
    );

    return 0;
}
