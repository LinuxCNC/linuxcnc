/********************************************************************
* Description: motion.c
*   Main module initialisation and cleanup routines.
*
* Author:
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
* Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
********************************************************************/

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "gomc_env.h"
#include "kins_api.h"
#include "mot_api.h"
#include <string.h>
#include "motion.h"
#include "motion_struct.h"
#include "mot_priv.h"

#include "tp_api.h"
#include "home_api.h"
#include "rtapi_math.h"
#include "axis.h"

#include "motctl_api.h"
#include "motstat_api.h"

/* From motctl_handlers.c / motstat_handlers.c */
typedef struct motctl_ctx motctl_ctx_t;
typedef struct motstat_ctx motstat_ctx_t;
extern motctl_callbacks_t motctl_get_callbacks(motctl_ctx_t **ctx_out);
extern void motctl_init_ctx(motctl_ctx_t *mc, emcmot_struct_t *mot, double timeout);
extern motstat_callbacks_t motstat_get_callbacks(motstat_ctx_t **ctx_out);
extern void motstat_init_ctx(motstat_ctx_t *mc, emcmot_struct_t *mot, axis_inst_t *ai);


/***********************************************************************
*                    MODULE PARAMETERS                                 *
************************************************************************/
/***********************************************************************
*                  GLOBAL VARIABLE DEFINITIONS                         *
************************************************************************/

/***********************************************************************
*                  LOCAL VARIABLE DECLARATIONS                         *
************************************************************************/

/* PFMT: prefix HAL pin/param format strings with the instance name.
   Usage: gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &p, id, PFMT("joint.%d.pos-cmd"), num)
   When pin_prefix is empty (default/no alias), expands to bare "joint.%d.pos-cmd".
   When pin_prefix is "name.", expands to "name.joint.%d.pos-cmd". */
#define PFMT(fmt) "%s" fmt, inst->pin_prefix

/***********************************************************************
*           KINEMATICS API WRAPPERS (via GMI kins_callbacks_t)         *
************************************************************************/

/* These functions delegate to the registered kins API callbacks.
   Called from command.c and control.c which pass their local inst. */

int motmod_kinematicsForward(motmod_inst_t *inst,
                      const double *joint,
                      struct EmcPose *world,
                      const KINEMATICS_FORWARD_FLAGS *fflags,
                      KINEMATICS_INVERSE_FLAGS *iflags)
{
    const kins_callbacks_t *kins = (const kins_callbacks_t *)inst->kins;
    uint64_t ifl = *iflags;
    int32_t result = kins->forward(kins->ctx, joint, (kins_pose_t *)world,
                         (uint64_t)*fflags, &ifl);
    *iflags = ifl;
    return result;
}

int motmod_kinematicsInverse(motmod_inst_t *inst,
                      const struct EmcPose *world,
                      double *joint,
                      const KINEMATICS_INVERSE_FLAGS *iflags,
                      KINEMATICS_FORWARD_FLAGS *fflags)
{
    const kins_callbacks_t *kins = (const kins_callbacks_t *)inst->kins;
    uint64_t ffl = *fflags;
    int32_t result = kins->inverse(kins->ctx, (const kins_pose_t *)world, joint,
                         (uint64_t)*iflags, &ffl);
    *fflags = ffl;
    return result;
}

KINEMATICS_TYPE motmod_kinematicsType(motmod_inst_t *inst)
{
    const kins_callbacks_t *kins = (const kins_callbacks_t *)inst->kins;
    return (KINEMATICS_TYPE)kins->type(kins->ctx);
}

int motmod_kinematicsSwitchable(motmod_inst_t *inst)
{
    const kins_callbacks_t *kins = (const kins_callbacks_t *)inst->kins;
    return kins->switchable(kins->ctx);
}

int motmod_kinematicsSwitch(motmod_inst_t *inst, int switchkins_type)
{
    const kins_callbacks_t *kins = (const kins_callbacks_t *)inst->kins;
    return kins->switch_(kins->ctx, switchkins_type);
}

/***********************************************************************
*           MOT API CALLBACKS (provided by motmod, consumed by         *
*           tpmod and homemod via mot_api_get())                       *
************************************************************************/

/* --- I/O callbacks --- */

static void gmi_mot_dio_write(void *ctx, int32_t index, int8_t value)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; emcmotDioWrite(inst, index, value);
}

static void gmi_mot_aio_write(void *ctx, int32_t index, double value)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; emcmotAioWrite(inst, index, value);
}

/* --- Rotary unlock --- */

static void gmi_mot_set_rotary_unlock(void *ctx, int32_t jnum, int32_t unlock)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; emcmotSetRotaryUnlock(inst, jnum, unlock);
}

static int32_t gmi_mot_get_rotary_unlock(void *ctx, int32_t jnum)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return emcmotGetRotaryIsUnlocked(inst, jnum);
}

/* --- Axis limits --- */

static double gmi_mot_axis_get_vel_limit(void *ctx, int32_t axis)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return axis_get_vel_limit((axis_inst_t *)inst->axis_inst, axis);
}

static double gmi_mot_axis_get_acc_limit(void *ctx, int32_t axis)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return axis_get_acc_limit((axis_inst_t *)inst->axis_inst, axis);
}

/* --- Config getters (inst->config fields, read-only) --- */

static int32_t gmi_mot_cfg_get_arc_blend_enable(void *ctx)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->config->arcBlendEnable;
}

static int32_t gmi_mot_cfg_get_arc_blend_gap_cycles(void *ctx)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->config->arcBlendGapCycles;
}

static int32_t gmi_mot_cfg_get_arc_blend_opt_depth(void *ctx)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->config->arcBlendOptDepth;
}

static double gmi_mot_cfg_get_arc_blend_ramp_freq(void *ctx)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->config->arcBlendRampFreq;
}

static double gmi_mot_cfg_get_arc_blend_tangent_kink_ratio(void *ctx)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->config->arcBlendTangentKinkRatio;
}

static double gmi_mot_cfg_get_max_feed_scale(void *ctx)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->config->maxFeedScale;
}

static int32_t gmi_mot_cfg_get_num_aio(void *ctx)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->config->numAIO;
}

static int32_t gmi_mot_cfg_get_num_dio(void *ctx)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->config->numDIO;
}

static int32_t gmi_mot_cfg_get_num_spindles(void *ctx)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->config->numSpindles;
}

/* --- Status getters --- */

static double gmi_mot_status_get_net_feed_scale(void *ctx)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->status->net_feed_scale;
}

static int32_t gmi_mot_status_get_stepping(void *ctx)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->status->stepping;
}

static double gmi_mot_status_get_current_vel(void *ctx)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->status->current_vel;
}

static int32_t gmi_mot_status_get_spindle_sync(void *ctx)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->status->spindleSync;
}

static double gmi_mot_status_get_spindle_revs(void *ctx, int32_t spindle)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->status->spindle_status[spindle].spindleRevs;
}

static int32_t gmi_mot_status_get_spindle_direction(void *ctx, int32_t spindle)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->status->spindle_status[spindle].direction;
}

static int32_t gmi_mot_status_get_spindle_at_speed(void *ctx, int32_t spindle)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->status->spindle_status[spindle].at_speed;
}

static double gmi_mot_status_get_spindle_speed_in(void *ctx, int32_t spindle)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->status->spindle_status[spindle].spindleSpeedIn;
}

static int32_t gmi_mot_status_get_spindle_index_enable(void *ctx, int32_t spindle)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->status->spindle_status[spindle].spindle_index_enable;
}

static uint8_t gmi_mot_status_get_enables_new(void *ctx)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->status->enables_new;
}

static double gmi_mot_status_get_spindle_speed(void *ctx, int32_t spindle)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->status->spindle_status[spindle].speed;
}

/* --- Status setters --- */

static void gmi_mot_status_set_current_vel(void *ctx, double vel)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->status->current_vel = vel;
}

static void gmi_mot_status_set_requested_vel(void *ctx, double vel)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->status->requested_vel = vel;
}

static void gmi_mot_status_set_distance_to_go(void *ctx, double dist)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->status->distance_to_go = dist;
}

static void gmi_mot_status_set_dtg(void *ctx, mot_pose_t *dtg)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; memcpy(&inst->status->dtg, dtg, sizeof(EmcPose));
}

static void gmi_mot_status_or_motion_flag(void *ctx, uint32_t bits)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->status->motionFlag |= bits;
}

static void gmi_mot_status_set_enables_queued(void *ctx, uint8_t val)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->status->enables_queued = val;
}

static void gmi_mot_status_set_spindle_sync(void *ctx, int32_t val)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->status->spindleSync = val;
}

static void gmi_mot_status_set_tcqlen(void *ctx, uint32_t len)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->status->tcqlen = len;
}

static void gmi_mot_status_set_spindle_speed(void *ctx, int32_t spindle, double speed)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->status->spindle_status[spindle].speed = speed;
}

