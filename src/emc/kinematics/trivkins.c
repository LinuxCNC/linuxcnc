/********************************************************************
* Description: trivkins.c
*   general trivkins for 3 axis Cartesian machine
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
#include "rtapi.h"      /* RTAPI realtime OS API */
#include "rtapi_app.h"  /* RTAPI realtime module decls */
#include "rtapi_math.h"
#include "rtapi_string.h"
#include "kinematics.h"

#define ALLOW_DUPLICATES 1
static int axis_idx_for_jno[EMCMOT_MAX_JOINTS];

#define SET(f) pos->f = joints[i]

int kinematicsForward(const double *joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
{
    int i;

    //NOTE: unspecified joints use axis_idx_for_jno[i] == -1
    //      and will not change any coordinate value
    for(i = 0; i < EMCMOT_MAX_JOINTS; i++) {
        switch(axis_idx_for_jno[i]) {
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
        switch(axis_idx_for_jno[i]) {
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

static KINEMATICS_TYPE ktype = -1;

KINEMATICS_TYPE kinematicsType()
{
    return ktype;
}

#define TRIVKINS_DEFAULT_COORDINATES "XYZABCUVW"
static char *coordinates = TRIVKINS_DEFAULT_COORDINATES;
RTAPI_MP_STRING(coordinates, "Existing Axes");

static char *kinstype = "1"; // use KINEMATICS_IDENTITY
RTAPI_MP_STRING(kinstype, "Kinematics Type (Identity,Both)");

EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
MODULE_LICENSE("GPL");

static int comp_id;

int rtapi_app_main(void) {
    if (map_coordinates_to_jnumbers(coordinates,
                                    EMCMOT_MAX_JOINTS,
                                    ALLOW_DUPLICATES,
                                    axis_idx_for_jno)) {
       return -1; //mapping failed
    }
    comp_id = hal_init("trivkins");
    if(comp_id < 0) return comp_id;

    switch (*kinstype) {
      case 'b': case 'B': ktype = KINEMATICS_BOTH;         break;
      case 'f': case 'F': ktype = KINEMATICS_FORWARD_ONLY; break;
      case 'i': case 'I': ktype = KINEMATICS_INVERSE_ONLY; break;
      case '1': default:  ktype = KINEMATICS_IDENTITY;
    }

    /* print message for unconventional ordering;
    **   a) duplicate coordinate letters
    **   b) letters not ordered by "XYZABCUVW" sequence
    **      (use kinstype=both works best for these)
    */
    {
        int jno,islathe,show=0;
        for (jno=0; jno<EMCMOT_MAX_JOINTS; jno++) {
            if (axis_idx_for_jno[jno] == -1) break; //fini
            if (axis_idx_for_jno[jno] != jno) { show++; } //not default order
        }
        islathe = !strcasecmp(coordinates,"xz"); // no show if simple lathe
        if (show && !islathe) {
            rtapi_print("\ntrivkins: coordinates:%s\n", coordinates);
            char *p="XYZABCUVW";
            for (jno=0; jno<EMCMOT_MAX_JOINTS; jno++) {
                if (axis_idx_for_jno[jno] == -1) break; //fini
                rtapi_print("   Joint %d ==> Axis %c\n",
                           jno,*(p+axis_idx_for_jno[jno]));
            }
            if (ktype != KINEMATICS_BOTH) {
                rtapi_print("trivkins: Recommend: kinstype=both\n");
            }
            rtapi_print("\n");
        }
    }

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
