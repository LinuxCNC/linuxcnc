
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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

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
	joint->home_state = HOME_IDLE;

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
    j->coarse_pos_cmd = (hal_float_t*)xalloc(sizeof(hal_float_t));
    j->joint_vel_cmd = (hal_float_t*)xalloc(sizeof(hal_float_t));
    j->backlash_corr = (hal_float_t*)xalloc(sizeof(hal_float_t));
    j->backlash_filt = (hal_float_t*)xalloc(sizeof(hal_float_t));
    j->backlash_vel = (hal_float_t*)xalloc(sizeof(hal_float_t));
    j->motor_offset = (hal_float_t*)xalloc(sizeof(hal_float_t));
    j->motor_pos_cmd = (hal_float_t*)xalloc(sizeof(hal_float_t));
    j->motor_pos_fb = (hal_float_t*)xalloc(sizeof(hal_float_t));
    j->joint_pos_cmd = (hal_float_t*)xalloc(sizeof(hal_float_t));
    j->joint_pos_fb = (hal_float_t*)xalloc(sizeof(hal_float_t));
    j->f_error = (hal_float_t*)xalloc(sizeof(hal_float_t));
    j->f_error_lim = (hal_float_t*)xalloc(sizeof(hal_float_t));

    j->free_pos_cmd = (hal_float_t*)xalloc(sizeof(hal_float_t));
    j->free_vel_lim = (hal_float_t*)xalloc(sizeof(hal_float_t));

    j->free_tp_enable = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->kb_jog_active = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->wheel_jog_active = (hal_bit_t*)xalloc(sizeof(hal_bit_t));

    j->active = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->in_position = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->error = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->phl = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->nhl = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->homing = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->homed = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->f_errored = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->faulted = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->pos_lim_sw = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->neg_lim_sw = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->home_sw = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->index_enable = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->amp_fault = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->amp_enable = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->home_state = (hal_s32_t*)xalloc(sizeof(hal_s32_t));

    j->unlock = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->is_unlocked = (hal_bit_t*)xalloc(sizeof(hal_bit_t));

    j->jog_counts = (hal_s32_t*)xalloc(sizeof(hal_s32_t));
    j->jog_enable = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    j->jog_scale = (hal_float_t*)xalloc(sizeof(hal_float_t));
    j->jog_vel_mode = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
}


void init_hal_mock(void) {
    emcmot_hal_data->probe_input = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->enable = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_index_enable = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_is_atspeed = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_revs = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->spindle_inhibit = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->adaptive_feed = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->feed_hold = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->feed_inhibit = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->motion_enabled = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->in_position = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->coord_mode = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->teleop_mode = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->coord_error = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->on_soft_limit = (hal_bit_t*)xalloc(sizeof(hal_bit_t));

    emcmot_hal_data->program_line = (hal_s32_t*)xalloc(sizeof(hal_s32_t));
    emcmot_hal_data->motion_type = (hal_s32_t*)xalloc(sizeof(hal_s32_t));
    emcmot_hal_data->current_vel = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->requested_vel = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->distance_to_go = (hal_float_t*)xalloc(sizeof(hal_float_t));

    for (int i = 0; i < EMCMOT_MAX_DIO; i ++) {
        emcmot_hal_data->synch_do[i] = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
        emcmot_hal_data->synch_di[i] = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    }

    for (int i = 0; i < EMCMOT_MAX_AIO; i ++) {
        emcmot_hal_data->analog_input[i] = (hal_float_t*)xalloc(sizeof(hal_float_t));
        emcmot_hal_data->analog_output[i] = (hal_float_t*)xalloc(sizeof(hal_float_t));
    }

    emcmot_hal_data->spindle_on = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_forward = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_reverse = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_incr_speed = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_decr_speed = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_brake = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_speed_out = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->spindle_speed_out_rps = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->spindle_speed_out_abs = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->spindle_speed_out_rps_abs = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->spindle_speed_cmd_rps = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->spindle_speed_in = (hal_float_t*)xalloc(sizeof(hal_float_t));

    emcmot_hal_data->spindle_orient_angle = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->spindle_orient_mode = (hal_s32_t*)xalloc(sizeof(hal_s32_t));
    emcmot_hal_data->spindle_orient = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_locked = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_is_oriented = (hal_bit_t*)xalloc(sizeof(hal_bit_t));
    emcmot_hal_data->spindle_orient_fault = (hal_s32_t*)xalloc(sizeof(hal_s32_t));

    emcmot_hal_data->tooloffset_x = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_y = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_z = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_a = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_b = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_c = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_u = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_v = (hal_float_t*)xalloc(sizeof(hal_float_t));
    emcmot_hal_data->tooloffset_w = (hal_float_t*)xalloc(sizeof(hal_float_t));

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

void motion_set_vel_limit(double vel) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_VEL_LIMIT;
    emcmotCommand->vel = vel;
}