static void gmi_mot_status_set_spindle_index_enable(void *ctx, int32_t spindle, int32_t enable)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->status->spindle_status[spindle].spindle_index_enable = enable;
}

/* --- Joint accessors (for homing subsystem) --- */

static int32_t gmi_mot_get_num_joints(void *ctx)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;
    return inst->num_joints;
}

static int32_t gmi_mot_joint_get_active_flag(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return GET_JOINT_ACTIVE_FLAG(&inst->joints[jno]);
}

static int32_t gmi_mot_joint_get_inpos_flag(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return GET_JOINT_INPOS_FLAG(&inst->joints[jno]);
}

static int32_t gmi_mot_joint_get_free_tp_active(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return inst->joints[jno].free_tp.active;
}

static void gmi_mot_joint_set_free_tp_enable(void *ctx, int32_t jno, int32_t enable)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->joints[jno].free_tp.enable = enable;
}

static double gmi_mot_joint_get_free_tp_pos_cmd(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return inst->joints[jno].free_tp.pos_cmd;
}

static void gmi_mot_joint_set_free_tp_pos_cmd(void *ctx, int32_t jno, double val)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->joints[jno].free_tp.pos_cmd = val;
}

static double gmi_mot_joint_get_free_tp_curr_pos(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return inst->joints[jno].free_tp.curr_pos;
}

static void gmi_mot_joint_set_free_tp_curr_pos(void *ctx, int32_t jno, double val)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->joints[jno].free_tp.curr_pos = val;
}

static void gmi_mot_joint_set_free_tp_max_vel(void *ctx, int32_t jno, double vel)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->joints[jno].free_tp.max_vel = vel;
}

static double gmi_mot_joint_get_free_tp_max_vel(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return inst->joints[jno].free_tp.max_vel;
}

static double gmi_mot_joint_get_pos_cmd(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return inst->joints[jno].pos_cmd;
}

static void gmi_mot_joint_set_pos_cmd(void *ctx, int32_t jno, double val)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->joints[jno].pos_cmd = val;
}

static double gmi_mot_joint_get_pos_fb(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return inst->joints[jno].pos_fb;
}

static void gmi_mot_joint_set_pos_fb(void *ctx, int32_t jno, double val)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->joints[jno].pos_fb = val;
}

static double gmi_mot_joint_get_motor_pos_fb(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return inst->joints[jno].motor_pos_fb;
}

static double gmi_mot_joint_get_motor_offset(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return inst->joints[jno].motor_offset;
}

static void gmi_mot_joint_set_motor_offset(void *ctx, int32_t jno, double val)
{    motmod_inst_t *inst = (motmod_inst_t *)ctx; inst->joints[jno].motor_offset = val;
}

static double gmi_mot_joint_get_backlash_filt(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return inst->joints[jno].backlash_filt;
}

static double gmi_mot_joint_get_vel_limit(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return inst->joints[jno].vel_limit;
}

static double gmi_mot_joint_get_max_pos_limit(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return inst->joints[jno].max_pos_limit;
}

static double gmi_mot_joint_get_min_pos_limit(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return inst->joints[jno].min_pos_limit;
}

static int32_t gmi_mot_joint_get_on_pos_limit(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return inst->joints[jno].on_pos_limit;
}

static int32_t gmi_mot_joint_get_on_neg_limit(void *ctx, int32_t jno)
{
    motmod_inst_t *inst = (motmod_inst_t *)ctx;

    return inst->joints[jno].on_neg_limit;
}

/* mot API callback table (template — per-instance copy gets ctx set in New()) */
static const mot_callbacks_t motmod_mot_callbacks_template = GMI_MOT_CALLBACKS;

/***********************************************************************
*                   LOCAL FUNCTION PROTOTYPES                          *
************************************************************************/
/* init_hal_io() exports HAL pins and parameters making data from
   the realtime control module visible and usable by the world
*/
static int init_hal_io(motmod_inst_t *inst);

/* functions called by init_hal_io() */

// halpins for ALL joints (kinematic joints and extra joints):
static int export_joint(motmod_inst_t *inst, int num, joint_hal_t * addr);
// additional halpins for extrajoints:
static int export_extrajoint(motmod_inst_t *inst, int num, extrajoint_hal_t * addr);

static int export_spindle(motmod_inst_t *inst, int num, spindle_hal_t * addr);

/* init_comm_buffers() allocates and initializes the command,
   status, and error buffers used to communicate with the user
   space parts of emc.
*/
static int init_comm_buffers(motmod_inst_t *inst);

/* export_functions() exports realtime functions for the motion controller.
   Thread creation is handled externally (by the launcher loading the
   threads component); motmod only exports its functions here. The caller
   is responsible for adding these functions to the appropriate threads via
   `addf` (e.g., `addf motion-command-handler servo-thread`).
*/
static int export_functions(motmod_inst_t *inst);

/* functions called by export_functions() */
static int setTrajCycleTime(motmod_inst_t *inst, double secs);
static int setServoCycleTime(motmod_inst_t *inst, double secs);

static int module_intfc(motmod_inst_t *inst);
static int tp_init(motmod_inst_t *inst);
/***********************************************************************
*                     PUBLIC FUNCTION CODE                             *
************************************************************************/
int joint_is_lockable(motmod_inst_t *inst, int joint_num) {
    return (inst->unlock_joints_mask & (1 << joint_num) );
}

void switch_to_teleop_mode(motmod_inst_t *inst) {
    int joint_num;
    emcmot_joint_t *joint;
    const gomc_log_t *log = inst->log;

    if (inst->config->kinType != KINEMATICS_IDENTITY) {
        if (!inst->all_homed) {
            gomc_log_errorf(log, inst->name, "all joints must be homed before going into teleop mode");
            return;
        }
    }

    for (joint_num = 0; joint_num < ALL_JOINTS; joint_num++) {
        joint = &inst->joints[joint_num];
        if (joint != 0) { joint->free_tp.enable = 0; }
    }

    inst->internal->teleoperating = 1;
    inst->internal->coordinating  = 0;
}


void emcmot_config_change(motmod_inst_t *inst)
{
    if (inst->config->head == inst->config->tail) {
	inst->config->config_num++;
	inst->status->config_num = inst->config->config_num;
	inst->config->head++;
    }
}

int count_names(char *names[]){
  int namecount = 0;
  int i;
  for (i = 0; i < MOTMOD_MAX_IO; i++) {
    if (((names[i] == NULL) || (*names[i] == 0))){
      break;
    }
    namecount = i + 1;
  }
  return namecount;
}

static int module_intfc(motmod_inst_t *inst) {
    inst->tp_api->init(inst->tp_api->ctx);
    return 0;
}

static int tp_init(motmod_inst_t *inst) {
    const gomc_log_t *log = inst->log;
    if (-1 == inst->tp_api->create(inst->tp_api->ctx, DEFAULT_TC_QUEUE_SIZE,inst->comp_id)) {
        gomc_log_errorf(log, inst->name, "MOTION: tp_api->create failed\n");
        return -1;
    }
    // tpInit is called from tp_api->create
    inst->tp_api->set_cycle_time(inst->tp_api->ctx, inst->config->trajCycleTime);
    inst->tp_api->set_vmax(inst->tp_api->ctx, inst->status->vel, inst->status->vel);
    inst->tp_api->set_amax(inst->tp_api->ctx, inst->status->acc);
    inst->tp_api->set_pos(inst->tp_api->ctx, (tp_pose_t *)&inst->status->carte_pos_cmd);
    return 0;
}

/***********************************************************************
*              ARGUMENT PARSING (replaces RTAPI_MP_* macros)           *
************************************************************************/

/* Parse "key=value" from argv[].  Supports int, long, and array-of-string.
   Returns 0 on success, -1 if a required value is malformed. */
