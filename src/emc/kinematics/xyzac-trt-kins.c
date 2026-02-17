/********************************************************************
* xyzac-trt-kins.c employing switchkins.[ch]
* License: GPL Version 2
*
* NOTEs:
*  1) specify all kparms items
*  2) specify 3 KS,KF,KI functions for switchkins_type=0,1,2
*  3) the 0th switchkins_type is the startup default
*  4) sparm is a module string parameter for configuration
*  5) The directions of the rotational axes are the opposite of the
*     conventional axis directions.
*/

#include "motion.h"
#include "switchkins.h"
#include "rtapi_string.h"
#include "rtapi.h"

int switchkinsSetup(kparms* kp,
                    KS* kset0, KS* kset1, KS* kset2,
                    KF* kfwd0, KF* kfwd1, KF* kfwd2,
                    KI* kinv0, KI* kinv1, KI* kinv2
                   )
{
    kp->kinsname    = "xyzac-trt-kins"; // !!! must agree with filename
    kp->halprefix   = "xyzac-trt-kins"; // hal pin names
    kp->required_coordinates = "xyzac";
    kp->allow_duplicates     = 1;
    kp->max_joints           = EMCMOT_MAX_JOINTS;

    if (kp->sparm && strstr(kp->sparm,"identityfirst")) {
        rtapi_print("\n!!! switchkins-type 0 is IDENTITY\n");
        *kset0 = identityKinematicsSetup;
        *kfwd0 = identityKinematicsForward;
        *kinv0 = identityKinematicsInverse;

        *kset1 = trtKinematicsSetup; // trt: xyzac,xyzbc
        *kfwd1 = xyzacKinematicsForward;
        *kinv1 = xyzacKinematicsInverse;
    } else {
        rtapi_print("\n!!! switchkins-type 0 is %s\n",kp->kinsname);
        *kset0 = trtKinematicsSetup; // trt: xyzac,xyzbc
        *kfwd0 = xyzacKinematicsForward;
        *kinv0 = xyzacKinematicsInverse;

        *kset1 = identityKinematicsSetup;
        *kfwd1 = identityKinematicsForward;
        *kinv1 = identityKinematicsInverse;
    }

    *kset2 = userkKinematicsSetup;
    *kfwd2 = userkKinematicsForward;
    *kinv2 = userkKinematicsInverse;

    return 0;
}

/* ========================================================================
 * Non-RT interface for userspace trajectory planner
 * ======================================================================== */
#include "kinematics_params.h"

/* TRT math types and functions (defined in trtfuncs.c, linked into this .so) */
typedef struct {
    double x_rot_point, y_rot_point, z_rot_point;
    double x_offset, y_offset, z_offset, tool_offset;
    int conventional_directions;
} trt_params_t;

typedef struct {
    int jx, jy, jz, ja, jb, jc, ju, jv, jw;
} trt_joints_t;

extern int xyzac_forward_math(const trt_params_t *, const trt_joints_t *,
                               const double *, EmcPose *);
extern int xyzac_inverse_math(const trt_params_t *, const trt_joints_t *,
                               const EmcPose *, EmcPose *);
extern void trt_axis_to_joints(const trt_joints_t *, const EmcPose *, double *);

static void nonrt_build_trt(const kinematics_params_t *kp,
                             trt_params_t *p, trt_joints_t *jm)
{
    p->x_rot_point = kp->params.trt.x_rot_point;
    p->y_rot_point = kp->params.trt.y_rot_point;
    p->z_rot_point = kp->params.trt.z_rot_point;
    p->x_offset    = kp->params.trt.x_offset;
    p->y_offset    = kp->params.trt.y_offset;
    p->z_offset    = kp->params.trt.z_offset;
    p->tool_offset = kp->params.trt.tool_offset;
    p->conventional_directions = kp->params.trt.conventional_directions;

    jm->jx = kp->axis_to_joint[0]; jm->jy = kp->axis_to_joint[1];
    jm->jz = kp->axis_to_joint[2]; jm->ja = kp->axis_to_joint[3];
    jm->jb = kp->axis_to_joint[4]; jm->jc = kp->axis_to_joint[5];
    jm->ju = kp->axis_to_joint[6]; jm->jv = kp->axis_to_joint[7];
    jm->jw = kp->axis_to_joint[8];
}

int nonrt_kinematicsForward(const void *params,
                            const double *joints,
                            EmcPose *pos)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    trt_params_t p;
    trt_joints_t jm;
    nonrt_build_trt(kp, &p, &jm);
    return xyzac_forward_math(&p, &jm, joints, pos);
}

int nonrt_kinematicsInverse(const void *params,
                            const EmcPose *pos,
                            double *joints)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    trt_params_t p;
    trt_joints_t jm;
    EmcPose axis_values;

    nonrt_build_trt(kp, &p, &jm);
    xyzac_inverse_math(&p, &jm, pos, &axis_values);
    trt_axis_to_joints(&jm, &axis_values, joints);
    return 0;
}

int nonrt_refresh(void *params,
                  int (*read_float)(const char *, double *),
                  int (*read_bit)(const char *, int *),
                  int (*read_s32)(const char *, int *))
{
    kinematics_params_t *kp = (kinematics_params_t *)params;
    (void)read_s32;

    read_float("xyzac-trt-kins.x-rot-point", &kp->params.trt.x_rot_point);
    read_float("xyzac-trt-kins.y-rot-point", &kp->params.trt.y_rot_point);
    read_float("xyzac-trt-kins.z-rot-point", &kp->params.trt.z_rot_point);
    read_float("xyzac-trt-kins.x-offset",    &kp->params.trt.x_offset);
    read_float("xyzac-trt-kins.y-offset",    &kp->params.trt.y_offset);
    read_float("xyzac-trt-kins.z-offset",    &kp->params.trt.z_offset);
    read_float("xyzac-trt-kins.tool-offset", &kp->params.trt.tool_offset);
    read_bit("xyzac-trt-kins.conventional-directions",
             &kp->params.trt.conventional_directions);
    return 0;
}

int nonrt_is_identity(void) { return 0; }

EXPORT_SYMBOL(nonrt_kinematicsForward);
EXPORT_SYMBOL(nonrt_kinematicsInverse);
EXPORT_SYMBOL(nonrt_refresh);
EXPORT_SYMBOL(nonrt_is_identity);
