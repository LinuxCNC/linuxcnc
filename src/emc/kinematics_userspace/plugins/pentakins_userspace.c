/********************************************************************
 * pentakins userspace plugin - 5-strut parallel kinematics
 * (needs libposemath)
 ********************************************************************/
#include "kins_plugin.h"
#include "hal_pin_reader.h"
#include "pentakins_math.h"
#include <stdio.h>

static void build_params(kinematics_params_t *kp, pentakins_params_t *pparams)
{
    int i;
    pentakins_init_params(pparams);
    for (i = 0; i < PENTAKINS_NUM_STRUTS; i++) {
        pparams->base[i].x = kp->params.penta.basex[i];
        pparams->base[i].y = kp->params.penta.basey[i];
        pparams->base[i].z = kp->params.penta.basez[i];
        pparams->effector_r[i] = kp->params.penta.effectorr[i];
        pparams->effector_z[i] = kp->params.penta.effectorz[i];
    }
    pparams->conv_criterion = kp->params.penta.conv_criterion;
    pparams->iter_limit = kp->params.penta.iter_limit;
}

static int penta_inv(KinematicsUserContext *ctx,
                     const EmcPose *world, double *joints)
{
    pentakins_params_t pparams;
    build_params(&ctx->params, &pparams);
    return pentakins_inv(&pparams, world, joints);
}

static int penta_fwd(KinematicsUserContext *ctx,
                     const double *joints, EmcPose *world)
{
    pentakins_params_t pparams;
    int result;
    build_params(&ctx->params, &pparams);
    result = pentakins_fwd(&pparams, joints, world);
    ctx->params.params.penta.iter_limit = pparams.last_iterations;
    return result;
}

static int penta_refresh(KinematicsUserContext *ctx)
{
    kinematics_params_t *kp = &ctx->params;
    int i;
    char pin_name[64];
    double fval;
    int ival;

    for (i = 0; i < KINS_PENTA_NUM_STRUTS; i++) {
        snprintf(pin_name, sizeof(pin_name), "pentakins.base.%d.x", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.penta.basex[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "pentakins.base.%d.y", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.penta.basey[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "pentakins.base.%d.z", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.penta.basez[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "pentakins.effector.%d.r", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.penta.effectorr[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "pentakins.effector.%d.z", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.penta.effectorz[i] = fval;
    }
    if (hal_pin_reader_read_float("pentakins.convergence-criterion", &fval) == 0)
        kp->params.penta.conv_criterion = fval;
    if (hal_pin_reader_read_s32("pentakins.limit-iterations", &ival) == 0)
        kp->params.penta.iter_limit = (unsigned int)ival;
    return 0;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    kinematics_params_t *kp = &ctx->params;

    ctx->inverse = penta_inv;
    ctx->forward = penta_fwd;
    ctx->refresh = penta_refresh;
    ctx->is_identity = 0;
    ctx->kins_type = KINEMATICS_BOTH;

    kp->params.penta.basex[0] = -418.03; kp->params.penta.basey[0] =  324.56; kp->params.penta.basez[0] = 895.56;
    kp->params.penta.basex[1] =  417.96; kp->params.penta.basey[1] =  324.56; kp->params.penta.basez[1] = 895.56;
    kp->params.penta.basex[2] = -418.03; kp->params.penta.basey[2] = -325.44; kp->params.penta.basez[2] = 895.56;
    kp->params.penta.basex[3] =  417.96; kp->params.penta.basey[3] = -325.44; kp->params.penta.basez[3] = 895.56;
    kp->params.penta.basex[4] =   -0.06; kp->params.penta.basey[4] = -492.96; kp->params.penta.basez[4] = 895.56;
    kp->params.penta.effectorr[0] = kp->params.penta.effectorr[1] = kp->params.penta.effectorr[2] =
    kp->params.penta.effectorr[3] = kp->params.penta.effectorr[4] = 80.32;
    kp->params.penta.effectorz[0] = -185.50;
    kp->params.penta.effectorz[1] = -159.50;
    kp->params.penta.effectorz[2] =  -67.50;
    kp->params.penta.effectorz[3] =  -41.50;
    kp->params.penta.effectorz[4] =  -14.00;
    kp->params.penta.conv_criterion = 1e-9;
    kp->params.penta.iter_limit = 120;
    return 0;
}