static int parse_argv(motmod_inst_t *inst, int argc, const char **argv)
{
    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (!a) continue;

        if (strncmp(a, "base_period_nsec=", 17) == 0) inst->base_period_nsec = atol(a + 17);
        else if (strncmp(a, "base_thread_fp=", 15) == 0)  inst->base_thread_fp = atoi(a + 15);
        else if (strncmp(a, "servo_period_nsec=", 18) == 0) inst->servo_period_nsec = atol(a + 18);
        else if (strncmp(a, "traj_period_nsec=", 17) == 0) inst->traj_period_nsec = atol(a + 17);
        else if (strncmp(a, "num_spindles=", 13) == 0)    inst->num_spindles = atoi(a + 13);
        else if (strncmp(a, "num_joints=", 11) == 0)      inst->num_joints = atoi(a + 11);
        else if (strncmp(a, "num_extrajoints=", 16) == 0) inst->num_extrajoints = atoi(a + 16);
        else if (strncmp(a, "num_dio=", 8) == 0)          inst->num_dio = atoi(a + 8);
        else if (strncmp(a, "num_aio=", 8) == 0)          inst->num_aio = atoi(a + 8);
        else if (strncmp(a, "num_misc_error=", 15) == 0)  inst->num_misc_error = atoi(a + 15);
        else if (strncmp(a, "unlock_joints_mask=", 19) == 0) inst->unlock_joints_mask = atoi(a + 19);
        else if (strncmp(a, "kins_instance=", 14) == 0) strncpy(inst->kins_inst_name, a + 14, HAL_NAME_LEN - 1);
        else if (strncmp(a, "tp_instance=", 12) == 0) strncpy(inst->tp_inst_name, a + 12, HAL_NAME_LEN - 1);
        else if (strncmp(a, "home_instance=", 14) == 0) strncpy(inst->home_inst_prefix, a + 14, HAL_NAME_LEN - 1);
        /* Array-of-string params: inst->names_din=foo,bar,baz */
        else if (strncmp(a, "names_din=", 10) == 0) {
            const char *p = a + 10;
            for (int n = 0; n < MOTMOD_MAX_IO && *p; n++) {
                const char *c = strchr(p, ',');
                size_t len = c ? (size_t)(c - p) : strlen(p);
                inst->names_din[n] = strndup(p, len);
                p = c ? c + 1 : p + len;
            }
        }
        else if (strncmp(a, "names_dout=", 11) == 0) {
            const char *p = a + 11;
            for (int n = 0; n < MOTMOD_MAX_IO && *p; n++) {
                const char *c = strchr(p, ',');
                size_t len = c ? (size_t)(c - p) : strlen(p);
                inst->names_dout[n] = strndup(p, len);
                p = c ? c + 1 : p + len;
            }
        }
        else if (strncmp(a, "names_ain=", 10) == 0) {
            const char *p = a + 10;
            for (int n = 0; n < MOTMOD_MAX_IO && *p; n++) {
                const char *c = strchr(p, ',');
                size_t len = c ? (size_t)(c - p) : strlen(p);
                inst->names_ain[n] = strndup(p, len);
                p = c ? c + 1 : p + len;
            }
        }
        else if (strncmp(a, "names_aout=", 11) == 0) {
            const char *p = a + 11;
            for (int n = 0; n < MOTMOD_MAX_IO && *p; n++) {
                const char *c = strchr(p, ',');
                size_t len = c ? (size_t)(c - p) : strlen(p);
                inst->names_aout[n] = strndup(p, len);
                p = c ? c + 1 : p + len;
            }
        }
        else if (strncmp(a, "names_misc_errors=", 18) == 0) {
            const char *p = a + 18;
            for (int n = 0; n < MOTMOD_MAX_IO && *p; n++) {
                const char *c = strchr(p, ',');
                size_t len = c ? (size_t)(c - p) : strlen(p);
                inst->names_misc_errors[n] = strndup(p, len);
                p = c ? c + 1 : p + len;
            }
        }
        /* Ignore unknown params — HAL file may pass extra args */
    }
    return 0;
}

static void free_name_arrays(motmod_inst_t *inst)
{
    for (int i = 0; i < MOTMOD_MAX_IO; i++) {
        free(inst->names_din[i]);   inst->names_din[i] = NULL;
        free(inst->names_dout[i]);  inst->names_dout[i] = NULL;
        free(inst->names_ain[i]);   inst->names_ain[i] = NULL;
        free(inst->names_aout[i]);  inst->names_aout[i] = NULL;
        free(inst->names_misc_errors[i]); inst->names_misc_errors[i] = NULL;
    }
}

/***********************************************************************
*                    cmod LIFECYCLE                                     *
************************************************************************/

/* Forward declaration — filled in below. */
static void motmod_Destroy(cmod_t *self);

static int motmod_init(cmod_t *self);

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    int retval;
    motmod_inst_t *inst;
    cmod_t *cmod;
    const gomc_hal_t *hal = (const gomc_hal_t *)env->hal;
    const gomc_log_t *log = (const gomc_log_t *)env->log;

    gomc_log_infof(log, name, "MOTION: New('%s') starting...\n", name);

    /* Allocate per-instance state */
    inst = calloc(1, sizeof(*inst));
    if (!inst) {
        gomc_log_errorf(log, name, "MOTION: failed to allocate instance\n");
        return -1;
    }
    inst->env = env;
    inst->name = name;
    inst->hal = env->hal;
    inst->log = env->log;
    inst->rtapi = env->rtapi;
    inst->ctl_first_pass = 1;

    /* Defaults for module parameters (overridden by parse_argv) */
    inst->servo_period_nsec = 1000000;
    inst->num_spindles = 1;
    inst->num_joints = EMCMOT_MAX_JOINTS;
    inst->num_misc_error = -1;
    strncpy(inst->kins_inst_name, "trivkins", HAL_NAME_LEN - 1);
    strncpy(inst->tp_inst_name, "tpmod", HAL_NAME_LEN - 1);
    strncpy(inst->home_inst_prefix, "homemod", HAL_NAME_LEN - 1);

    /* Allocate per-instance axis state */
    inst->axis_inst = axis_inst_new();
    if (!inst->axis_inst) {
        gomc_log_errorf(log, inst->name, "MOTION: failed to allocate axis instance\n");
        free(inst);
        return -1;
    }

    /* Set pin_prefix: empty for default module name (bare pins), "name." for aliases */
    if (strcmp(name, "motmod") == 0) {
        inst->pin_prefix[0] = '\0';
    } else {
        snprintf(inst->pin_prefix, sizeof(inst->pin_prefix), "%s.", name);
    }

    /* Allocate cmod handle */
    cmod = calloc(1, sizeof(*cmod));
    if (!cmod) {
        gomc_log_errorf(log, inst->name, "MOTION: failed to allocate cmod\n");
        free(inst);
        return -1;
    }

    /* Parse module arguments from argv */
    if (parse_argv(inst, argc, argv) != 0) {
        gomc_log_errorf(log, inst->name, "MOTION: argument parsing failed\n");
        free(cmod);
        free(inst);
        return -1;
    }

    /* connect to the HAL and RTAPI */
    inst->comp_id = hal->init(hal->ctx, name, env->dl_handle, GOMC_HAL_COMP_REALTIME);
    if (inst->comp_id < 0) {
	gomc_log_errorf(log, inst->name, "MOTION: hal_init_ex() failed\n");
	free(cmod);
	free(inst);
	return -1;
    }
    /* Register the mot reverse-callback API so tpmod/homemod can look it up
       in their Init() functions. */
    /* Per-instance mot callbacks with ctx = inst */
    mot_callbacks_t *mot_cb = malloc(sizeof(mot_callbacks_t));
    if (!mot_cb) { hal->exit(hal->ctx, inst->comp_id); return -1; }
    *mot_cb = motmod_mot_callbacks_template;
    mot_cb->ctx = inst;
    inst->mot_cb = mot_cb;
    retval = mot_api_register(env->api, name, mot_cb);
    if (retval != 0) {
	gomc_log_errorf(log, inst->name, "MOTION: failed to register mot API: %d\n", retval);
	hal->exit(hal->ctx, inst->comp_id);
	return -1;
    }

    /* Register motctl/motstat GMI APIs so milltask can look them up. */
    {
        motctl_ctx_t *mctl_ctx = NULL;
        motstat_ctx_t *mstat_ctx = NULL;

        motctl_callbacks_t *motctl_cb = calloc(1, sizeof(*motctl_cb));
        motstat_callbacks_t *motstat_cb = calloc(1, sizeof(*motstat_cb));
        if (!motctl_cb || !motstat_cb) {
            gomc_log_errorf(log, inst->name, "MOTION: failed to allocate motctl/motstat callbacks\n");
            hal->exit(hal->ctx, inst->comp_id);
            return -1;
        }

        *motctl_cb = motctl_get_callbacks(&mctl_ctx);
        retval = motctl_api_register(env->api, name, motctl_cb);
        if (retval != 0) {
            gomc_log_errorf(log, inst->name, "MOTION: failed to register motctl API: %d\n", retval);
            hal->exit(hal->ctx, inst->comp_id);
            return -1;
        }

        *motstat_cb = motstat_get_callbacks(&mstat_ctx);
        retval = motstat_api_register(env->api, name, motstat_cb);
        if (retval != 0) {
            gomc_log_errorf(log, inst->name, "MOTION: failed to register motstat API: %d\n", retval);
            hal->exit(hal->ctx, inst->comp_id);
            return -1;
        }

        inst->motctl_ctx = mctl_ctx;
        inst->motstat_ctx = mstat_ctx;
        inst->motctl_cb = motctl_cb;
        inst->motstat_cb = motstat_cb;
    }

    if (( inst->num_joints < 1 ) || ( inst->num_joints > EMCMOT_MAX_JOINTS )) {
	gomc_log_errorf(log, inst->name, "MOTION: inst->num_joints is %d, must be between 1 and %d\n", inst->num_joints, EMCMOT_MAX_JOINTS);
	hal->exit(hal->ctx, inst->comp_id);
	return -1;
    }

    if (( inst->num_extrajoints < 0 ) || ( inst->num_extrajoints > inst->num_joints )) {
	gomc_log_errorf(log, inst->name, "\nMOTION: inst->num_extrajoints is %d, must be between 0 and %d\n\n", inst->num_extrajoints, inst->num_joints);
	hal->exit(hal->ctx, inst->comp_id);
	return -1;
    }
    if (inst->num_extrajoints > 0) {
	gomc_log_errorf(log, inst->name, "\nMOTION: kinematicjoints=%2d\n            extrajoints=%2d\n           Total joints=%2d\n\n",
            inst->num_joints-inst->num_extrajoints, inst->num_extrajoints, inst->num_joints);
    }

    if (( inst->num_spindles < 0 ) || ( inst->num_spindles > EMCMOT_MAX_SPINDLES )) {
	gomc_log_errorf(log, inst->name, "MOTION: inst->num_spindles is %d, must be between 0 and %d\n", inst->num_spindles, EMCMOT_MAX_SPINDLES);
	hal->exit(hal->ctx, inst->comp_id);
	return -1;
    }

    if(inst->num_dio && (inst->names_dout[0] || inst->names_din[0])){
      gomc_log_errorf(log, inst->name, "MOTION: Can't specify both names and number for digital pins\n");
      return -1;
    }
    else if(inst->names_dout[0] || inst->names_din[0]){
      inst->num_dio = count_names(inst->names_dout);
      inst->num_dio = (inst->num_dio > count_names(inst->names_din)) ? inst->num_dio : count_names(inst->names_din);
    }
    else if(!inst->num_dio){
      inst->num_dio = DEFAULT_DIO;
    }


    if (( inst->num_dio < 1 ) || ( inst->num_dio > EMCMOT_MAX_DIO )) {
	gomc_log_errorf(log, inst->name, "MOTION: inst->num_dio is %d, must be between 1 and %d\n", inst->num_dio, EMCMOT_MAX_DIO);
	hal->exit(hal->ctx, inst->comp_id);
	return -1;
    }

  if(inst->num_aio && (inst->names_aout[0] || inst->names_ain[0])){
    gomc_log_errorf(log, inst->name, "MOTION: Can't specify both names and number for analog pins\n");
    return -1;
  }
  else if(inst->names_aout[0] || inst->names_ain[0]){
    inst->num_aio = count_names(inst->names_aout);
    inst->num_aio = (inst->num_aio > count_names(inst->names_ain)) ? inst->num_aio : count_names(inst->names_ain);
  }
  else if(!inst->num_aio){
    inst->num_aio = DEFAULT_AIO;
  }

    if (( inst->num_aio < 1 ) || ( inst->num_aio > EMCMOT_MAX_AIO )) {
	gomc_log_errorf(log, inst->name, "MOTION: inst->num_aio is %d, must be between 1 and %d\n", inst->num_aio, EMCMOT_MAX_AIO);
	hal->exit(hal->ctx, inst->comp_id);
	return -1;
    }

  if(inst->num_misc_error != -1 && (inst->names_misc_errors[0])){
    gomc_log_errorf(log, inst->name, "MOTION: Can't specify both names and number for misc error\n");
    return -1;
  }
  else if(inst->names_misc_errors[0]){
    inst->num_misc_error = count_names(inst->names_misc_errors);
  }
  else if (inst->num_misc_error < 0) {
    inst->num_misc_error = DEFAULT_MISC_ERROR;
  }

  if (( inst->num_misc_error < 0 ) || ( inst->num_misc_error > EMCMOT_MAX_MISC_ERROR )) {
    gomc_log_errorf(log, inst->name, "MOTION: inst->num_misc_error is %d, must be between 0 and %d\n", inst->num_misc_error, EMCMOT_MAX_MISC_ERROR);
    hal->exit(hal->ctx, inst->comp_id);
    return -1;
  }

    gomc_log_infof(log, inst->name, "MOTION: New('%s') complete\n", name);

    /* Set up cmod interface */
    cmod->Init    = motmod_init;
    cmod->Start   = NULL;
    cmod->Stop    = NULL;
    cmod->Destroy = motmod_Destroy;
    cmod->priv    = inst;

    *out = cmod;
    return 0;
}

