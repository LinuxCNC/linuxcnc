/********************************************************************
* Description: gentrivkins.c
*   general trivkins for 3 axis Cartesian machine, based on gantrykins
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* License: GPL Version 2
*    
* Copyright (c) 2009 All rights reserved.
*
********************************************************************/

#include "motion.h"
#include "hal.h"
#include "rtapi.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "rtapi_math.h"
#include "rtapi_string.h"

struct data { 
    hal_s32_t joints[EMCMOT_MAX_JOINTS];
} *data;

#define SET(f) pos->f = joints[i]

int kinematicsForward(const double *joints,
		      EmcPose * pos,
		      const KINEMATICS_FORWARD_FLAGS * fflags,
		      KINEMATICS_INVERSE_FLAGS * iflags)
{
    int i;

    for(i = 0; i < EMCMOT_MAX_JOINTS; i++) {
        switch(data->joints[i]) {
            case 0: SET(tran.x); break;
            case 1: SET(tran.y); break;
            case 2: SET(tran.z); break;
            case 3: SET(a); break;
            case 4: SET(b); break;
            case 5: SET(c); break;
            case 6: SET(u); break;
            case 7: SET(v); break;
            case 8: SET(w); break;
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
    for(i = 0; i < EMCMOT_MAX_JOINTS; i++) {
        switch(data->joints[i]) {
            case 0: joints[i] = pos->tran.x; break;
            case 1: joints[i] = pos->tran.y; break;
            case 2: joints[i] = pos->tran.z; break;
            case 3: joints[i] = pos->a; break;
            case 4: joints[i] = pos->b; break;
            case 5: joints[i] = pos->c; break;
            case 6: joints[i] = pos->u; break;
            case 7: joints[i] = pos->v; break;
            case 8: joints[i] = pos->w; break;
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
    return KINEMATICS_IDENTITY;
}

char *coordinates = "XYZABCUVW";
RTAPI_MP_STRING(coordinates, "Existing Axes");
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
MODULE_LICENSE("GPL");

static int next_axis_number(void) {
    while(*coordinates) {
	switch(*coordinates) {
	    case 'x': case 'X': coordinates++; return 0;
	    case 'y': case 'Y': coordinates++; return 1;
	    case 'z': case 'Z': coordinates++; return 2;
	    case 'a': case 'A': coordinates++; return 3;
	    case 'b': case 'B': coordinates++; return 4;
	    case 'c': case 'C': coordinates++; return 5;
	    case 'u': case 'U': coordinates++; return 6;
	    case 'v': case 'V': coordinates++; return 7;
	    case 'w': case 'W': coordinates++; return 8;
	    case ' ': case '\t': coordinates++; continue;
	}
	rtapi_print_msg(RTAPI_MSG_ERR,
		"GENTRIVKINS: ERROR: Invalid character '%c' in coordinates",
		*coordinates);
    }
    return -1;
}

int comp_id;
int rtapi_app_main(void) {
    int i;
    comp_id = hal_init("gentrivkins");
    if(comp_id < 0) return comp_id;

    data = hal_malloc(sizeof(struct data));

    for(i=0; i<EMCMOT_MAX_JOINTS; i++) {
	data->joints[i] = next_axis_number();
    }

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
