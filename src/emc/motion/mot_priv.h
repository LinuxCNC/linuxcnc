/*******************************************************************
* Description: mot_priv.h
*   Private header for motmod.so — defines motmod_inst_t and all
*   multi-instance declarations.  Only included by motmod sources
*   (motion.c, command.c, control.c, axis.c).
*
* License: GPL Version 2
********************************************************************/
#ifndef MOT_PRIV_H
#define MOT_PRIV_H

#include "mot_hal_types.h"
#include "gomc_hal.h"
#include "gomc_log.h"
#include "gomc_rtapi.h"
#include "kins_api.h"
#include "tp_api.h"
#include "home_api.h"

/* Helper to get per-joint home_api from joint struct */
#define JOINT_HOME_API(joint) ((const home_callbacks_t *)(joint)->home_api)

/***********************************************************************
*                   INSTANCE STRUCT (multi-instance support)            *
************************************************************************/

/*
 * motmod_inst_t — all per-instance state for the motion controller.
 * Each New() call allocates one of these and stores it in cmod->priv.
 */
typedef struct motmod_inst {
    /* identity / environment */
    const void *env;           /* cmod_env_t*, only used in motion.c */
    const char *name;
    const gomc_hal_t *hal;
    const gomc_log_t *log;
    const gomc_rtapi_t *rtapi;
    char pin_prefix[HAL_NAME_LEN]; /* "name." when aliased, "" when default */
    char kins_inst_name[HAL_NAME_LEN];
    char tp_inst_name[HAL_NAME_LEN];
    char home_inst_prefix[HAL_NAME_LEN]; /* prefix for per-joint homemod lookup */
    int comp_id;

    /* cross-module API pointers */
    const kins_callbacks_t *kins;
    const tp_callbacks_t *tp_api;

    /* homing sequence coordination (replicated from original homing.c FSM) */
    enum {
        HOME_SEQUENCE_IDLE = 0,
        HOME_SEQUENCE_START,
        HOME_SEQUENCE_DO_ONE_JOINT,
        HOME_SEQUENCE_DO_ONE_SEQUENCE,
        HOME_SEQUENCE_START_JOINTS,
        HOME_SEQUENCE_WAIT_JOINTS,
    } sequence_state;
    int current_sequence;      /* which sequence number is currently active */
    int homing_active;         /* 1 if any joint is homing */
    int all_homed;             /* 1 if all joints homed */
    int joint_in_sequence[EMCMOT_MAX_JOINTS]; /* per-joint participation flag */

    /* HAL data */
    emcmot_hal_data_t *hal_data;

    /* core motion data */
    emcmot_joint_t joints[EMCMOT_MAX_JOINTS];
    struct emcmot_struct_t *mot_struct;
    struct emcmot_command_t *command;
    struct emcmot_status_t *status;
    struct emcmot_config_t *config;
    struct emcmot_internal_t *internal;

    /* kinematics flags */
    KINEMATICS_FORWARD_FLAGS fflags;
    KINEMATICS_INVERSE_FLAGS iflags;

    /* config (parsed from argv) */
    int num_joints;
    int num_extrajoints;
    int num_spindles;
    int num_dio;
    int num_aio;
    int num_misc_error;
    int unlock_joints_mask;
    int unlock_msg_given;       /* per-joint one-shot error flag */
    long base_period_nsec;
    int base_thread_fp;
    long servo_period_nsec;
    long traj_period_nsec;
#define MOTMOD_MAX_IO 64
    char *names_din[MOTMOD_MAX_IO];
    char *names_dout[MOTMOD_MAX_IO];
    char *names_ain[MOTMOD_MAX_IO];
    char *names_aout[MOTMOD_MAX_IO];
    char *names_misc_errors[MOTMOD_MAX_IO];

    /* control.c runtime state */
    int ext_offset_teleop_limit;
    int ext_offset_coord_limit;
    int coord_cubic_active;
    int ctl_switchkins_type;
    long ctl_last_period;
    double ctl_servo_period;
    double *pcmd_p[EMCMOT_MAX_AXIS];
    long long last_clocks;

    /* command.c runtime state */
    int rehomeAll;

    /* control.c per-instance state (formerly static locals) */
    int ctl_first_pass;
    int ctl_old_motion_index[EMCMOT_MAX_SPINDLES];
    int ctl_old_hal_index[EMCMOT_MAX_SPINDLES];
    int ctl_old_joint_flags[8];
    int ctl_old_motion_flag;
    int ctl_old_probeVal;

    /* Per-instance motctl/motstat handler contexts and callbacks (opaque) */
    void *motctl_ctx;
    void *motstat_ctx;
    void *motctl_cb;
    void *motstat_cb;
    void *mot_cb;  /* per-instance mot_callbacks_t* (freed in Destroy) */

    /* Per-instance axis state (opaque, axis_inst_t* from axis.c) */
    void *axis_inst;

    /* Jerk filter (boxcar) state for coordinated/teleop motion */
    #define JERK_FILTER_MAX_WINDOW 64
    struct {
        double *buf;       /* [num_joints * window_size] ring buffer (row-major) */
        double *sum;       /* [num_joints] running sums */
        int num_joints;    /* allocated joint count */
        int idx;
        int filled;
        int window_size;   /* 0 = filter disabled */
    } jerk_filter;
} motmod_inst_t;

