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

#include <rtapi.h>      /* RTAPI realtime OS API */
#include <rtapi_app.h>  /* RTAPI realtime module decls */
#include <rtapi_string.h>
#include <hal.h>
#include <emcmotcfg.h>
#include <kinematics.h>
#include <kins_rt.h>

// joints are axes, through whatever map coordinates= gives; the maths is
// the shared identity and the entry points come from kins_single.c
const kins_module_info kins_module = {
    .name                 = "trivkins",
    .halprefix            = "trivkins",
    .params               = NULL,
    .nparams              = 0,
    .required_coordinates = "",
    .max_joints           = EMCMOT_MAX_JOINTS,
    .allow_duplicates     = 1,
    .ntypes               = 1,
    .ops                  = { &KINS_IDENTITY_OPS },
};

#define TRIVKINS_DEFAULT_COORDINATES "XYZABCUVW"
static char *coordinates = TRIVKINS_DEFAULT_COORDINATES;
RTAPI_MP_STRING(coordinates, "Existing Axes");

static char *kinstype = "1"; // use KINEMATICS_IDENTITY
RTAPI_MP_STRING(kinstype, "Kinematics Type (Identity,Both)");

MODULE_LICENSE("GPL");

static int comp_id;

// say so when the joints are not in axis order, and which type suits that
static void show_map(KINEMATICS_TYPE ktype)
{
    kins_params p;
    int a, unconventional = 0;

    if (kinsParamsInit(&p, &kins_module, coordinates)) { return; }
    for (a = 0; a < EMCMOT_MAX_AXIS; a++) {
        if (p.joint_of_axis[a] >= 0 && p.joint_of_axis[a] != a) { unconventional = 1; }
        if (p.joints_of_axis[a] & (p.joints_of_axis[a] - 1)) { unconventional = 1; }
    }
    if (!unconventional || !strcasecmp(coordinates, "xz")) { return; }

    rtapi_print("\ntrivkins: coordinates:%s\n", coordinates);
    for (a = 0; a < EMCMOT_MAX_AXIS; a++) {
        int j;
        for (j = 0; j < p.max_joints; j++) {
            if (p.joints_of_axis[a] & (1 << j)) {
                rtapi_print("   Joint %d ==> Axis %c\n", j, "XYZABCUVW"[a]);
            }
        }
    }
    if (ktype != KINEMATICS_BOTH) {
        rtapi_print("trivkins: Recommend: kinstype=both\n");
    }
    rtapi_print("\n");
}

int rtapi_app_main(void) {
    KINEMATICS_TYPE ktype;

    switch (*kinstype) {
      case 'b': case 'B': ktype = KINEMATICS_BOTH;         break;
      case 'f': case 'F': ktype = KINEMATICS_FORWARD_ONLY; break;
      case 'i': case 'I': ktype = KINEMATICS_INVERSE_ONLY; break;
      case '1': default:  ktype = KINEMATICS_IDENTITY;
    }

    comp_id = hal_init("trivkins");
    if(comp_id < 0) return comp_id;

    if (kinsSingleInit(comp_id, coordinates, ktype)) {
        hal_exit(comp_id);
        return -1;
    }
    show_map(ktype);

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
