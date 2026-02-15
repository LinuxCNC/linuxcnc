/********************************************************************
* Description: genhexkins.c
*
*   Kinematics for a generalised hexapod machine
*
*   Derived from a work by R. Brian Register
*
* Adapting Author: Andrew Kyrychenko
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
*********************************************************************

  These are the forward and inverse kinematic functions for a class of
  machines referred to as "Stewart Platforms".

  The functions are general enough to be configured for any platform
  configuration.  In the functions "genhexKinematicsForward" and
  "genhexKinematicsInverse" are arrays "a[i]" and "b[i]".  The values stored
  in these arrays correspond to the positions of the ends of the i'th
  strut. The value stored in a[i] is the position of the end of the i'th
  strut attached to the platform, in platform coordinates. The value
  stored in b[i] is the position of the end of the i'th strut attached
  to the base, in base (world) coordinates.

  The default values for base and platform joints positions are defined
  in the header file genhexkins.h.  The actual values for a particular
  machine can be adjusted by hal parameters:

  genhexkins.base.N.x
  genhexkins.base.N.y
  genhexkins.base.N.z - base joint coordinates.

  genhexkins.platform.N.x
  genhexkins.platform.N.y
  genhexkins.platform.N.z - platform joint coordinates.

  genhexkins.spindle-offset - added to Z coordinates of all joints to
                              change the machine origin. Facilitates
                              adjusting spindle position.

  genhexkins.tool-offset - tool length offset (TCP offset along Z),
                           implements RTCP function when connected to
                           motion.tooloffset.Z.

  To avoid joints jump change tool offset (G43, G49) only when the
  platform is not tilted (A = B = 0).

  Some hexapods use non-captive screw actuators and universal (cardanic)
  joints, thus the strut lengths depend on orientation of joints axes.
  Strut length correction is implemented to compensate for this.
  The calculations use orientation (unit vectors) of base and platform
  joint axes and the lead of actuator screws:

  genhexkins.base-n.N.x
  genhexkins.base-n.N.y
  genhexkins.base-n.N.z - unit vectors of base joint axes;

  genhexkins.platform-n.N.x
  genhexkins.platform-n.N.y
  genhexkins.platform-n.N.z - unit vectors of platform joint axes
                              in platform CS.
  genhexkins.screw-lead - lead of strut actuator screw, positive for
                          right-hand thread. Default is 0 (strut length
                          correction disabled).
  genhexkins.correction.N - pins showing current values of strut length
                            correction.

  The genhexKinematicsInverse function solves the inverse kinematics using
  a closed form algorithm.  The inverse kinematics problem is given
  the pose of the platform and returns the strut lengths. For this
  problem there is only one solution that is always returned correctly.

  The genhexKinematicsForward function solves the forward kinematics using
  an iterative algorithm.  Due to the iterative nature of this algorithm
  the genhexKinematicsForward function requires an initial value to begin the
  iterative routine and then converges to the "nearest" solution. The
  forward kinematics problem is given the strut lengths and returns the
  pose of the platform.  For this problem there arein multiple
  solutions.  The genhexKinematicsForward function will return only one of
  these solutions which will be the solution nearest to the initial
  value given.  It is possible that there are no solutions "near" the
  given initial value and the iteration will not converge and no
  solution will be returned.  Assuming there is a solution "near" the
  initial value, the function will always return one correct solution
  out of the multiple possible solutions.

  Hal pins to control and observe forward kinematics iterations:

  genhexkins.convergence-criterion - minimum error value that ends
                    iterations with converged solution;

  genhexkins.limit-iterations - limit of iterations, if exceeded
                    iterations stop with no convergence;

  genhexkins.max-error - maximum error value, if exceeded iterations
                    stop with no convergence;

  genhexkins.last-iterations - number of iterations spent for the
                    last forward kinematics solution;

  genhexkins.max-iterations - maximum number of iterations spent for
                    a converged solution during current session.

 ----------------------------------------------------------------------------*/

#include "rtapi.h"
#include "rtapi_math.h"
#include "rtapi_string.h"
#include "posemath.h"
#include "hal.h"
#include "genhexkins.h"
#include "genhexkins_math.h"
#include "motion.h"
#include "kinematics.h"             /* these decls, KINEMATICS_FORWARD_FLAGS */
#include "switchkins.h"