/***********************************************************************
*                    FUNCTION PROTOTYPES                               *
************************************************************************/

extern void emcmotCommandHandler(void *arg, long period);
extern void emcmotController(void *arg, long period);
extern void emcmotSetCycleTime(motmod_inst_t *inst, unsigned long nsec);

/* kinematics wrappers — take inst explicitly (no globals) */
extern int motmod_kinematicsForward(motmod_inst_t *inst, const double *joint,
                             struct EmcPose *world,
                             const KINEMATICS_FORWARD_FLAGS *fflags,
                             KINEMATICS_INVERSE_FLAGS *iflags);
extern int motmod_kinematicsInverse(motmod_inst_t *inst, const struct EmcPose *world,
                             double *joint,
                             const KINEMATICS_INVERSE_FLAGS *iflags,
                             KINEMATICS_FORWARD_FLAGS *fflags);
extern KINEMATICS_TYPE motmod_kinematicsType(motmod_inst_t *inst);
extern int motmod_kinematicsSwitchable(motmod_inst_t *inst);
extern int motmod_kinematicsSwitch(motmod_inst_t *inst, int switchkins_type);

/* synchronized I/O */
extern void emcmotDioWrite(motmod_inst_t *inst, int index, char value);
extern void emcmotAioWrite(motmod_inst_t *inst, int index, double value);

extern void emcmotSetRotaryUnlock(motmod_inst_t *inst, int axis, int unlock);
extern int emcmotGetRotaryIsUnlocked(motmod_inst_t *inst, int axis);

void switch_to_teleop_mode(motmod_inst_t *inst);

extern void refresh_jog_limits(motmod_inst_t *inst, emcmot_joint_t *joint, int joint_num);
extern void jerk_filter_recompute_window(motmod_inst_t *inst);
extern void clearHomes(motmod_inst_t *inst, int joint_num);
extern void emcmot_config_change(motmod_inst_t *inst);
int joint_is_lockable(motmod_inst_t *inst, int joint_num);

/***********************************************************************
*                    CONVENIENCE MACROS                                *
************************************************************************/

#define ALL_JOINTS inst->config->numJoints
// number of kinematics-only joints:
#define NO_OF_KINS_JOINTS (ALL_JOINTS - inst->config->numExtraJoints)
#define IS_EXTRA_JOINT(jno) (jno >= NO_OF_KINS_JOINTS)

 /* rtapi_get_time() returns a nanosecond value. */
#define etime() (((double) inst->rtapi->get_time(inst->rtapi->ctx)) / 1.0e9)

/* motion flags */
#define GET_MOTION_ERROR_FLAG() (inst->status->motionFlag & EMCMOT_MOTION_ERROR_BIT ? 1 : 0)
#define SET_MOTION_ERROR_FLAG(fl) if (fl) inst->status->motionFlag |= EMCMOT_MOTION_ERROR_BIT; else inst->status->motionFlag &= ~EMCMOT_MOTION_ERROR_BIT;

#define GET_MOTION_COORD_FLAG() (inst->status->motionFlag & EMCMOT_MOTION_COORD_BIT ? 1 : 0)
#define SET_MOTION_COORD_FLAG(fl) if (fl) inst->status->motionFlag |= EMCMOT_MOTION_COORD_BIT; else inst->status->motionFlag &= ~EMCMOT_MOTION_COORD_BIT;

#define GET_MOTION_TELEOP_FLAG() (inst->status->motionFlag & EMCMOT_MOTION_TELEOP_BIT ? 1 : 0)
#define SET_MOTION_TELEOP_FLAG(fl) if (fl) inst->status->motionFlag |= EMCMOT_MOTION_TELEOP_BIT; else inst->status->motionFlag &= ~EMCMOT_MOTION_TELEOP_BIT;

#define GET_MOTION_INPOS_FLAG() (inst->status->motionFlag & EMCMOT_MOTION_INPOS_BIT ? 1 : 0)
#define SET_MOTION_INPOS_FLAG(fl) if (fl) inst->status->motionFlag |= EMCMOT_MOTION_INPOS_BIT; else inst->status->motionFlag &= ~EMCMOT_MOTION_INPOS_BIT;

#define GET_MOTION_ENABLE_FLAG() (inst->status->motionFlag & EMCMOT_MOTION_ENABLE_BIT ? 1 : 0)
#define SET_MOTION_ENABLE_FLAG(fl) if (fl) inst->status->motionFlag |= EMCMOT_MOTION_ENABLE_BIT; else inst->status->motionFlag &= ~EMCMOT_MOTION_ENABLE_BIT;

#endif /* MOT_PRIV_H */
