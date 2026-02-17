/********************************************************************
 * genserkins userspace plugin - generic serial robot with DH params
 * (needs libposemath)
 ********************************************************************/
#include "kins_plugin.h"
#include "hal_pin_reader.h"
#include "genserkins_math.h"
#include <stdio.h>

static void build_params(kinematics_params_t *kp, genserkins_params_t *gparams)
{
    int i;
    genserkins_init_params(gparams);
    gparams->link_num = kp->params.genser.link_num;
    gparams->max_iterations = kp->params.genser.max_iterations;
    for (i = 0; i < GENSERKINS_MAX_JOINTS && i < KINS_GENSER_MAX_JOINTS; i++) {
        gparams->a[i] = kp->params.genser.a[i];
        gparams->alpha[i] = kp->params.genser.alpha[i];
        gparams->d[i] = kp->params.genser.d[i];
        gparams->unrotate[i] = kp->params.genser.unrotate[i];
    }
}

static int genser_inv(KinematicsUserContext *ctx,
                      const EmcPose *world, double *joints)
{
    genserkins_params_t gparams;
    int result;
    build_params(&ctx->params, &gparams);
    result = genserkins_inv(&gparams, world, joints);
    ctx->params.params.genser.last_iterations = gparams.last_iterations;
    return result;
}

static int genser_fwd(KinematicsUserContext *ctx,
                      const double *joints, EmcPose *world)
{
    genserkins_params_t gparams;
    build_params(&ctx->params, &gparams);
    return genserkins_fwd(&gparams, joints, world);
}

static int genser_refresh(KinematicsUserContext *ctx)
{
    kinematics_params_t *kp = &ctx->params;
    int i;
    char pin_name[64];
    double fval;
    int ival;

    for (i = 0; i < KINS_GENSER_MAX_JOINTS && i < 6; i++) {
        snprintf(pin_name, sizeof(pin_name), "genserkins.A-%d", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genser.a[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "genserkins.ALPHA-%d", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genser.alpha[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "genserkins.D-%d", i);
        if (hal_pin_reader_read_float(pin_name, &fval) == 0)
            kp->params.genser.d[i] = fval;
        snprintf(pin_name, sizeof(pin_name), "genserkins.unrotate-%d", i);
        if (hal_pin_reader_read_s32(pin_name, &ival) == 0)
            kp->params.genser.unrotate[i] = ival;
    }
    if (hal_pin_reader_read_s32("genserkins.max-iterations", &ival) == 0)
        kp->params.genser.max_iterations = (unsigned int)ival;
    return 0;
}

int kins_userspace_setup(KinematicsUserContext *ctx)
{
    kinematics_params_t *kp = &ctx->params;

    ctx->inverse = genser_inv;
    ctx->forward = genser_fwd;
    ctx->refresh = genser_refresh;
    ctx->is_identity = 0;
    ctx->kins_type = KINEMATICS_BOTH;

    kp->params.genser.link_num = 6;
    kp->params.genser.max_iterations = GENSERKINS_DEFAULT_MAX_ITERATIONS;
    kp->params.genser.last_iterations = 0;
    kp->params.genser.a[0] = GENSERKINS_DEFAULT_A1; kp->params.genser.alpha[0] = GENSERKINS_DEFAULT_ALPHA1; kp->params.genser.d[0] = GENSERKINS_DEFAULT_D1;
    kp->params.genser.a[1] = GENSERKINS_DEFAULT_A2; kp->params.genser.alpha[1] = GENSERKINS_DEFAULT_ALPHA2; kp->params.genser.d[1] = GENSERKINS_DEFAULT_D2;
    kp->params.genser.a[2] = GENSERKINS_DEFAULT_A3; kp->params.genser.alpha[2] = GENSERKINS_DEFAULT_ALPHA3; kp->params.genser.d[2] = GENSERKINS_DEFAULT_D3;
    kp->params.genser.a[3] = GENSERKINS_DEFAULT_A4; kp->params.genser.alpha[3] = GENSERKINS_DEFAULT_ALPHA4; kp->params.genser.d[3] = GENSERKINS_DEFAULT_D4;
    kp->params.genser.a[4] = GENSERKINS_DEFAULT_A5; kp->params.genser.alpha[4] = GENSERKINS_DEFAULT_ALPHA5; kp->params.genser.d[4] = GENSERKINS_DEFAULT_D5;
    kp->params.genser.a[5] = GENSERKINS_DEFAULT_A6; kp->params.genser.alpha[5] = GENSERKINS_DEFAULT_ALPHA6; kp->params.genser.d[5] = GENSERKINS_DEFAULT_D6;
    kp->params.genser.a[6] = kp->params.genser.a[7] = kp->params.genser.a[8] = 0.0;
    kp->params.genser.alpha[6] = kp->params.genser.alpha[7] = kp->params.genser.alpha[8] = 0.0;
    kp->params.genser.d[6] = kp->params.genser.d[7] = kp->params.genser.d[8] = 0.0;
    kp->params.genser.unrotate[0] = kp->params.genser.unrotate[1] = kp->params.genser.unrotate[2] = 0;
    kp->params.genser.unrotate[3] = kp->params.genser.unrotate[4] = kp->params.genser.unrotate[5] = 0;
    kp->params.genser.unrotate[6] = kp->params.genser.unrotate[7] = kp->params.genser.unrotate[8] = 0;
    return 0;
}
