/********************************************************************
 * genhexkins userspace plugin - 6-DOF hexapod/Stewart platform
 * (needs libposemath)
 ********************************************************************/
#include "kins_plugin.h"
#include "hal_pin_reader.h"
#include "genhexkins_math.h"
#include <stdio.h>

static void build_params(kinematics_params_t *kp, genhex_params_t *gparams)
{
    int i;
    genhex_init_params(gparams);
    for (i = 0; i < GENHEX_NUM_STRUTS; i++) {
        gparams->base[i].x = kp->params.genhex.basex[i];
        gparams->base[i].y = kp->params.genhex.basey[i];
        gparams->base[i].z = kp->params.genhex.basez[i];
        gparams->platform[i].x = kp->params.genhex.platformx[i];
        gparams->platform[i].y = kp->params.genhex.platformy[i];
        gparams->platform[i].z = kp->params.genhex.platformz[i];
        gparams->base_n[i].x = kp->params.genhex.basenx[i];
        gparams->base_n[i].y = kp->params.genhex.baseny[i];
        gparams->base_n[i].z = kp->params.genhex.basenz[i];
        gparams->platform_n[i].x = kp->params.genhex.platformnx[i];
        gparams->platform_n[i].y = kp->params.genhex.platformny[i];
        gparams->platform_n[i].z = kp->params.genhex.platformnz[i];
    }
    gparams->conv_criterion = kp->params.genhex.conv_criterion;
    gparams->iter_limit = kp->params.genhex.iter_limit;
    gparams->max_error = kp->params.genhex.max_error;
    gparams->tool_offset = kp->params.genhex.tool_offset;
    gparams->spindle_offset = kp->params.genhex.spindle_offset;
    gparams->screw_lead = kp->params.genhex.screw_lead;
}

static int ghex_inv(KinematicsUserContext *ctx,
                    const EmcPose *world, double *joints)
{
    genhex_params_t gparams;
    build_params(&ctx->params, &gparams);
    return genhex_inv(&gparams, world, joints);
}

static int ghex_fwd(KinematicsUserContext *ctx,
                    const double *joints, EmcPose *world)
{
    genhex_params_t gparams;
    int result;
    build_params(&ctx->params, &gparams);
    result = genhex_fwd(&gparams, joints, world);
    ctx->params.params.genhex.max_iter = gparams.max_iterations;
    return result;
}

