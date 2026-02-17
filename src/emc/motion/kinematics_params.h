/********************************************************************
 * Description: kinematics_params.h
 *   Shared memory structure for kinematics parameters
 *   Allows userspace to read kinematics module parameters from RT
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#ifndef KINEMATICS_PARAMS_H
#define KINEMATICS_PARAMS_H

#include "emcmotcfg.h"  /* EMCMOT_MAX_JOINTS */

/* Parameters for 5axiskins */
typedef struct {
    double pivot_length;
} kins_5axis_params_t;

/* Parameters for xyzac-trt-kins and xyzbc-trt-kins */
typedef struct {
    double x_rot_point;
    double y_rot_point;
    double z_rot_point;
    double x_offset;
    double y_offset;
    double z_offset;
    double tool_offset;
    int conventional_directions;
} kins_trt_params_t;

/* Parameters for maxkins */
typedef struct {
    double pivot_length;
    int conventional_directions;
} kins_max_params_t;

/* Parameters for pumakins */
typedef struct {
    double a2;
    double a3;
    double d3;
    double d4;
    double d6;
} kins_puma_params_t;

/* Parameters for tripodkins */
typedef struct {
    double bx;
    double cx;
    double cy;
} kins_tripod_params_t;

/* Number of struts for hexapod/pentapod */
#define KINS_GENHEX_NUM_STRUTS 6
#define KINS_PENTA_NUM_STRUTS 5

/* Parameters for genhexkins */
typedef struct {
    double basex[KINS_GENHEX_NUM_STRUTS];
    double basey[KINS_GENHEX_NUM_STRUTS];
    double basez[KINS_GENHEX_NUM_STRUTS];
    double platformx[KINS_GENHEX_NUM_STRUTS];
    double platformy[KINS_GENHEX_NUM_STRUTS];
    double platformz[KINS_GENHEX_NUM_STRUTS];
    double basenx[KINS_GENHEX_NUM_STRUTS];
    double baseny[KINS_GENHEX_NUM_STRUTS];
    double basenz[KINS_GENHEX_NUM_STRUTS];
    double platformnx[KINS_GENHEX_NUM_STRUTS];
    double platformny[KINS_GENHEX_NUM_STRUTS];
    double platformnz[KINS_GENHEX_NUM_STRUTS];
    unsigned int iter_limit;
    unsigned int max_iter;
    double max_error;
    double conv_criterion;
    double tool_offset;
    double spindle_offset;
    double screw_lead;
} kins_genhex_params_t;

/* Max joints for genser DH parameters */
#define KINS_GENSER_MAX_JOINTS 9

/* Parameters for genserfuncs (generic serial robot) */
typedef struct {
    int link_num;                           /* Number of active links (up to KINS_GENSER_MAX_JOINTS) */
    unsigned int max_iterations;
    unsigned int last_iterations;           /* Iterations used in last inverse solve */
    double a[KINS_GENSER_MAX_JOINTS];
    double alpha[KINS_GENSER_MAX_JOINTS];
    double d[KINS_GENSER_MAX_JOINTS];
    int unrotate[KINS_GENSER_MAX_JOINTS];
} kins_genser_params_t;

/* Parameters for pentakins */
typedef struct {
    double basex[KINS_PENTA_NUM_STRUTS];
    double basey[KINS_PENTA_NUM_STRUTS];
    double basez[KINS_PENTA_NUM_STRUTS];
    double effectorr[KINS_PENTA_NUM_STRUTS];
    double effectorz[KINS_PENTA_NUM_STRUTS];
    unsigned int iter_limit;
    double conv_criterion;
} kins_penta_params_t;

/* Parameters for lineardeltakins */
typedef struct {
    double radius;
    double jointradius;
} kins_lineardelta_params_t;

/* Parameters for rotarydeltakins */
typedef struct {
    double platformradius;  /* distance from origin to a hip joint */
    double thighlength;     /* thigh connects the hip to the knee */
    double shinlength;      /* shin (parallelogram) connects the knee to the foot */
    double footradius;      /* distance from center of foot to an ankle joint */
} kins_rotarydelta_params_t;

/* Parameters for rosekins */
typedef struct {
    double revolutions;
    double theta_degrees;
    double bigtheta_degrees;
} kins_rose_params_t;

/* Parameters for scarakins */
typedef struct {
    double d1;  /* Vertical distance from ground to inner arm center */
    double d2;  /* Length of inner arm */
    double d3;  /* Vertical offset between inner and outer arm */
    double d4;  /* Length of outer arm */
    double d5;  /* Vertical distance from end effector to tooltip */
    double d6;  /* Horizontal offset from end effector axis to tooltip */
} kins_scara_params_t;

/* Parameters for scorbotkins */
typedef struct {
    double l0_horizontal;  /* Horizontal distance from J0 to J1 */
    double l0_vertical;    /* Vertical distance from ground to J1 */
    double l1_length;      /* Link 1: J1 (shoulder) to J2 (elbow) */
    double l2_length;      /* Link 2: J2 (elbow) to wrist */
} kins_scorbot_params_t;

/*
 * Main kinematics parameters structure for shared memory
 *
 * Thread safety: Uses split-read pattern (head/tail) like emcmot_status_t
 * RT increments head before update, sets tail=head after
 * Userspace copies struct, checks head==tail, retries if mismatch
 */
typedef struct kinematics_params_t {
    /* Split-read consistency markers */
    unsigned char head;
    unsigned char tail;

    /* Kinematics identification */
    char module_name[32];       /* e.g., "5axiskins", "xyzac-trt-kins" */
    char coordinates[16];       /* e.g., "XYZBCW", "XYZAC" */

    /* Joint configuration */
    int num_joints;
    int joint_to_axis[EMCMOT_MAX_JOINTS];  /* joint -> axis index (-1 = unmapped) */
    int axis_to_joint[9];                   /* axis (XYZABCUVW) -> principal joint (-1 = unmapped) */

    /* Kinematics-specific parameters (union) */
    union {
        kins_5axis_params_t fiveaxis;
        kins_trt_params_t trt;
        kins_max_params_t maxkins;
        kins_puma_params_t puma;
        kins_tripod_params_t tripod;
        kins_genhex_params_t genhex;
        kins_genser_params_t genser;
        kins_penta_params_t penta;
        kins_lineardelta_params_t lineardelta;
        kins_rotarydelta_params_t rotarydelta;
        kins_rose_params_t rose;
        kins_scara_params_t scara;
        kins_scorbot_params_t scorbot;
        double raw[128];  /* Reserve space for largest struct (genhex ~= 78 doubles) */
    } params;

    /* Validity flag - set to 1 when params are initialized */
    int valid;

    /* Sequence number - incremented on any parameter change */
    unsigned int seq_num;

} kinematics_params_t;

#endif /* KINEMATICS_PARAMS_H */