void motion_set_acc(double acc) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_ACC;
    emcmotCommand->acc = acc;
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

void motion_set_min_ferror(int joint, double ferror) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_MIN_FERROR;
    emcmotCommand->axis = joint;
    emcmotCommand->minFerror = ferror;
}

void motion_set_max_ferror(int joint, double ferror) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_MAX_FERROR;
    emcmotCommand->axis = joint;
    emcmotCommand->maxFerror = ferror;
}

// FIXME
void motion_enable(void) {
    // turn on the enable pin in HAL
    *emcmot_hal_data->enable = 1;

    // send the Enable command
    emcmotCommand->head++;
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_ENABLE;
    emcmotCommand->tail++;
}

void motion_activate_joint(int joint) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_ACTIVATE_JOINT;
    emcmotCommand->axis = joint;
}

void motion_enable_amplifier(int joint) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_ENABLE_AMPLIFIER;
    emcmotCommand->axis = joint;
}

void motion_set_joint_vel_limit(int joint, double vel_limit) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_JOINT_VEL_LIMIT;
    emcmotCommand->axis = joint;
    emcmotCommand->vel = vel_limit;
}

void motion_set_joint_acc_limit(int joint, double acc_limit) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_JOINT_ACC_LIMIT;
    emcmotCommand->axis = joint;
    emcmotCommand->acc = acc_limit;
}

void motion_set_motor_offset(void) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_SET_MOTOR_OFFSET;
    emcmotCommand->axis = 0;
    emcmotCommand->motor_offset = 0.0;
}

void motion_jog_incr(int joint, double vel, double offset) {
    emcmotCommand->commandNum++;
    emcmotCommand->command = EMCMOT_JOG_INCR;
    emcmotCommand->axis = joint;
    emcmotCommand->vel = vel;
    emcmotCommand->offset = offset;
}


typedef void (*fptr)(void);


//
// Reads from the input file, performs the action indicated.
//
// Returns 0 on success.
//
// Returns -1 on failure, with errno set to indicate what went wrong:
//     ENODATA: EOF on input
//     EINVAL: invalid input
//

