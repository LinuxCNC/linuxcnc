/********************************************************************
* Description: kinematics for corexy
* Adapted from trivkins.c
* ref: http://corexy.com/theory.html
********************************************************************/

#include <rtapi.h>
#include <rtapi_app.h>
#include <hal.h>
#include <kinematics.h>
#include <kins_rt.h>

static int corexy_forward(const kins_params *p, kins_scratch *s,
                          const double *joints, EmcPose *pos,
                          const KINEMATICS_FORWARD_FLAGS *fflags,
                          KINEMATICS_INVERSE_FLAGS *iflags)
{
    (void)p;
    (void)s;
    (void)fflags;
    (void)iflags;
    pos->tran.x = 0.5 * (joints[0] + joints[1]);
    pos->tran.y = 0.5 * (joints[0] - joints[1]);
    pos->tran.z = joints[2];
    pos->a      = joints[3];
    pos->b      = joints[4];
    pos->c      = joints[5];
    pos->u      = joints[6];
    pos->v      = joints[7];
    pos->w      = joints[8];

    return 0;
}

static int corexy_inverse(const kins_params *p, kins_scratch *s,
                          const EmcPose *pos, double *joints,
                          const KINEMATICS_INVERSE_FLAGS *iflags,
                          KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)p;
    (void)s;
    (void)iflags;
    (void)fflags;
    joints[0] = pos->tran.x + pos->tran.y;
    joints[1] = pos->tran.x - pos->tran.y;
    joints[2] = pos->tran.z;
    joints[3] = pos->a;
    joints[4] = pos->b;
    joints[5] = pos->c;
    joints[6] = pos->u;
    joints[7] = pos->v;
    joints[8] = pos->w;

    return 0;
}

static int corexy_jacobian(const kins_params *p, const double *joints,
                           const EmcPose *pos,
                           double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                           const KINEMATICS_INVERSE_FLAGS *iflags)
{
    int j, a;
    (void)p;
    (void)joints;
    (void)pos;
    (void)iflags;
    for (j = 0; j < EMCMOT_MAX_JOINTS; j++) {
        for (a = 0; a < EMCMOT_MAX_AXIS; a++) { jac[j][a] = 0; }
    }
    // the two belt motors each carry x and y, in opposite senses for y
    jac[0][0] = 1; jac[0][1] =  1;
    jac[1][0] = 1; jac[1][1] = -1;
    for (j = 2; j < 9; j++) { jac[j][j] = 1; }
    return 0;
}

static const kins_ops corexy_ops = {
    .forward  = corexy_forward,
    .inverse  = corexy_inverse,
    .jacobian = corexy_jacobian,
};

// no geometry: the belts are what they are.  Joints 0..8 are the nine
// letters in order; the entry points come from kins_single.c
const kins_module_info kins_module = {
    .name                 = "corexykins",
    .halprefix            = "corexykins",
    .params               = NULL,
    .nparams              = 0,
    .required_coordinates = "XYZABCUVW",
    .max_joints           = 9,
    .allow_duplicates     = 0,
    .ntypes               = 1,
    .ops                  = { &corexy_ops },
};

MODULE_LICENSE("GPL");

static int comp_id;
int rtapi_app_main(void) {
    comp_id = hal_init("corexykins");
    if(comp_id < 0) return comp_id;

    if (kinsSingleInit(comp_id, "XYZABCUVW", KINEMATICS_BOTH)) {
        hal_exit(comp_id);
        return -1;
    }

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