static struct haldata {
    hal_float_t *basex[NUM_STRUTS];
    hal_float_t *basey[NUM_STRUTS];
    hal_float_t *basez[NUM_STRUTS];
    hal_float_t *platformx[NUM_STRUTS];
    hal_float_t *platformy[NUM_STRUTS];
    hal_float_t *platformz[NUM_STRUTS];
    hal_float_t *basenx[NUM_STRUTS];
    hal_float_t *baseny[NUM_STRUTS];
    hal_float_t *basenz[NUM_STRUTS];
    hal_float_t *platformnx[NUM_STRUTS];
    hal_float_t *platformny[NUM_STRUTS];
    hal_float_t *platformnz[NUM_STRUTS];
    hal_float_t *correction[NUM_STRUTS];
    hal_float_t *screw_lead;
    hal_u32_t   *last_iter;
    hal_u32_t   *max_iter;
    hal_u32_t   *iter_limit;
    hal_float_t *max_error;
    hal_float_t *conv_criterion;
    hal_float_t *tool_offset;
    hal_float_t *spindle_offset;
    hal_bit_t   *fwd_kins_fail;

    hal_float_t *gui_x;
    hal_float_t *gui_y;
    hal_float_t *gui_z;
    hal_float_t *gui_a;
    hal_float_t *gui_b;
    hal_float_t *gui_c;

} *haldata;

static int genhex_gui_forward_kins(EmcPose *pos)
{
    *haldata->gui_x = pos->tran.x;
    *haldata->gui_y = pos->tran.y;
    *haldata->gui_z = pos->tran.z;
    *haldata->gui_a = pos->a;
    *haldata->gui_b = pos->b;
    *haldata->gui_c = pos->c;
    return 0;
} // genhex_gui_forward_kins

/************************genhex_read_hal_pins**************************/

static void genhex_read_hal_pins(genhex_params_t *params) {
    int t;

    /* set the base and platform coordinates from hal pin values */
    for (t = 0; t < NUM_STRUTS; t++) {
        params->base[t].x = *haldata->basex[t];
        params->base[t].y = *haldata->basey[t];
        params->base[t].z = *haldata->basez[t];

        params->platform[t].x = *haldata->platformx[t];
        params->platform[t].y = *haldata->platformy[t];
        params->platform[t].z = *haldata->platformz[t];

        params->base_n[t].x = *haldata->basenx[t];
        params->base_n[t].y = *haldata->baseny[t];
        params->base_n[t].z = *haldata->basenz[t];

        params->platform_n[t].x = *haldata->platformnx[t];
        params->platform_n[t].y = *haldata->platformny[t];
        params->platform_n[t].z = *haldata->platformnz[t];
    }

    /* set iteration and offset parameters */
    params->conv_criterion = *haldata->conv_criterion;
    params->iter_limit = *haldata->iter_limit;
    params->max_error = *haldata->max_error;
    params->tool_offset = *haldata->tool_offset;
    params->spindle_offset = *haldata->spindle_offset;
    params->screw_lead = *haldata->screw_lead;
} // genhex_read_hal_pins()

/**************** genhexKinematicsForward() *****************/
static int genhexKinematicsForward(const double * joints,
                                   EmcPose * pos,
                                   const KINEMATICS_FORWARD_FLAGS * fflags,
                                   KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    (void)iflags;

    genhex_params_t params;
    int result;
    int i;

    genhex_read_hal_pins(&params);

    result = genhex_fwd(&params, joints, pos);

    if (result != 0) {
        *haldata->fwd_kins_fail = 1;
        return result;
    }

    /* update HAL output pins from params */
    *haldata->last_iter = params.last_iterations;
    if (params.last_iterations > *haldata->max_iter) {
        *haldata->max_iter = params.last_iterations;
    }
    for (i = 0; i < NUM_STRUTS; i++) {
        *haldata->correction[i] = params.correction[i];
    }
    *haldata->fwd_kins_fail = 0;

    genhex_gui_forward_kins(pos);

    return 0;
} // genhexKinematicsForward()


/************************ genhexKinematicsInverse() ************************/
/* the inverse kinematics take world coordinates and determine joint values,
   given the inverse kinematics flags to resolve any ambiguities. The forward
   flags are set to indicate their value appropriate to the world coordinates
   passed in. */

