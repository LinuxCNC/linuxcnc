/********************************************************************
* Description: rotatekins.c
*   Simple example kinematics for a rotary table in software
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author: Chris Radek
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2006 All rights reserved.
*
********************************************************************/

#include "kinematics.h"		/* these decls */

#ifdef RTAPI
#include "rtapi_math.h"
#else
#include <math.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * Rotatekins - simple rotary table kinematics
 *
 * The C axis (joint 5) rotates the XY plane.
 *
 * Forward: Rotate (J0, J1) by -C to get (X, Y)
 * Inverse: Rotate (X, Y) by +C to get (J0, J1)
 */
static int rotate_forward_math(const double *joints, EmcPose *world)
{
    double c_rad = -joints[5] * M_PI / 180.0;

    world->tran.x = joints[0] * cos(c_rad) - joints[1] * sin(c_rad);
    world->tran.y = joints[0] * sin(c_rad) + joints[1] * cos(c_rad);
    world->tran.z = joints[2];
    world->a = joints[3];
    world->b = joints[4];
    world->c = joints[5];
    world->u = joints[6];
    world->v = joints[7];
    world->w = joints[8];

    return 0;
}

static int rotate_inverse_math(const EmcPose *world, double *joints)
{
    double c_rad = world->c * M_PI / 180.0;

    joints[0] = world->tran.x * cos(c_rad) - world->tran.y * sin(c_rad);
    joints[1] = world->tran.x * sin(c_rad) + world->tran.y * cos(c_rad);
    joints[2] = world->tran.z;
    joints[3] = world->a;
    joints[4] = world->b;
    joints[5] = world->c;
    joints[6] = world->u;
    joints[7] = world->v;
    joints[8] = world->w;

    return 0;
}

int kinematicsForward(const double *joints,
		      EmcPose * pos,
		      const KINEMATICS_FORWARD_FLAGS * fflags,
		      KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    (void)iflags;
    return rotate_forward_math(joints, pos);
}

int kinematicsInverse(const EmcPose * pos,
		      double *joints,
		      const KINEMATICS_INVERSE_FLAGS * iflags,
		      KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)iflags;
    (void)fflags;
    return rotate_inverse_math(pos, joints);
}

/* implemented for these kinematics as giving joints preference */
int kinematicsHome(EmcPose * world,
		   double *joint,
		   KINEMATICS_FORWARD_FLAGS * fflags,
		   KINEMATICS_INVERSE_FLAGS * iflags)
{
    *fflags = 0;
    *iflags = 0;

    return kinematicsForward(joint, world, fflags, iflags);
}

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"
#include "hal_priv.h"
#include "kinematics_params.h"
#include <string.h>

const char* kinematicsGetName(void) { return "rotatekins"; }

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsGetName);
MODULE_LICENSE("GPL");

static kinematics_params_t *uspace_params;

int comp_id;
int rtapi_app_main(void) {
    comp_id = hal_init("rotatekins");
    if(comp_id < 0) return comp_id;

    uspace_params = (kinematics_params_t *)hal_malloc(sizeof(kinematics_params_t));
    if (!uspace_params) { hal_exit(comp_id); return -1; }
    if (hal_param_s32_newf(HAL_RO, &uspace_params->self_offset, comp_id,
                         "rotatekins.uspace-params-offset") < 0) {
        hal_exit(comp_id); return -1;
    }
    memset(uspace_params, 0, sizeof(*uspace_params));
    uspace_params->num_joints = 6;
    uspace_params->valid       = 1;
    uspace_params->is_identity = 0;
    uspace_params->head = 1;
    uspace_params->tail = 1;
    uspace_params->self_offset = (int)SHMOFF(uspace_params);

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }

/* ========================================================================
 * Non-RT interface for userspace trajectory planner
 * ======================================================================== */

void nonrt_attach(char *shmem_base, int offset, nonrt_ops_t *ops)
{
    (void)shmem_base; (void)offset;
    ops->forward = kinematicsForward;
    ops->inverse = kinematicsInverse;
}

EXPORT_SYMBOL(nonrt_attach);