int handle_input(bool interactive, FILE *in) {
    bool compound = false;
    static int pass_count = 0;
    static int pass_index = 0;

    if (pass_count > 0) {
        pass_index ++;
        fprintf(stderr, "pass %d/%d\n", pass_index, pass_count);
        if (pass_index == pass_count) {
            pass_count = 0;
        }
        return 0;
    }

    if (interactive) {
        printf("sam> ");
    }

    do {
        char buffer[LINE_MAX];
        char *line;

        double d0, d1;
        int i0;

        line = fgets(buffer, sizeof(buffer), in);
        if (line == NULL) {
            if (interactive) {
                printf("\n");
            }
            errno = ENODATA;
            return -1;
        }
        if (line[strlen(line) - 1] != '\n') {
            fprintf(stderr, "input line too long\n");
            errno = EINVAL;
            return -1;
        }
        line[strlen(line)-1] = '\0';

        // trim leading white space
        while (isspace(*line)) {
            line ++;
        }
        if (*line == '\0') {
            continue;
        }

        // ignore comments
        if (*line == '#') {
            continue;
        }

        // parse the line
        if (sscanf(line, "SET_VEL_LIMIT %lf", &d0) == 1) {
            fprintf(stderr, "set vel limit %f\n", d0);
            motion_set_vel_limit(d0);

        } else if (sscanf(line, "SET_ACC %lf", &d0) == 1) {
            fprintf(stderr, "set acc %f\n", d0);
            motion_set_acc(d0);

        } else if (sscanf(line, "SET_MIN_FERROR joint=%d ferror=%lf", &i0, &d0) == 2) {
            fprintf(stderr, "set min ferror joint=%d ferror=%lf\n", i0, d0);
            motion_set_min_ferror(i0, d0);

        } else if (sscanf(line, "SET_MAX_FERROR joint=%d ferror=%lf", &i0, &d0) == 2) {
            fprintf(stderr, "set max ferror joint=%d ferror=%lf\n", i0, d0);
            motion_set_max_ferror(i0, d0);

        } else if (sscanf(line, "SET_JOINT_VEL_LIMIT joint=%d vel_limit=%lf", &i0, &d0) == 2) {
            fprintf(stderr, "set joint vel limit joint=%d vel_limit=%lf\n", i0, d0);
            motion_set_joint_vel_limit(i0, d0);

        } else if (sscanf(line, "SET_JOINT_ACC_LIMIT joint=%d acc_limit=%lf", &i0, &d0) == 2) {
            fprintf(stderr, "set joint acc limit joint=%d acc_limit=%lf\n", i0, d0);
            motion_set_joint_acc_limit(i0, d0);

        } else if (sscanf(line, "ACTIVATE_JOINT %d", &i0) == 1) {
            fprintf(stderr, "activate joint %d\n", i0);
            motion_activate_joint(i0);

        } else if (strcmp(line, "ENABLE") == 0) {
            fprintf(stderr, "enable\n");
            motion_enable();

        } else if (sscanf(line, "ENABLE_AMP %d", &i0) == 1) {
            fprintf(stderr, "enable amplifier %d\n", i0);
            motion_enable_amplifier(i0);

        } else if (sscanf(line, "JOG_INCR joint=%d vel=%lf offset=%lf", &i0, &d0, &d1) == 3) {
            fprintf(stderr, "jog incremental joint=%d vel=%lf offset=%lf\n", i0, d0, d1);
            motion_jog_incr(i0, d0, d1);

        } else if (strcmp(line, "pass") == 0) {
            fprintf(stderr, "pass 1/1\n");
            pass_count = 0;

        } else if (sscanf(line, "pass %d", &i0) == 1) {
            fprintf(stderr, "pass 1/%d\n", i0);
            pass_index = 1;
            pass_count = i0;

        } else {
            fprintf(stderr, "unknown input: %s\n", line);
            errno = EINVAL;
            return -1;
        }
    } while (compound);

    return 0;
}


int main(int argc, char *argv[]) {
    int pass = 0;

    bool interactive = true;
    FILE *in;

    if (argc == 1) {
        // no input file specified on command-line, use stdin
        in = stdin;
    } else {
        in = fopen(argv[1], "r");
        if (in == NULL) {
            fprintf(stderr, "error opening '%s': %s\n", argv[1], strerror(errno));
            exit(1);
        }
        interactive = false;
    }

    rtapi_set_msg_level(RTAPI_MSG_ALL);

    joints = &emcmotDebug->joints[0];
    init_joints();

    kinType = kinematicsType();

    init_hal_mock();

    do {
        if (emcmotCommand->commandNum == emcmotStatus->commandNumEcho) {
            int r;

            // Motion has caught up, do the next thing
            r = handle_input(interactive, in);
            if (r != 0) {
                if (errno == ENODATA) {
                    // EOF on input, this is a normal terminating condition.
                    exit(0);
                }
                if (!interactive) {
                    // Other input error, handle_input() has logged something
                    // informative already.
                    exit(1);
                }
            }
        } else {
            fprintf(stderr, "WARNING: Motion did not echo command, pausing input\n");
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
    } while(1);

    return 0;
}