/*
 * motmod Init() — look up APIs registered by other modules during New(),
 * then initialize the trajectory planner and homing subsystem.
 *
 * By the time Init() runs, all modules' New() have completed (APIs
 * registered) and earlier-loaded modules' Init() have also completed
 * (tpmod/homemod have wired their function pointers via the mot API).
 */
static int motmod_init(cmod_t *self)
{
    int retval;
    motmod_inst_t *inst = (motmod_inst_t *)self->priv;
    const cmod_env_t *env = (const cmod_env_t *)inst->env;
    const gomc_hal_t *hal = inst->hal;
    const gomc_log_t *log = inst->log;



    gomc_log_infof(log, inst->name, "MOTION: Init('%s') starting...\n", inst->name);

    /* --- Cross-module API lookups (must come first) --- */

    /* Look up the kinematics API registered by the kins module */
    inst->kins = kins_api_get(env->api, inst->kins_inst_name);
    if (!inst->kins) {
	gomc_log_errorf(log, inst->name, "MOTION: kinematics API not registered (instance '%s', is kins module loaded?)\n", inst->kins_inst_name);
	return -1;
    }

    /* Look up the trajectory planner API registered by the tp module */
    inst->tp_api = tp_api_get(env->api, inst->tp_inst_name);
    if (!inst->tp_api) {
	gomc_log_errorf(log, inst->name, "MOTION: tp API not registered (instance '%s', is tp module loaded?)\n", inst->tp_inst_name);
	return -1;
    }

    /* Look up per-joint homing APIs registered by homemod instances */
    inst->sequence_state = HOME_SEQUENCE_IDLE;
    inst->current_sequence = 0;
    inst->homing_active = 0;
    inst->all_homed = 0;
    {
        int j;
        for (j = 0; j < inst->num_joints + inst->num_extrajoints; j++) {
            char hname[HAL_NAME_LEN + 4];
            snprintf(hname, sizeof(hname), "%s.%d", inst->home_inst_prefix, j);
            const home_callbacks_t *hapi = home_api_get(env->api, hname);
            if (!hapi) {
                gomc_log_errorf(log, inst->name,
                    "MOTION: home API not registered (instance '%s', is home module loaded?)\n", hname);
                return -1;
            }
            inst->joints[j].home_api = hapi;
            env->api->record_consumer(env->api->ctx, inst->name, "home", hname);
        }
    }

    /* Record consumer relationships for introspection */
    env->api->record_consumer(env->api->ctx, inst->name, "kins", inst->kins_inst_name);
    env->api->record_consumer(env->api->ctx, inst->name, "tp", inst->tp_inst_name);

    /* --- Validation (depends on kins) --- */

    if ( (inst->num_extrajoints > 0) && (motmod_kinematicsType(inst) != KINEMATICS_BOTH) ) {
	gomc_log_errorf(log, inst->name, "\nMOTION: nonzero inst->num_extrajoints requires KINEMATICS_BOTH\n\n");
        return -1;
    }

    /* --- HAL pins, shared memory, RT function export --- */

    retval = init_hal_io(inst);
    if (retval != 0) {
	gomc_log_errorf(log, inst->name, "MOTION: init_hal_io() failed\n");
	return -1;
    }

    retval = init_comm_buffers(inst);
    if (retval != 0) {
	gomc_log_errorf(log, inst->name, "MOTION: init_comm_buffers() failed\n");
	return -1;
    }

    /* Wire up motctl/motstat handler contexts now that inst->mot_struct exists. */
    motctl_init_ctx(inst->motctl_ctx, inst->mot_struct, DEFAULT_EMCMOT_COMM_TIMEOUT);
    motstat_init_ctx(inst->motstat_ctx, inst->mot_struct, (axis_inst_t *)inst->axis_inst);

    retval = export_functions(inst);
    if (retval != 0) {
	gomc_log_errorf(log, inst->name, "MOTION: export_functions() failed\n");
	return -1;
    }

    /* --- Subsystem initialization --- */

    if (module_intfc(inst)) {
	gomc_log_errorf(log, inst->name, "MOTION: module_intfc() failed\n");
	return -1;
    }
    if (tp_init(inst)) {
	gomc_log_errorf(log, inst->name, "MOTION: tp_init() failed\n");
	return -1;
    }

    /* Initialize per-joint homing via GMI home API */
    {
        int j;
        for (j = 0; j < inst->num_joints + inst->num_extrajoints; j++) {
            const home_callbacks_t *hapi = (const home_callbacks_t *)inst->joints[j].home_api;
            if (hapi->init(hapi->ctx, inst->comp_id,
                           inst->config->servoCycleTime) != 0) {
                gomc_log_errorf(log, inst->name, "MOTION: homing init failed for joint %d\n", j);
                return -1;
            }
        }
    }

    hal->ready(hal->ctx, inst->comp_id);

    gomc_log_infof(log, inst->name, "MOTION: Init('%s') complete\n", inst->name);
    return 0;
}