static int genhexKinematicsInverse(const EmcPose * pos,
                                   double * joints,
                                   const KINEMATICS_INVERSE_FLAGS * iflags,
                                   KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)iflags;
    (void)fflags;

    genhex_params_t params;
    int result;
    int i;

    genhex_read_hal_pins(&params);

    result = genhex_inv(&params, pos, joints);

    /* update HAL output pins from params */
    for (i = 0; i < NUM_STRUTS; i++) {
        *haldata->correction[i] = params.correction[i];
    }

    return result;
} //genhexKinematicsInverse()

static
int genhexKinematicsSetup(const  int   comp_id,
                          const  char* coordinates,
                          kparms*      kp)
{
    (void)coordinates;
    int i,res=0;

    haldata = hal_malloc(sizeof(struct haldata));
    if (!haldata) {
        rtapi_print_msg(RTAPI_MSG_ERR,"genhexKinematicsSetup: hal_malloc fail\n");
        return -1;
    }

    for (i = 0; i < kp->max_joints; i++) {
        res += hal_pin_float_newf(HAL_IN, &(haldata->basex[i]), comp_id,
            "%s.base.%d.x", kp->halprefix, i);
        res += hal_pin_float_newf(HAL_IN, &haldata->basey[i], comp_id,
            "%s.base.%d.y", kp->halprefix, i);
        res += hal_pin_float_newf(HAL_IN, &haldata->basez[i], comp_id,
            "%s.base.%d.z", kp->halprefix, i);
        res += hal_pin_float_newf(HAL_IN, &haldata->platformx[i], comp_id,
            "%s.platform.%d.x", kp->halprefix, i);
        res += hal_pin_float_newf(HAL_IN, &haldata->platformy[i], comp_id,
            "%s.platform.%d.y", kp->halprefix, i);
        res += hal_pin_float_newf(HAL_IN, &haldata->platformz[i], comp_id,
            "%s.platform.%d.z", kp->halprefix, i);
        res += hal_pin_float_newf(HAL_IN, &haldata->basenx[i], comp_id,
            "%s.base-n.%d.x", kp->halprefix, i);
        res += hal_pin_float_newf(HAL_IN, &haldata->baseny[i], comp_id,
            "%s.base-n.%d.y", kp->halprefix, i);
        res += hal_pin_float_newf(HAL_IN, &haldata->basenz[i], comp_id,
            "%s.base-n.%d.z", kp->halprefix, i);
        res += hal_pin_float_newf(HAL_IN, &haldata->platformnx[i], comp_id,
            "%s.platform-n.%d.x", kp->halprefix, i);
        res += hal_pin_float_newf(HAL_IN, &haldata->platformny[i], comp_id,
            "%s.platform-n.%d.y", kp->halprefix, i);
        res += hal_pin_float_newf(HAL_IN, &haldata->platformnz[i], comp_id,
            "%s.platform-n.%d.z", kp->halprefix, i);
        res += hal_pin_float_newf(HAL_OUT, &haldata->correction[i], comp_id,
            "%s.correction.%d", kp->halprefix, i);
        if (res) {goto error;}
        *haldata->correction[i] = 0.0;

    }

    res += hal_pin_u32_newf(HAL_OUT, &haldata->last_iter, comp_id,
        "genhexkins.last-iterations");
    *haldata->last_iter = 0;
    res += hal_pin_u32_newf(HAL_OUT, &haldata->max_iter, comp_id,
        "genhexkins.max-iterations");
    *haldata->max_iter = 0;
    res += hal_pin_float_newf(HAL_IN, &haldata->max_error, comp_id,
        "genhexkins.max-error");
    *haldata->max_error = 500.0;
    res += hal_pin_float_newf(HAL_IN, &haldata->conv_criterion, comp_id,
        "genhexkins.convergence-criterion");
    *haldata->conv_criterion = 1e-9;
    res += hal_pin_u32_newf(HAL_IN, &haldata->iter_limit, comp_id,
        "genhexkins.limit-iterations");
    *haldata->iter_limit = 120;
    res += hal_pin_float_newf(HAL_IN, &haldata->tool_offset, comp_id,
        "genhexkins.tool-offset");
    *haldata->tool_offset = 0.0;
    res += hal_pin_float_newf(HAL_IN, &haldata->spindle_offset, comp_id,
        "genhexkins.spindle-offset");
    *haldata->spindle_offset = 0.0;
    res += hal_pin_float_newf(HAL_IN, &haldata->screw_lead, comp_id,
        "genhexkins.screw-lead");
    *haldata->screw_lead = DEFAULT_SCREW_LEAD;

    if (res) {goto error;}

    *haldata->basex[0] = DEFAULT_BASE_0_X;
    *haldata->basey[0] = DEFAULT_BASE_0_Y;
    *haldata->basez[0] = DEFAULT_BASE_0_Z;
    *haldata->basex[1] = DEFAULT_BASE_1_X;
    *haldata->basey[1] = DEFAULT_BASE_1_Y;
    *haldata->basez[1] = DEFAULT_BASE_1_Z;
    *haldata->basex[2] = DEFAULT_BASE_2_X;
    *haldata->basey[2] = DEFAULT_BASE_2_Y;
    *haldata->basez[2] = DEFAULT_BASE_2_Z;
    *haldata->basex[3] = DEFAULT_BASE_3_X;
    *haldata->basey[3] = DEFAULT_BASE_3_Y;
    *haldata->basez[3] = DEFAULT_BASE_3_Z;
    *haldata->basex[4] = DEFAULT_BASE_4_X;
    *haldata->basey[4] = DEFAULT_BASE_4_Y;
    *haldata->basez[4] = DEFAULT_BASE_4_Z;
    *haldata->basex[5] = DEFAULT_BASE_5_X;
    *haldata->basey[5] = DEFAULT_BASE_5_Y;
    *haldata->basez[5] = DEFAULT_BASE_5_Z;

    *haldata->platformx[0] = DEFAULT_PLATFORM_0_X;
    *haldata->platformy[0] = DEFAULT_PLATFORM_0_Y;
    *haldata->platformz[0] = DEFAULT_PLATFORM_0_Z;
    *haldata->platformx[1] = DEFAULT_PLATFORM_1_X;
    *haldata->platformy[1] = DEFAULT_PLATFORM_1_Y;
    *haldata->platformz[1] = DEFAULT_PLATFORM_1_Z;
    *haldata->platformx[2] = DEFAULT_PLATFORM_2_X;
    *haldata->platformy[2] = DEFAULT_PLATFORM_2_Y;
    *haldata->platformz[2] = DEFAULT_PLATFORM_2_Z;
    *haldata->platformx[3] = DEFAULT_PLATFORM_3_X;
    *haldata->platformy[3] = DEFAULT_PLATFORM_3_Y;
    *haldata->platformz[3] = DEFAULT_PLATFORM_3_Z;
    *haldata->platformx[4] = DEFAULT_PLATFORM_4_X;
    *haldata->platformy[4] = DEFAULT_PLATFORM_4_Y;
    *haldata->platformz[4] = DEFAULT_PLATFORM_4_Z;
    *haldata->platformx[5] = DEFAULT_PLATFORM_5_X;
    *haldata->platformy[5] = DEFAULT_PLATFORM_5_Y;
    *haldata->platformz[5] = DEFAULT_PLATFORM_5_Z;

    *haldata->basenx[0] = DEFAULT_BASE_0_NX;
    *haldata->baseny[0] = DEFAULT_BASE_0_NY;
    *haldata->basenz[0] = DEFAULT_BASE_0_NZ;
    *haldata->basenx[1] = DEFAULT_BASE_1_NX;
    *haldata->baseny[1] = DEFAULT_BASE_1_NY;
    *haldata->basenz[1] = DEFAULT_BASE_1_NZ;
    *haldata->basenx[2] = DEFAULT_BASE_2_NX;
    *haldata->baseny[2] = DEFAULT_BASE_2_NY;
    *haldata->basenz[2] = DEFAULT_BASE_2_NZ;
    *haldata->basenx[3] = DEFAULT_BASE_3_NX;
    *haldata->baseny[3] = DEFAULT_BASE_3_NY;
    *haldata->basenz[3] = DEFAULT_BASE_3_NZ;
    *haldata->basenx[4] = DEFAULT_BASE_4_NX;
    *haldata->baseny[4] = DEFAULT_BASE_4_NY;
    *haldata->basenz[4] = DEFAULT_BASE_4_NZ;
    *haldata->basenx[5] = DEFAULT_BASE_5_NX;
    *haldata->baseny[5] = DEFAULT_BASE_5_NY;
    *haldata->basenz[5] = DEFAULT_BASE_5_NZ;

    *haldata->platformnx[0] = DEFAULT_PLATFORM_0_NX;
    *haldata->platformny[0] = DEFAULT_PLATFORM_0_NY;
    *haldata->platformnz[0] = DEFAULT_PLATFORM_0_NZ;
    *haldata->platformnx[1] = DEFAULT_PLATFORM_1_NX;
    *haldata->platformny[1] = DEFAULT_PLATFORM_1_NY;
    *haldata->platformnz[1] = DEFAULT_PLATFORM_1_NZ;
    *haldata->platformnx[2] = DEFAULT_PLATFORM_2_NX;
    *haldata->platformny[2] = DEFAULT_PLATFORM_2_NY;
    *haldata->platformnz[2] = DEFAULT_PLATFORM_2_NZ;
    *haldata->platformnx[3] = DEFAULT_PLATFORM_3_NX;
    *haldata->platformny[3] = DEFAULT_PLATFORM_3_NY;
    *haldata->platformnz[3] = DEFAULT_PLATFORM_3_NZ;
    *haldata->platformnx[4] = DEFAULT_PLATFORM_4_NX;
    *haldata->platformny[4] = DEFAULT_PLATFORM_4_NY;
    *haldata->platformnz[4] = DEFAULT_PLATFORM_4_NZ;
    *haldata->platformnx[5] = DEFAULT_PLATFORM_5_NX;
    *haldata->platformny[5] = DEFAULT_PLATFORM_5_NY;
    *haldata->platformnz[5] = DEFAULT_PLATFORM_5_NZ;

    //note: switchkins does not uses these as it provides gui.x, gui.y, etc.
    res += hal_pin_float_newf(HAL_IN, &haldata->gui_x, comp_id, "genhexkins.x");
    res += hal_pin_float_newf(HAL_IN, &haldata->gui_y, comp_id, "genhexkins.y");
    res += hal_pin_float_newf(HAL_IN, &haldata->gui_z, comp_id, "genhexkins.z");
    res += hal_pin_float_newf(HAL_IN, &haldata->gui_a, comp_id, "genhexkins.a");
    res += hal_pin_float_newf(HAL_IN, &haldata->gui_b, comp_id, "genhexkins.b");
    res += hal_pin_float_newf(HAL_IN, &haldata->gui_c, comp_id, "genhexkins.c");

    res += hal_pin_bit_newf(HAL_OUT, &haldata->fwd_kins_fail, comp_id,
        "genhexkins.fwd-kins-fail");

    if (res) goto error;
    return 0;

error:
    return res;
} // genhexKinematicsSetup()