static int ghex_refresh(KinematicsUserContext *ctx)
{
    kinematics_params_t *kp = &ctx->params;
    int i;
    char pin_name[64];
    double fval;
    int ival;

    for (i = 0; i < KINS_GENHEX_NUM_STRUTS; i++) {
        snprintf(pin_name, sizeof(pin_name), "genhexkins.base.%d.x", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.basex[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "genhexkins.base.%d.y", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.basey[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "genhexkins.base.%d.z", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.basez[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "genhexkins.platform.%d.x", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.platformx[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "genhexkins.platform.%d.y", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.platformy[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "genhexkins.platform.%d.z", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.platformz[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "genhexkins.base-n.%d.x", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.basenx[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "genhexkins.base-n.%d.y", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.baseny[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "genhexkins.base-n.%d.z", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.basenz[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "genhexkins.platform-n.%d.x", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.platformnx[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "genhexkins.platform-n.%d.y", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.platformny[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "genhexkins.platform-n.%d.z", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genhex.platformnz[i] = fval;
    }
    if (hal_pin_reader_read_float("genhexkins.convergence-criterion", &fval) == 0)
        kp->params.genhex.conv_criterion = fval;
    if (hal_pin_reader_read_s32("genhexkins.limit-iterations", &ival) == 0)
        kp->params.genhex.iter_limit = (unsigned int)ival;
    if (hal_pin_reader_read_float("genhexkins.max-error", &fval) == 0)
        kp->params.genhex.max_error = fval;
    if (hal_pin_reader_read_float("genhexkins.tool-offset", &fval) == 0)
        kp->params.genhex.tool_offset = fval;
    if (hal_pin_reader_read_float("genhexkins.spindle-offset", &fval) == 0)
        kp->params.genhex.spindle_offset = fval;
    if (hal_pin_reader_read_float("genhexkins.screw-lead", &fval) == 0)
        kp->params.genhex.screw_lead = fval;
    return 0;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    kinematics_params_t *kp = &ctx->params;

    ctx->inverse = ghex_inv;
    ctx->forward = ghex_fwd;
    ctx->refresh = ghex_refresh;
    ctx->is_identity = 0;
    ctx->kins_type = KINEMATICS_BOTH;

    /* Default hexapod geometry */
    kp->params.genhex.basex[0] = -22.950; kp->params.genhex.basey[0] =  13.250; kp->params.genhex.basez[0] = 0.000;
    kp->params.genhex.basex[1] =  22.950; kp->params.genhex.basey[1] =  13.250; kp->params.genhex.basez[1] = 0.000;
    kp->params.genhex.basex[2] =  22.950; kp->params.genhex.basey[2] =  13.250; kp->params.genhex.basez[2] = 0.000;
    kp->params.genhex.basex[3] =   0.000; kp->params.genhex.basey[3] = -26.500; kp->params.genhex.basez[3] = 0.000;
    kp->params.genhex.basex[4] =   0.000; kp->params.genhex.basey[4] = -26.500; kp->params.genhex.basez[4] = 0.000;
    kp->params.genhex.basex[5] = -22.950; kp->params.genhex.basey[5] =  13.250; kp->params.genhex.basez[5] = 0.000;
    kp->params.genhex.platformx[0] =  -1.000; kp->params.genhex.platformy[0] =  11.500; kp->params.genhex.platformz[0] = 0.000;
    kp->params.genhex.platformx[1] =   1.000; kp->params.genhex.platformy[1] =  11.500; kp->params.genhex.platformz[1] = 0.000;
    kp->params.genhex.platformx[2] =  10.459; kp->params.genhex.platformy[2] =  -4.884; kp->params.genhex.platformz[2] = 0.000;
    kp->params.genhex.platformx[3] =   9.459; kp->params.genhex.platformy[3] =  -6.616; kp->params.genhex.platformz[3] = 0.000;
    kp->params.genhex.platformx[4] =  -9.459; kp->params.genhex.platformy[4] =  -6.616; kp->params.genhex.platformz[4] = 0.000;
    kp->params.genhex.platformx[5] = -10.459; kp->params.genhex.platformy[5] =  -4.884; kp->params.genhex.platformz[5] = 0.000;
    kp->params.genhex.basenx[0] =  0.707107; kp->params.genhex.baseny[0] =  0.0;      kp->params.genhex.basenz[0] = 0.707107;
    kp->params.genhex.basenx[1] =  0.0;      kp->params.genhex.baseny[1] = -0.707107; kp->params.genhex.basenz[1] = 0.707107;
    kp->params.genhex.basenx[2] = -0.707107; kp->params.genhex.baseny[2] =  0.0;      kp->params.genhex.basenz[2] = 0.707107;
    kp->params.genhex.basenx[3] = -0.707107; kp->params.genhex.baseny[3] =  0.0;      kp->params.genhex.basenz[3] = 0.707107;
    kp->params.genhex.basenx[4] =  0.0;      kp->params.genhex.baseny[4] =  0.707107; kp->params.genhex.basenz[4] = 0.707107;
    kp->params.genhex.basenx[5] =  0.707107; kp->params.genhex.baseny[5] =  0.0;      kp->params.genhex.basenz[5] = 0.707107;
    kp->params.genhex.platformnx[0] = -1.0;     kp->params.genhex.platformny[0] =  0.0; kp->params.genhex.platformnz[0] = 0.0;
    kp->params.genhex.platformnx[1] =  0.866025; kp->params.genhex.platformny[1] =  0.5; kp->params.genhex.platformnz[1] = 0.0;
    kp->params.genhex.platformnx[2] =  0.866025; kp->params.genhex.platformny[2] =  0.5; kp->params.genhex.platformnz[2] = 0.0;
    kp->params.genhex.platformnx[3] =  0.866025; kp->params.genhex.platformny[3] = -0.5; kp->params.genhex.platformnz[3] = 0.0;
    kp->params.genhex.platformnx[4] =  0.866025; kp->params.genhex.platformny[4] = -0.5; kp->params.genhex.platformnz[4] = 0.0;
    kp->params.genhex.platformnx[5] = -1.0;     kp->params.genhex.platformny[5] =  0.0; kp->params.genhex.platformnz[5] = 0.0;
    kp->params.genhex.conv_criterion = 1e-9;
    kp->params.genhex.iter_limit = 120;
    kp->params.genhex.max_error = 500.0;
    kp->params.genhex.tool_offset = 0.0;
    kp->params.genhex.spindle_offset = 0.0;
    kp->params.genhex.screw_lead = 0.0;
    return 0;
}