static void motmod_Destroy(cmod_t *self)
{
    motmod_inst_t *inst = (motmod_inst_t *)self->priv;
    const gomc_hal_t *hal = inst->hal;
    const gomc_log_t *log = inst->log;

    gomc_log_infof(log, inst->name, "MOTION: Destroy('%s') started.\n", inst->name);

    /* free motion structure */
    if (inst->mot_struct) {
        rtapi_free(inst->mot_struct);
        inst->mot_struct = 0;
    }
    /* free jerk filter buffers */
    if (inst->jerk_filter.buf) {
        rtapi_free(inst->jerk_filter.buf);
        inst->jerk_filter.buf = NULL;
    }
    if (inst->jerk_filter.sum) {
        rtapi_free(inst->jerk_filter.sum);
        inst->jerk_filter.sum = NULL;
    }
    /* disconnect from HAL and RTAPI */
    hal->exit(hal->ctx, inst->comp_id);

    free_name_arrays(inst);

    gomc_log_infof(log, inst->name, "MOTION: Destroy('%s') finished.\n", inst->name);

    /* free per-instance state */
    free(inst->mot_cb);
    if (inst->axis_inst) axis_inst_free((axis_inst_t *)inst->axis_inst);
    free(inst);
    free(self);
}

/***********************************************************************
*                         LOCAL FUNCTION CODE                          *
************************************************************************/

#define CALL_CHECK(expr) do {           \
        int _retval;                    \
        _retval = expr;                 \
        if (_retval) return _retval;    \
    } while (0);