int switchkinsSetup(kparms* kp,
                    KS* kset0, KS* kset1, KS* kset2,
                    KF* kfwd0, KF* kfwd1, KF* kfwd2,
                    KI* kinv0, KI* kinv1, KI* kinv2
                   )
{
    kp->kinsname    = "genhexkins"; // !!! must agree with filename
    kp->halprefix   = "genhexkins"; // hal pin names
    kp->required_coordinates = "xyzabc";
    kp->max_joints  = strlen(kp->required_coordinates);
    kp->allow_duplicates  = 0;
    kp->fwd_iterates_mask = 0x1; //genhexkins switchkins_type==0
    kp->gui_kinstype      = 0;   //vismach gui for switchkins_type==0

    // switchkins_type==0 is startup default
    // kins with iterative forward algorithm should be switchkins_type==0
    *kset0 = genhexKinematicsSetup;
    *kfwd0 = genhexKinematicsForward;
    *kinv0 = genhexKinematicsInverse;

    *kset1 = identityKinematicsSetup;
    *kfwd1 = identityKinematicsForward;
    *kinv1 = identityKinematicsInverse;

    *kset2 = userkKinematicsSetup;
    *kfwd2 = userkKinematicsForward;
    *kinv2 = userkKinematicsInverse;

    return 0;
} //switchkinsSetup()
