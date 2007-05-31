/********************************************************************
* Description: gantrykins.c
*   Gantry (ganged joint) kinematics for 3 axis Cartesian machine
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* License: GPL Version 2
*    
* Copyright (c) 2006 All rights reserved.
*
********************************************************************/

#include "motion.h"		/* these decls */
#include "hal.h"
#include "rtapi.h"
#include "rtapi_math.h"
#include "rtapi_string.h"

struct data { 
    hal_s32_t joints[8];
/* hal_u8_t joints[8]; */
} *data;

#define SET(f) pos->f = joints[i]

int kinematicsForward(const double *joints,
		      EmcPose * pos,
		      const KINEMATICS_FORWARD_FLAGS * fflags,
		      KINEMATICS_INVERSE_FLAGS * iflags)
{
    int i;

    for(i=0; i<8; i++) {
        switch(data->joints[i]) {
            case 0: SET(tran.x); break;
            case 1: SET(tran.y); break;
            case 2: SET(tran.z); break;
            case 3: SET(a); break;
            case 4: SET(b); break;
            case 5: SET(c); break;
        }
    }

    return 0;
}

int kinematicsInverse(const EmcPose * pos,
		      double *joints,
		      const KINEMATICS_INVERSE_FLAGS * iflags,
		      KINEMATICS_FORWARD_FLAGS * fflags)
{
    int i;
    for(i=0; i<8; i++) {
        switch(data->joints[i]) {
            case 0: joints[i] = pos->tran.x; break;
            case 1: joints[i] = pos->tran.y; break;
            case 2: joints[i] = pos->tran.z; break;
            case 3: joints[i] = pos->a; break;
            case 4: joints[i] = pos->b; break;
            case 5: joints[i] = pos->c; break;
        }
    }

    return 0;
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

#ifdef RTAPI
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"

EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
MODULE_LICENSE("GPL");

int comp_id;
int rtapi_app_main(void) {
    int result, i;
    comp_id = hal_init("gantrykins");
    if(comp_id < 0) return comp_id;

    data = hal_malloc(sizeof(struct data));

    for(i=0; i<8; i++) {
        result = hal_param_s32_newf(HAL_RW, &(data->joints[i]), comp_id,
                "gantrykins.joint-%d", i);
        if(result < 0) goto error;
        if(i < 6)
            data->joints[i] = i;
        else
            data->joints[i] = -1;
    }

    hal_ready(comp_id);
    return 0;

error:
    hal_exit(comp_id);
    return result;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
#endif