/* init_hal_io() exports HAL pins and parameters making data from
   the realtime control module visible and usable by the world
*/
static int init_hal_io(motmod_inst_t *inst)
{
    int n, retval;
    joint_hal_t      *joint_data;
    extrajoint_hal_t *ejoint_data;
    const gomc_hal_t *hal = inst->hal;
    const gomc_log_t *log = inst->log;

    gomc_log_infof(log, inst->name, "MOTION: init_hal_io() starting...\n");

    /* allocate shared memory for machine data */
    inst->hal_data = hal->malloc(hal->ctx, sizeof(emcmot_hal_data_t));
    if (inst->hal_data == 0) {
	gomc_log_errorf(log, inst->name, "MOTION: inst->hal_data malloc failed\n");
	return -1;
    }

    /* export machine wide hal pins */
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(inst->hal_data->probe_input), inst->comp_id, PFMT("motion.probe-input")));
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &(inst->hal_data->adaptive_feed), inst->comp_id, PFMT("motion.adaptive-feed")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(inst->hal_data->feed_hold), inst->comp_id, PFMT("motion.feed-hold")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(inst->hal_data->feed_inhibit), inst->comp_id, PFMT("motion.feed-inhibit")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(inst->hal_data->homing_inhibit), inst->comp_id, PFMT("motion.homing-inhibit")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(inst->hal_data->jog_inhibit), inst->comp_id, PFMT("motion.jog-inhibit")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(inst->hal_data->jog_stop), inst->comp_id, PFMT("motion.jog-stop")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(inst->hal_data->jog_stop_immediate), inst->comp_id, PFMT("motion.jog-stop-immediate")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(inst->hal_data->tp_reverse), inst->comp_id, PFMT("motion.tp-reverse")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(inst->hal_data->enable), inst->comp_id, PFMT("motion.enable")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(inst->hal_data->is_all_homed), inst->comp_id, PFMT("motion.is-all-homed")));

    /* state tags pins */
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->feed_inches_per_minute), inst->comp_id, PFMT("motion.feed-inches-per-minute")));
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->feed_inches_per_second), inst->comp_id, PFMT("motion.feed-inches-per-second")));
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->feed_mm_per_minute), inst->comp_id, PFMT("motion.feed-mm-per-minute")));
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->feed_mm_per_second), inst->comp_id, PFMT("motion.feed-mm-per-second")));

    /* export motion-synched digital output pins */
    /* export motion digital input pins */
    if (inst->names_din[0]){
        for (n = 0; n < inst->num_dio; n++) {
            if (inst->names_din[n] == NULL || (*inst->names_din[n] == 0)) {break;}
            CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(inst->hal_data->synch_di[n]), inst->comp_id, PFMT("motion.din-%s"), inst->names_din[n]));
        }
    } else {
        for (n = 0; n < inst->num_dio; n++) {
            CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(inst->hal_data->synch_di[n]), inst->comp_id, PFMT("motion.digital-in-%02d"), n));
        }
    }

    if (inst->names_dout[0]){
        for (n = 0; n < inst->num_dio; n++) {
            if (inst->names_dout[n] == NULL || (*inst->names_dout[n] == 0)) {break;}
            CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(inst->hal_data->synch_do[n]), inst->comp_id, PFMT("motion.dout-%s"), inst->names_dout[n]));
        }
    } else {
        for (n = 0; n < inst->num_dio; n++) {
            CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(inst->hal_data->synch_do[n]), inst->comp_id, PFMT("motion.digital-out-%02d"),n));
        }
    }

    /* export motion-synched analog output pins */
    /* export motion analog input pins */
    if (inst->names_ain[0]) {
        for (n = 0; n < inst->num_aio; n++) {
            if (inst->names_ain[n] == NULL || (*inst->names_ain[n] == 0)) {break;}
            CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &(inst->hal_data->analog_input[n]), inst->comp_id, PFMT("motion.ain-%s"), inst->names_ain[n]));
        }
    } else {
        for (n = 0; n < inst->num_aio; n++) {
            CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &(inst->hal_data->analog_input[n]), inst->comp_id, PFMT("motion.analog-in-%02d"), n));
        }
    }
    if (inst->names_aout[0]) {
        for (n = 0; n < inst->num_aio; n++) {
            if (inst->names_aout[n] == NULL || (*inst->names_aout[n] == 0)) {break;}
            CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->analog_output[n]), inst->comp_id, PFMT("motion.aout-%s"), inst->names_aout[n]));
        }
    } else {
        for (n = 0; n < inst->num_aio; n++) {
            CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->analog_output[n]), inst->comp_id, PFMT("motion.analog-out-%02d"), n));
        }
    }

    if (inst->names_misc_errors[0]) {
        for (n = 0; n < inst->num_misc_error; n++) {
            if (inst->names_misc_errors[n] == NULL || (*inst->names_misc_errors[n] == 0)) {break;}
            CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(inst->hal_data->misc_error[n]), inst->comp_id, PFMT("motion.err-%s"), inst->names_misc_errors[n]));
        }
    } else {
        /* export misc error input pins */
        for (n = 0; n < inst->num_misc_error; n++) {
            CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(inst->hal_data->misc_error[n]), inst->comp_id, PFMT("motion.misc-error-%02d"), n));
        }
    }

    /* export machine wide hal pins */
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(inst->hal_data->motion_enabled), inst->comp_id, PFMT("motion.motion-enabled")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(inst->hal_data->in_position), inst->comp_id, PFMT("motion.in-position")));
    CALL_CHECK(gomc_hal_pin_s32_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->motion_type), inst->comp_id, PFMT("motion.motion-type")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(inst->hal_data->coord_mode), inst->comp_id, PFMT("motion.coord-mode")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(inst->hal_data->teleop_mode), inst->comp_id, PFMT("motion.teleop-mode")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(inst->hal_data->coord_error), inst->comp_id, PFMT("motion.coord-error")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(inst->hal_data->on_soft_limit), inst->comp_id, PFMT("motion.on-soft-limit")));
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->current_vel), inst->comp_id, PFMT("motion.current-vel")));
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->requested_vel), inst->comp_id, PFMT("motion.requested-vel")));
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->distance_to_go), inst->comp_id, PFMT("motion.distance-to-go")));
    CALL_CHECK(gomc_hal_pin_s32_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->program_line), inst->comp_id, PFMT("motion.program-line")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(inst->hal_data->jog_is_active), inst->comp_id, PFMT("motion.jog-is-active")));

    /* export debug parameters */
    /* these can be used to view any internal variable, simply change a line
       in control.c:output_to_hal() and recompile */
    CALL_CHECK(gomc_hal_param_bit_newf(hal, GOMC_HAL_RO, (gomc_hal_bit_t *)&(inst->hal_data->debug_bit_0), inst->comp_id, PFMT("motion.debug-bit-0")));
    CALL_CHECK(gomc_hal_param_bit_newf(hal, GOMC_HAL_RO, (gomc_hal_bit_t *)&(inst->hal_data->debug_bit_1), inst->comp_id, PFMT("motion.debug-bit-1")));
    CALL_CHECK(gomc_hal_param_float_newf(hal, GOMC_HAL_RO, &(inst->hal_data->debug_float_0), inst->comp_id, PFMT("motion.debug-float-0")));
    CALL_CHECK(gomc_hal_param_float_newf(hal, GOMC_HAL_RO, &(inst->hal_data->debug_float_1), inst->comp_id, PFMT("motion.debug-float-1")));
    CALL_CHECK(gomc_hal_param_float_newf(hal, GOMC_HAL_RO, &(inst->hal_data->debug_float_2), inst->comp_id, PFMT("motion.debug-float-2")));
    CALL_CHECK(gomc_hal_param_float_newf(hal, GOMC_HAL_RO, &(inst->hal_data->debug_float_3), inst->comp_id, PFMT("motion.debug-float-3")));
    CALL_CHECK(gomc_hal_param_s32_newf(hal, GOMC_HAL_RO, &(inst->hal_data->debug_s32_0), inst->comp_id, PFMT("motion.debug-s32-0")));
    CALL_CHECK(gomc_hal_param_s32_newf(hal, GOMC_HAL_RO, &(inst->hal_data->debug_s32_1), inst->comp_id, PFMT("motion.debug-s32-1")));

    // FIXME - debug only, remove later
    // export HAL parameters for some trajectory planner internal variables
    // so they can be scoped
    CALL_CHECK(gomc_hal_param_float_newf(hal, GOMC_HAL_RO, &(inst->hal_data->traj_pos_out), inst->comp_id, PFMT("traj.pos_out")));
    CALL_CHECK(gomc_hal_param_float_newf(hal, GOMC_HAL_RO, &(inst->hal_data->traj_vel_out), inst->comp_id, PFMT("traj.vel_out")));
    CALL_CHECK(gomc_hal_param_u32_newf(hal, GOMC_HAL_RO, &(inst->hal_data->traj_active_tc), inst->comp_id, PFMT("traj.active_tc")));

    for (n = 0; n < 4; n++) {
        CALL_CHECK(gomc_hal_param_float_newf(hal, GOMC_HAL_RO, &(inst->hal_data->tc_pos[n]), inst->comp_id, PFMT("tc.%d.pos"), n));
        CALL_CHECK(gomc_hal_param_float_newf(hal, GOMC_HAL_RO, &(inst->hal_data->tc_vel[n]), inst->comp_id, PFMT("tc.%d.vel"), n));
        CALL_CHECK(gomc_hal_param_float_newf(hal, GOMC_HAL_RO, &(inst->hal_data->tc_acc[n]), inst->comp_id, PFMT("tc.%d.acc"), n));
    }
    // end of exporting trajectory planner internals

    // export timing related HAL pins so they can be scoped and/or connected
    CALL_CHECK(gomc_hal_pin_u32_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->last_period), inst->comp_id, PFMT("motion.servo.last-period")));

    // export timing related HAL pins so they can be scoped
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->tooloffset_x), inst->comp_id, PFMT("motion.tooloffset.x")));
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->tooloffset_y), inst->comp_id, PFMT("motion.tooloffset.y")));
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->tooloffset_z), inst->comp_id, PFMT("motion.tooloffset.z")));
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->tooloffset_a), inst->comp_id, PFMT("motion.tooloffset.a")));
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->tooloffset_b), inst->comp_id, PFMT("motion.tooloffset.b")));
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->tooloffset_c), inst->comp_id, PFMT("motion.tooloffset.c")));
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->tooloffset_u), inst->comp_id, PFMT("motion.tooloffset.u")));
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->tooloffset_v), inst->comp_id, PFMT("motion.tooloffset.v")));
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(inst->hal_data->tooloffset_w), inst->comp_id, PFMT("motion.tooloffset.w")));

    /* Always create switchkins-type pin; it's a no-op if kins isn't switchable. */
    CALL_CHECK(gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &(inst->hal_data->switchkins_type), inst->comp_id, PFMT("motion.switchkins-type")));

    /* initialize machine wide pins and parameters */
    *(inst->hal_data->adaptive_feed) = 1.0;
    *(inst->hal_data->feed_hold) = 0;
    *(inst->hal_data->feed_inhibit) = 0;
    *(inst->hal_data->homing_inhibit) = 0;
    *(inst->hal_data->jog_inhibit) = 0;
    *(inst->hal_data->jog_stop) = 0;
    *(inst->hal_data->jog_stop_immediate) = 0;
    *(inst->hal_data->is_all_homed) = 0;

    *(inst->hal_data->probe_input) = 0;
    /* default value of enable is TRUE, so simple machines
       can leave it disconnected */
    *(inst->hal_data->enable) = 1;

    /* motion synched dio, init to not enabled */
    for (n = 0; n < inst->num_dio; n++) {
        *(inst->hal_data->synch_do[n]) = 0;
        *(inst->hal_data->synch_di[n]) = 0;
    }

    for (n = 0; n < inst->num_aio; n++) {
        *(inst->hal_data->analog_output[n]) = 0.0;
        *(inst->hal_data->analog_input[n]) = 0.0;
    }

    for (n = 0; n < inst->num_misc_error; n++) {
        *(inst->hal_data->misc_error[n]) = 0;
    }

    /*! \todo FIXME - these don't really need initialized, since they are written
       with data from the inst->status struct */
    *(inst->hal_data->motion_enabled) = 0;
    *(inst->hal_data->in_position) = 0;
    *(inst->hal_data->motion_type) = 0;
    *(inst->hal_data->coord_mode) = 0;
    *(inst->hal_data->teleop_mode) = 0;
    *(inst->hal_data->coord_error) = 0;
    *(inst->hal_data->on_soft_limit) = 0;

    /* init debug parameters */
    inst->hal_data->debug_bit_0 = 0;
    inst->hal_data->debug_bit_1 = 0;
    inst->hal_data->debug_float_0 = 0.0;
    inst->hal_data->debug_float_1 = 0.0;
    inst->hal_data->debug_float_2 = 0.0;
    inst->hal_data->debug_float_3 = 0.0;

    *(inst->hal_data->last_period) = 0;

    /* export spindle pins and params */
    for (n = 0; n < inst->num_spindles; n++) {
        retval = export_spindle(inst, n, &(inst->hal_data->spindle[n]));
        if (retval != 0){
            gomc_log_errorf(log, inst->name, "MOTION: spindle %d pin export failed", n);
            return -1;
        }
    }
    /* export joint pins and parameters */
    for (n = 0; n < inst->num_joints; n++) {
        joint_data = &(inst->hal_data->joint[n]);
        /* export all vars */
        retval = export_joint(inst, n, joint_data);
        if (retval != 0) {
            gomc_log_errorf(log, inst->name, "MOTION: joint %d pin/param export failed\n", n);
            return -1;
        }
        *(joint_data->amp_enable) = 0;

        /* We'll init the index model to EXT_ENCODER_INDEX_MODEL_RAW for now,
           because it is always supported. */
    }
    /* export joint pins and parameters */
    for (n = 0; n < inst->num_extrajoints; n++) {
        ejoint_data = &(inst->hal_data->ejoint[n]);
        retval = export_extrajoint(inst, n + inst->num_joints - inst->num_extrajoints,ejoint_data);
        if (retval != 0) {
            gomc_log_errorf(log, inst->name, "MOTION: ejoint %d pin/param export failed\n", n);
            return -1;
        }
    }

    CALL_CHECK(axis_init_hal_io((axis_inst_t *)inst->axis_inst, inst->hal, inst->log, inst->comp_id, inst->pin_prefix));

    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(inst->hal_data->eoffset_limited), inst->comp_id, PFMT("motion.eoffset-limited")));
    CALL_CHECK(gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(inst->hal_data->eoffset_active), inst->comp_id, PFMT("motion.eoffset-active")));

    /* Done! */
    gomc_log_infof(log, inst->name, "MOTION: init_hal_io() complete, %d axes.\n", n);
    return 0;
}

static int export_spindle(motmod_inst_t *inst, int num, spindle_hal_t * addr){
	int retval;
    const gomc_hal_t *hal = inst->hal;
    (void)hal;

    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IO, (gomc_hal_bit_t **)&(addr->spindle_index_enable), inst->comp_id, PFMT("spindle.%d.index-enable"), num)) != 0) return retval;

    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->spindle_on), inst->comp_id, PFMT("spindle.%d.on"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->spindle_forward), inst->comp_id, PFMT("spindle.%d.forward"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->spindle_reverse), inst->comp_id, PFMT("spindle.%d.reverse"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->spindle_brake), inst->comp_id, PFMT("spindle.%d.brake"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->spindle_speed_out), inst->comp_id, PFMT("spindle.%d.speed-out"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->spindle_speed_out_abs), inst->comp_id, PFMT("spindle.%d.speed-out-abs"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->spindle_speed_out_rps), inst->comp_id, PFMT("spindle.%d.speed-out-rps"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->spindle_speed_out_rps_abs), inst->comp_id, PFMT("spindle.%d.speed-out-rps-abs"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->spindle_speed_cmd_rps), inst->comp_id, PFMT("spindle.%d.speed-cmd-rps"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(addr->spindle_inhibit), inst->comp_id, PFMT("spindle.%d.inhibit"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(addr->spindle_amp_fault), inst->comp_id, PFMT("spindle.%d.amp-fault-in"), num)) != 0) return retval;
    *(addr->spindle_inhibit) = 0;

    // spindle orient pins
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->spindle_orient_angle), inst->comp_id, PFMT("spindle.%d.orient-angle"), num)) < 0) return retval;
    if ((retval = gomc_hal_pin_s32_newf(hal, GOMC_HAL_OUT, &(addr->spindle_orient_mode), inst->comp_id, PFMT("spindle.%d.orient-mode"), num)) < 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->spindle_orient), inst->comp_id, PFMT("spindle.%d.orient"), num)) < 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->spindle_locked), inst->comp_id, PFMT("spindle.%d.locked"), num)) < 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(addr->spindle_is_oriented), inst->comp_id, PFMT("spindle.%d.is-oriented"), num)) < 0) return retval;
    if ((retval = gomc_hal_pin_s32_newf(hal, GOMC_HAL_IN, &(addr->spindle_orient_fault), inst->comp_id, PFMT("spindle.%d.orient-fault"), num)) < 0) return retval;
    *(addr->spindle_orient_angle) = 0.0;
    *(addr->spindle_orient_mode) = 0;
    *(addr->spindle_orient) = 0;

    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &(addr->spindle_revs), inst->comp_id, PFMT("spindle.%d.revs"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &(addr->spindle_speed_in), inst->comp_id, PFMT("spindle.%d.speed-in"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(addr->spindle_is_atspeed), inst->comp_id, PFMT("spindle.%d.at-speed"), num)) != 0) return retval;
    *(addr->spindle_is_atspeed) = 1;
    return 0;
}

static int export_joint(motmod_inst_t *inst, int num, joint_hal_t * addr)
{
    int retval;
    const gomc_hal_t *hal = inst->hal;
    const gomc_log_t *log = inst->log;
    (void)log;

    /* export joint pins */
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->coarse_pos_cmd), inst->comp_id, PFMT("joint.%d.coarse-pos-cmd"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->joint_pos_cmd), inst->comp_id, PFMT("joint.%d.pos-cmd"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->joint_pos_fb), inst->comp_id, PFMT("joint.%d.pos-fb"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->motor_pos_cmd), inst->comp_id, PFMT("joint.%d.motor-pos-cmd"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &(addr->motor_pos_fb), inst->comp_id, PFMT("joint.%d.motor-pos-fb"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->motor_offset), inst->comp_id, PFMT("joint.%d.motor-offset"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(addr->pos_lim_sw), inst->comp_id, PFMT("joint.%d.pos-lim-sw-in"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(addr->neg_lim_sw), inst->comp_id, PFMT("joint.%d.neg-lim-sw-in"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->amp_enable), inst->comp_id, PFMT("joint.%d.amp-enable-out"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(addr->amp_fault), inst->comp_id, PFMT("joint.%d.amp-fault-in"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_s32_newf(hal, GOMC_HAL_IN,   &(addr->jjog_counts), inst->comp_id, PFMT("joint.%d.jog-counts"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(addr->jjog_enable), inst->comp_id, PFMT("joint.%d.jog-enable"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &(addr->jjog_scale), inst->comp_id, PFMT("joint.%d.jog-scale"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(addr->jjog_vel_mode), inst->comp_id, PFMT("joint.%d.jog-vel-mode"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->joint_vel_cmd), inst->comp_id, PFMT("joint.%d.vel-cmd"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->joint_acc_cmd), inst->comp_id, PFMT("joint.%d.acc-cmd"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->backlash_corr), inst->comp_id, PFMT("joint.%d.backlash-corr"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->backlash_filt), inst->comp_id, PFMT("joint.%d.backlash-filt"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->backlash_vel), inst->comp_id, PFMT("joint.%d.backlash-vel"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->f_error), inst->comp_id, PFMT("joint.%d.f-error"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->f_error_lim), inst->comp_id, PFMT("joint.%d.f-error-lim"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->free_pos_cmd), inst->comp_id, PFMT("joint.%d.free-pos-cmd"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &(addr->free_vel_lim), inst->comp_id, PFMT("joint.%d.free-vel-lim"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->free_tp_enable), inst->comp_id, PFMT("joint.%d.free-tp-enable"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->kb_jjog_active), inst->comp_id, PFMT("joint.%d.kb-jog-active"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->wheel_jjog_active), inst->comp_id, PFMT("joint.%d.wheel-jog-active"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->in_position), inst->comp_id, PFMT("joint.%d.in-position"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->phl), inst->comp_id, PFMT("joint.%d.pos-hard-limit"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->nhl), inst->comp_id, PFMT("joint.%d.neg-hard-limit"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->active), inst->comp_id, PFMT("joint.%d.active"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->error), inst->comp_id, PFMT("joint.%d.error"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->f_errored), inst->comp_id, PFMT("joint.%d.f-errored"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->faulted), inst->comp_id, PFMT("joint.%d.faulted"), num)) != 0) return retval;
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_IN,&(addr->jjog_accel_fraction),inst->comp_id,PFMT("joint.%d.jog-accel-fraction"), num)) != 0) return retval;
    *addr->jjog_accel_fraction = 1.0; // fraction of accel for wheel jjogs

    if ( joint_is_lockable(inst, num) ) {
        // these pins may be needed for rotary joints
        gomc_log_warnf(log, inst->name, "motion.c: Creating unlock hal pins for joint %d\n",num);
        if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, (gomc_hal_bit_t **)&(addr->unlock), inst->comp_id, PFMT("joint.%d.unlock"), num)) != 0) return retval;
        if ((retval = gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, (gomc_hal_bit_t **)&(addr->is_unlocked), inst->comp_id, PFMT("joint.%d.is-unlocked"), num)) != 0) return retval;
    }

    return 0;
}

static int export_extrajoint(motmod_inst_t *inst, int num, extrajoint_hal_t * addr)
{
    int retval;
    const gomc_hal_t *hal = inst->hal;
    /* export extrajoint pins */
    if ((retval = gomc_hal_pin_float_newf(hal, GOMC_HAL_IN,  &(addr->posthome_cmd),  inst->comp_id,
                                            "joint.%d.posthome-cmd",  num)) != 0) return retval;
    return 0;
}

/* init_comm_buffers() allocates and initializes the command,
   status, and error buffers used to communicate with the user
   space parts of emc.
*/
static int init_comm_buffers(motmod_inst_t *inst)
{
    int joint_num, spindle_num, n;
    emcmot_joint_t *joint;
    const gomc_log_t *log = inst->log;

    gomc_log_infof(log, inst->name, "MOTION: init_comm_buffers() starting...\n");

    inst->mot_struct = 0;
    inst->internal = 0;
    inst->status = 0;
    inst->command = 0;
    inst->config = 0;

    /* allocate the motion structure (direct memory, no shmem key) */
    inst->mot_struct = rtapi_calloc(sizeof(emcmot_struct_t));
    if (!inst->mot_struct) {
	gomc_log_errorf(log, inst->name, "MOTION: rtapi_calloc failed for emcmot_struct_t\n");
	return -1;
    }

    /* Initialize triple buffer: each index must be distinct. */
    inst->mot_struct->status_buf.write_idx = 0;
    atomic_init(&inst->mot_struct->status_buf.middle, 1);
    inst->mot_struct->status_buf.read_idx = 2;
    /* reader_mtx is zero-initialized by calloc = unlocked */

    /* we'll reference inst->mot_struct directly */
    inst->command = &inst->mot_struct->command;
    inst->status = &inst->mot_struct->status;
    inst->config = &inst->mot_struct->config;
    inst->internal = &inst->mot_struct->internal;

    /* init command struct */
    inst->command->command = 0;
    inst->command->commandNum = 0;

    /* init status struct */
    inst->status->head = 0;
    inst->status->commandEcho = 0;
    inst->status->commandNumEcho = 0;
    inst->status->commandStatus = 0;

    /* init more stuff */
    inst->internal->head = 0;
    inst->config->head = 0;

    inst->status->motionFlag = 0;
    SET_MOTION_ERROR_FLAG(0);
    SET_MOTION_COORD_FLAG(0);
    SET_MOTION_TELEOP_FLAG(0);
    inst->internal->split = 0;
    inst->status->heartbeat = 0;

    ALL_JOINTS                   = inst->num_joints;      // inst->config->numJoints from [KINS]JOINTS
    inst->config->numExtraJoints = inst->num_extrajoints; // from motmod inst->num_extrajoints=
    inst->status->numExtraJoints = inst->num_extrajoints;

    inst->config->numSpindles = inst->num_spindles;
    inst->config->numDIO = inst->num_dio;
    inst->config->numAIO = inst->num_aio;
    inst->config->numMiscError = inst->num_misc_error;

    ZERO_EMC_POSE(inst->status->carte_pos_cmd);
    ZERO_EMC_POSE(inst->status->carte_pos_fb);
    inst->status->vel = 0.0;
    inst->config->limitVel = 0.0;
    inst->status->acc = 0.0;
    inst->status->feed_scale = 1.0;
    inst->status->rapid_scale = 1.0;
    inst->status->net_feed_scale = 1.0;
    /* adaptive feed is off by default, feed override, spindle
       override, and feed hold are on */
    inst->status->enables_new = FS_ENABLED | SS_ENABLED | FH_ENABLED;
    inst->status->enables_queued = inst->status->enables_new;
    inst->status->id = 0;
    inst->status->depth = 0;
    inst->status->activeDepth = 0;
    inst->status->paused = 0;
    inst->status->overrideLimitMask = 0;
    SET_MOTION_INPOS_FLAG(1);
    SET_MOTION_ENABLE_FLAG(0);
    /* record the kinematics type of the machine */
    inst->config->kinType = motmod_kinematicsType(inst);
    emcmot_config_change(inst);

    for (spindle_num = 0; spindle_num < EMCMOT_MAX_SPINDLES; spindle_num++){
        inst->status->spindle_status[spindle_num].scale = 1.0;
        inst->status->spindle_status[spindle_num].speed = 0.0;
    }

    axis_init_all((axis_inst_t *)inst->axis_inst);

    /* init per-joint stuff */
    for (joint_num = 0; joint_num < ALL_JOINTS; joint_num++) {
	/* point to structure for this joint */
	joint = &inst->joints[joint_num];

	/* init the config fields with some "reasonable" defaults" */
	joint->type = 0;
	joint->max_pos_limit = 1.0;
	joint->min_pos_limit = -1.0;
	joint->vel_limit = 1.0;
	joint->acc_limit = 1.0;
	joint->jerk_limit = 0.0;	/* disabled by default */
	joint->min_ferror = 0.01;
	joint->max_ferror = 1.0;
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
	for ( n = 1 ; n < EMCMOT_COMP_SIZE+2 ; n++ ) {
	    joint->comp.array[n].nominal = DBL_MAX;
	    joint->comp.array[n].fwd_trim = 0.0;
	    joint->comp.array[n].rev_trim = 0.0;
	    joint->comp.array[n].fwd_slope = 0.0;
	    joint->comp.array[n].rev_slope = 0.0;
	}

	/* init joint flags */
	joint->flag = 0;
	SET_JOINT_INPOS_FLAG(joint, 1);

	/* init status info */
	joint->coarse_pos = 0.0;
	joint->pos_cmd = 0.0;
	joint->vel_cmd = 0.0;
	joint->acc_cmd = 0.0;
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
    }

    inst->status->tail = 0;

    gomc_log_infof(log, inst->name, "MOTION: init_comm_buffers() complete\n");
    return 0;
}

/* export_functions() exports the realtime functions that implement
   the motion controller. Thread creation is handled externally by
   the launcher (which loads the threads component); motmod only
   exports its functions so they can be added to threads via addf
   (e.g., `addf motion-command-handler servo-thread` and
   `addf motion-controller servo-thread`).
*/
static int export_functions(motmod_inst_t *inst)
{
    double base_period_sec, servo_period_sec;
    int servo_base_ratio;
    int retval;
    const gomc_hal_t *hal = inst->hal;
    const gomc_log_t *log = inst->log;

    gomc_log_infof(log, inst->name, "MOTION: export_functions() starting...\n");

    /* if base_period not specified, assume same as servo_period */
    if (inst->base_period_nsec == 0) {
	inst->base_period_nsec = inst->servo_period_nsec;
    }
    if (inst->traj_period_nsec == 0) {
	inst->traj_period_nsec = inst->servo_period_nsec;
    }
    /* servo period must be greater or equal to base period */
    if (inst->servo_period_nsec < inst->base_period_nsec) {
	gomc_log_errorf(log, inst->name, "MOTION: bad servo period %ld nsec\n", inst->servo_period_nsec);
	return -1;
    }
    /* convert desired periods to floating point */
    base_period_sec = inst->base_period_nsec * 0.000000001;
    servo_period_sec = inst->servo_period_nsec * 0.000000001;
    /* calculate period ratios, round to nearest integer */
    servo_base_ratio = (servo_period_sec / base_period_sec) + 0.5;
    /* revise desired periods to be integer multiples of each other */
    inst->servo_period_nsec = inst->base_period_nsec * servo_base_ratio;
    /* export realtime functions that do the real work */
    char fname[HAL_NAME_LEN + HAL_NAME_LEN];
    snprintf(fname, sizeof(fname), "%smotion-controller", inst->pin_prefix);
    retval = hal->export_funct(hal->ctx, fname, emcmotController, inst
	 /* arg */ , 1 /* uses_fp */ , 0 /* reentrant */ , inst->comp_id);
    if (retval < 0) {
	gomc_log_errorf(log, inst->name, "MOTION: failed to export controller function\n");
	return -1;
    }
    snprintf(fname, sizeof(fname), "%smotion-command-handler", inst->pin_prefix);
    retval = hal->export_funct(hal->ctx, fname, emcmotCommandHandler, inst
	 /* arg */ , 1 /* uses_fp */ , 0 /* reentrant */ , inst->comp_id);
    if (retval < 0) {
	gomc_log_errorf(log, inst->name, "MOTION: failed to export command handler function\n");
	return -1;
    }
/*! \todo Another #if 0 */
#if 0
    /*! \todo FIXME - currently the traj planner is called from the controller */
    /* eventually it will be a separate function */
    retval = hal->export_funct(hal->ctx, "motion-traj-planner", emcmotTrajPlanner, inst
	 /* arg */ , 1 /* uses_fp */ ,
	0 /* reentrant */ , inst->comp_id);
    if (retval < 0) {
	gomc_log_errorf(log, inst->name, "MOTION: failed to export traj planner function\n");
	return -1;
    }
#endif

    // if we don't set cycle times based on these guesses, emc doesn't
    // start up right
    setServoCycleTime(inst, inst->servo_period_nsec * 1e-9);
    setTrajCycleTime(inst, inst->traj_period_nsec * 1e-9);

    gomc_log_infof(log, inst->name, "MOTION: export_functions() complete\n");
    return 0;
}

void emcmotSetCycleTime(motmod_inst_t *inst, unsigned long nsec)
{
    int servo_mult;
    servo_mult = inst->traj_period_nsec / nsec;
    if(servo_mult < 0) servo_mult = 1;
    setTrajCycleTime(inst, nsec * 1e-9);
    setServoCycleTime(inst, nsec * servo_mult * 1e-9);
}

/* call this when setting the trajectory cycle time */
static int setTrajCycleTime(motmod_inst_t *inst, double secs)
{
    int t;
    const gomc_log_t *log = inst->log;

    gomc_log_infof(log, inst->name, "MOTION: setting Traj cycle time to %ld nsecs\n", (long) (secs * 1e9));

    /* make sure it's not zero */
    if (secs <= 0.0) {
	return -1;
    }

    emcmot_config_change(inst);

    /* compute the interpolation rate as nearest integer to traj/servo */
    if(inst->config->servoCycleTime)
        inst->config->interpolationRate =
            (int) (secs / inst->config->servoCycleTime + 0.5);
    else
        inst->config->interpolationRate = 1;

    /* set traj planner */
    inst->tp_api->set_cycle_time(inst->tp_api->ctx, secs);

    /* set the free planners, cubic interpolation rate and segment time */
    for (t = 0; t < ALL_JOINTS; t++) {
	cubicSetInterpolationRate(&(inst->joints[t].cubic),
	    inst->config->interpolationRate);
    }

    /* copy into status out */
    inst->config->trajCycleTime = secs;

    return 0;
}

/* call this when setting the servo cycle time */
static int setServoCycleTime(motmod_inst_t *inst, double secs)
{
    int t;
    const gomc_log_t *log = inst->log;

    gomc_log_infof(log, inst->name, "MOTION: setting Servo cycle time to %ld nsecs\n", (long) (secs * 1e9));

    /* make sure it's not zero */
    if (secs <= 0.0) {
	return -1;
    }

    emcmot_config_change(inst);

    /* compute the interpolation rate as nearest integer to traj/servo */
    inst->config->interpolationRate =
	(int) (inst->config->trajCycleTime / secs + 0.5);

    /* set the cubic interpolation rate and PID cycle time */
    for (t = 0; t < ALL_JOINTS; t++) {
	cubicSetInterpolationRate(&(inst->joints[t].cubic),
	    inst->config->interpolationRate);
	cubicSetSegmentTime(&(inst->joints[t].cubic), secs);
    }

    /* copy into status out */
    inst->config->servoCycleTime = secs;

    return 0;
}
