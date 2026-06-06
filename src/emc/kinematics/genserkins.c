// genserkins — generalised serial-link kinematics (cmod version)
//
// Derived from work by Fred Proctor, adapted by Alex Joni.
// License: GPL Version 2
//
// Uses Denavit-Hartenberg parameters for joint/link description.
// Forward kins: direct pose build from DH chain.
// Inverse kins: Newton-Raphson iterative via Jacobian.

#include <math.h>
#include <string.h>
#include "gomc_env.h"
#include "switchkins.h"
#include "gotypes.h"
#include "gomath.h"

#define REQUIRED_COORDINATES "XYZABCUVW"

// ─── Constants ───

#define GENSER_MAX_JOINTS 6
#define GENSER_DEFAULT_MAX_ITERATIONS 100

// Override GO_REAL_EPSILON for better mm-system convergence
#undef  GO_REAL_EPSILON
#define GO_REAL_EPSILON (1e-6)

#define PI_2 GO_PI_2

// Default DH parameters (PUMA-like, per Craig)
#define DEFAULT_A1 0
#define DEFAULT_ALPHA1 0
#define DEFAULT_D1 0

#define DEFAULT_A2 0
#define DEFAULT_ALPHA2 (-PI_2)
#define DEFAULT_D2 0

#define DEFAULT_A3 300
#define DEFAULT_ALPHA3 0
#define DEFAULT_D3 70

#define DEFAULT_A4 50
#define DEFAULT_ALPHA4 (-PI_2)
#define DEFAULT_D4 400

#define DEFAULT_A5 0
#define DEFAULT_ALPHA5 PI_2
#define DEFAULT_D5 0

#define DEFAULT_A6 0
#define DEFAULT_ALPHA6 (-PI_2)
#define DEFAULT_D6 0

// ─── Module state ───

static const gomc_hal_t  *g_hal;
static int                g_comp_id;
static sk_map_t           g_map;
static sk_switch_t        g_sw;

typedef struct {
    go_link links[GENSER_MAX_JOINTS];
    int     link_num;
    int     iterations;
} genser_struct;

struct haldata {
    gomc_hal_u32_t   *max_iterations;
    gomc_hal_u32_t   *last_iterations;
    gomc_hal_float_t *a[GENSER_MAX_JOINTS];
    gomc_hal_float_t *alpha[GENSER_MAX_JOINTS];
    gomc_hal_float_t *d[GENSER_MAX_JOINTS];
    gomc_hal_s32_t   *unrotate[GENSER_MAX_JOINTS];
    genser_struct    *kins;
    go_pose          *pos;
};
static struct haldata *haldata;
static int total_joints;

#define A(i)     (*(haldata->a[i]))
#define ALPHA(i) (*(haldata->alpha[i]))
#define D(i)     (*(haldata->d[i]))
#define KINS_PTR (haldata->kins)

// ─── Initialize links from HAL pins ───

static void genser_kin_init(void)
{
    genser_struct *genser = KINS_PTR;
    for (int t = 0; t < GENSER_MAX_JOINTS; t++) {
        genser->links[t].u.dh.a     = A(t);
        genser->links[t].u.dh.alpha = ALPHA(t);
        genser->links[t].u.dh.d     = D(t);
        genser->links[t].u.dh.theta = 0;
        genser->links[t].type       = GO_LINK_DH;
        genser->links[t].quantity   = GO_QUANTITY_ANGLE;
    }
    genser->link_num = 6;
}

// ─── Forward Jacobian ───

static int compute_jfwd(go_link *link_params, int link_number,
                        go_matrix *Jfwd, go_pose *T_L_0)
{
    GO_MATRIX_DECLARE(Jv, Jvstg, 3, GENSER_MAX_JOINTS);
    GO_MATRIX_DECLARE(Jw, Jwstg, 3, GENSER_MAX_JOINTS);
    GO_MATRIX_DECLARE(R_i_ip1, R_i_ip1stg, 3, 3);
    GO_MATRIX_DECLARE(scratch, scratchstg, 3, GENSER_MAX_JOINTS);
    GO_MATRIX_DECLARE(R_inv, R_invstg, 3, 3);
    go_pose pose;
    go_quat quat;
    go_vector P_ip1_i[3];
    int row, col;

    go_matrix_init(Jv, Jvstg, 3, link_number);
    go_matrix_init(Jw, Jwstg, 3, link_number);
    go_matrix_init(R_i_ip1, R_i_ip1stg, 3, 3);
    go_matrix_init(scratch, scratchstg, 3, link_number);
    go_matrix_init(R_inv, R_invstg, 3, 3);

    Jv.el[0][0] = 0; Jv.el[1][0] = 0;
    Jv.el[2][0] = (GO_QUANTITY_LENGTH == link_params[0].quantity ? 1 : 0);
    Jw.el[0][0] = 0; Jw.el[1][0] = 0;
    Jw.el[2][0] = (GO_QUANTITY_ANGLE == link_params[0].quantity ? 1 : 0);

    if (GO_LINK_DH == link_params[0].type)
        go_dh_pose_convert(&link_params[0].u.dh, &pose);
    else if (GO_LINK_PP == link_params[0].type)
        pose = link_params[0].u.pp.pose;
    else
        return GO_RESULT_IMPL_ERROR;

    *T_L_0 = pose;

    for (col = 1; col < link_number; col++) {
        if (GO_LINK_DH == link_params[col].type)
            go_dh_pose_convert(&link_params[col].u.dh, &pose);
        else if (GO_LINK_PP == link_params[col].type)
            pose = link_params[col].u.pp.pose;
        else
            return GO_RESULT_IMPL_ERROR;

        go_cart_vector_convert(&pose.tran, P_ip1_i);
        go_quat_inv(&pose.rot, &quat);
        go_quat_matrix_convert(&quat, &R_i_ip1);

        go_matrix_vector_cross(&Jw, P_ip1_i, &scratch);
        go_matrix_matrix_add(&Jv, &scratch, &scratch);
        go_matrix_matrix_mult(&R_i_ip1, &scratch, &Jv);
        Jv.el[0][col] = 0; Jv.el[1][col] = 0;
        Jv.el[2][col] = (GO_QUANTITY_LENGTH == link_params[col].quantity ? 1 : 0);

        go_matrix_matrix_mult(&R_i_ip1, &Jw, &Jw);
        Jw.el[0][col] = 0; Jw.el[1][col] = 0;
        Jw.el[2][col] = (GO_QUANTITY_ANGLE == link_params[col].quantity ? 1 : 0);

        if (GO_LINK_DH == link_params[col].type)
            go_dh_pose_convert(&link_params[col].u.dh, &pose);
        else if (GO_LINK_PP == link_params[col].type)
            pose = link_params[col].u.pp.pose;
        else
            return GO_RESULT_IMPL_ERROR;
        go_pose_pose_mult(T_L_0, &pose, T_L_0);
    }

    go_quat_matrix_convert(&T_L_0->rot, &R_inv);
    go_matrix_matrix_mult(&R_inv, &Jv, &Jv);
    go_matrix_matrix_mult(&R_inv, &Jw, &Jw);

    for (row = 0; row < 6; row++) {
        for (col = 0; col < link_number; col++) {
            if (row < 3)
                Jfwd->el[row][col] = Jv.el[row][col];
            else
                Jfwd->el[row][col] = Jw.el[row - 3][col];
        }
    }

    return GO_RESULT_OK;
}

// ─── Inverse Jacobian ───

static int compute_jinv(go_matrix *Jfwd, go_matrix *Jinv)
{
    int retval;
    GO_MATRIX_DECLARE(JT, JTstg, GENSER_MAX_JOINTS, 6);

    if (Jfwd->rows == Jfwd->cols) {
        retval = go_matrix_inv(Jfwd, Jinv);
        if (GO_RESULT_OK != retval) return retval;
    } else if (Jfwd->rows < Jfwd->cols) {
        GO_MATRIX_DECLARE(JJT, JJTstg, 6, 6);
        go_matrix_init(JT, JTstg, Jfwd->cols, Jfwd->rows);
        go_matrix_init(JJT, JJTstg, Jfwd->rows, Jfwd->rows);
        go_matrix_transpose(Jfwd, &JT);
        go_matrix_matrix_mult(Jfwd, &JT, &JJT);
        retval = go_matrix_inv(&JJT, &JJT);
        if (GO_RESULT_OK != retval) return retval;
        go_matrix_matrix_mult(&JT, &JJT, Jinv);
    } else {
        GO_MATRIX_DECLARE(JTJ, JTJstg, GENSER_MAX_JOINTS, GENSER_MAX_JOINTS);
        go_matrix_init(JT, JTstg, Jfwd->cols, Jfwd->rows);
        go_matrix_init(JTJ, JTJstg, Jfwd->cols, Jfwd->cols);
        go_matrix_transpose(Jfwd, &JT);
        go_matrix_matrix_mult(&JT, Jfwd, &JTJ);
        retval = go_matrix_inv(&JTJ, &JTJ);
        if (GO_RESULT_OK != retval) return retval;
        go_matrix_matrix_mult(&JTJ, &JT, Jinv);
    }

    return GO_RESULT_OK;
}

// ─── Forward kinematics (direct DH chain) ───

static int genser_kin_fwd(const go_real *joints, go_pose *pos)
{
    genser_struct *genser = KINS_PTR;
    go_link linkout[GENSER_MAX_JOINTS];
    int link, retval;

    genser_kin_init();

    for (link = 0; link < genser->link_num; link++) {
        retval = go_link_joint_set(&genser->links[link], joints[link],
                                   &linkout[link]);
        if (GO_RESULT_OK != retval) return retval;
    }

    retval = go_link_pose_build(linkout, genser->link_num, pos);
    if (GO_RESULT_OK != retval) return retval;

    return GO_RESULT_OK;
}

static int32_t genser_forward(const double *joint, kins_pose_t *world)
{
    go_pose *pos;
    go_rpy rpy;
    go_real jcopy[GENSER_MAX_JOINTS];
    int ret;

    for (int i = 0; i < 6; i++) {
        jcopy[i] = joint[i] * M_PI / 180.0;
        if (i && *(haldata->unrotate[i]))
            jcopy[i] -= *(haldata->unrotate[i]) * jcopy[i - 1];
    }

    pos = haldata->pos;
    rpy.y = world->c * M_PI / 180.0;
    rpy.p = world->b * M_PI / 180.0;
    rpy.r = world->a * M_PI / 180.0;
    go_rpy_quat_convert(&rpy, &pos->rot);
    pos->tran.x = world->x;
    pos->tran.y = world->y;
    pos->tran.z = world->z;

    // Pass through joints 6,7,8 as u,v,w
    if (total_joints > 6) world->u = joint[6];
    if (total_joints > 7) world->v = joint[7];
    if (total_joints > 8) world->w = joint[8];

    ret = genser_kin_fwd(jcopy, pos);
    if (ret < 0) return ret;

    ret = go_quat_rpy_convert(&pos->rot, &rpy);
    if (ret < 0) return ret;

    world->x = pos->tran.x;
    world->y = pos->tran.y;
    world->z = pos->tran.z;
    world->a = rpy.r * 180.0 / M_PI;
    world->b = rpy.p * 180.0 / M_PI;
    world->c = rpy.y * 180.0 / M_PI;

    return 0;
}

// ─── Inverse kinematics (Newton-Raphson via Jacobian) ───

static int32_t genser_inverse(const kins_pose_t *world, double *joints)
{
    genser_struct *genser = KINS_PTR;
    GO_MATRIX_DECLARE(Jfwd, Jfwd_stg, 6, GENSER_MAX_JOINTS);
    GO_MATRIX_DECLARE(Jinv, Jinv_stg, GENSER_MAX_JOINTS, 6);
    go_pose T_L_0;
    go_real dvw[6];
    go_real jest[GENSER_MAX_JOINTS];
    go_real dj[GENSER_MAX_JOINTS];
    go_pose pest, pestinv, Tdelta;
    go_rpy rpy;
    go_rvec rvec;
    go_cart cart;
    go_link linkout[GENSER_MAX_JOINTS];
    int link, smalls, retval;

    genser_kin_init();

    rpy.y = world->c * M_PI / 180.0;
    rpy.p = world->b * M_PI / 180.0;
    rpy.r = world->a * M_PI / 180.0;
    go_rpy_quat_convert(&rpy, &haldata->pos->rot);
    haldata->pos->tran.x = world->x;
    haldata->pos->tran.y = world->y;
    haldata->pos->tran.z = world->z;

    go_matrix_init(Jfwd, Jfwd_stg, 6, genser->link_num);
    go_matrix_init(Jinv, Jinv_stg, genser->link_num, 6);

    for (link = 0; link < genser->link_num; link++)
        jest[link] = joints[link] * (M_PI / 180.0);

    for (genser->iterations = 0;
         genser->iterations < (int)*haldata->max_iterations;
         genser->iterations++) {
        *haldata->last_iterations = (uint32_t)genser->iterations;

        for (link = 0; link < genser->link_num; link++)
            go_link_joint_set(&genser->links[link], jest[link], &linkout[link]);

        retval = compute_jfwd(linkout, genser->link_num, &Jfwd, &T_L_0);
        if (GO_RESULT_OK != retval) return retval;

        retval = compute_jinv(&Jfwd, &Jinv);
        if (GO_RESULT_OK != retval) return retval;

        genser_kin_fwd(jest, &pest);
        go_pose_inv(&pest, &pestinv);
        go_pose_pose_mult(&pestinv, haldata->pos, &Tdelta);

        // Rotate translation differential back into frame {0}
        go_quat_cart_mult(&pest.rot, &Tdelta.tran, &cart);
        dvw[0] = cart.x;
        dvw[1] = cart.y;
        dvw[2] = cart.z;

        // Rotate rotation differential back into frame {0}
        go_quat_rvec_convert(&Tdelta.rot, &rvec);
        cart.x = rvec.x;
        cart.y = rvec.y;
        cart.z = rvec.z;
        go_quat_cart_mult(&pest.rot, &cart, &cart);
        dvw[3] = cart.x;
        dvw[4] = cart.y;
        dvw[5] = cart.z;

        go_matrix_vector_mult(&Jinv, dvw, dj);

        // Pass through joints 6,7,8 as u,v,w
        if (total_joints > 6) joints[6] = world->u;
        if (total_joints > 7) joints[7] = world->v;
        if (total_joints > 8) joints[8] = world->w;

        // Check convergence
        for (link = 0, smalls = 0; link < genser->link_num; link++) {
            if (GO_QUANTITY_LENGTH == linkout[link].quantity) {
                if (GO_TRAN_SMALL(dj[link])) smalls++;
            } else {
                if (GO_ROT_SMALL(dj[link])) smalls++;
            }
        }
        if (smalls == genser->link_num) {
            for (link = 0; link < genser->link_num; link++) {
                joints[link] = jest[link] * 180.0 / M_PI;
                if (link && *(haldata->unrotate[link]))
                    joints[link] += *(haldata->unrotate[link]) * joints[link - 1];
            }
            return GO_RESULT_OK;
        }

        for (link = 0; link < genser->link_num; link++)
            jest[link] += dj[link];
    }

    return GO_RESULT_ERROR;
}

// ─── Dispatch (switchkins) ───

static int32_t dispatch_forward(
    void *ctx,
    const double joints[KINS_MAX_JOINTS], kins_pose_t *pos,
    uint64_t fflags, uint64_t *iflags)
{
    (void)ctx;
    (void)fflags; (void)iflags;
    switch (g_sw.current_type) {
    case 0:  return genser_forward(joints, pos);
    case 1:  sk_identity_forward(&g_map, joints, pos);
             return 0;
    default: return -1;
    }
}

static int32_t dispatch_inverse(
    void *ctx,
    const kins_pose_t *pos, double joints[KINS_MAX_JOINTS],
    uint64_t iflags, uint64_t *fflags)
{
    (void)ctx;
    (void)iflags; (void)fflags;
    switch (g_sw.current_type) {
    case 0:  return genser_inverse(pos, joints);
    case 1:  sk_identity_inverse(&g_map, pos, joints);
             return 0;
    default: return -1;
    }
}

static kins_kinematics_type_t dispatch_type(void *ctx) {
    (void)ctx;
    return KINS_BOTH;
}
static int32_t dispatch_switchable(void *ctx) { (void)ctx; return 1; }
static int32_t dispatch_switch(void *ctx, int32_t t)
    { (void)ctx; return sk_switch_to(&g_sw, t); }

static kins_callbacks_t genser_callbacks = {
    .ctx = NULL,
    .forward    = dispatch_forward,
    .inverse    = dispatch_inverse,
    .type       = dispatch_type,
    .switchable = dispatch_switchable,
    .switch_    = dispatch_switch,
};

// ─── cmod lifecycle ───

static cmod_t genser_cmod;

static void genser_destroy(cmod_t *self)
{
    (void)self;
    if (g_hal && g_comp_id > 0) g_hal->exit(g_hal->ctx, g_comp_id);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    if (!env->hal) {
        gomc_log_errorf(env->log, name, "HAL API not available");
        return -1;
    }
    g_hal = env->hal;

    // Parse coordinates
    const char *coordinates = REQUIRED_COORDINATES;
    for (int i = 0; i < argc; i++)
        if (strncmp(argv[i], "coordinates=", 12) == 0)
            coordinates = argv[i] + 12;
    if (env->ini) {
        const char *v = env->ini->get(env->ini->ctx, "KINS", "COORDINATES");
        if (v) coordinates = v;
    }

    if (sk_map_coordinates(&g_map, coordinates, 0) < 0) {
        gomc_log_errorf(env->log, name, "bad coordinates: %s", coordinates);
        return -1;
    }
    total_joints = g_map.num_joints;

    g_comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                               GOMC_HAL_COMP_REALTIME);
    if (g_comp_id < 0) return g_comp_id;

    haldata = env->hal->malloc(env->hal->ctx, sizeof(struct haldata));
    if (!haldata) { g_hal->exit(g_hal->ctx, g_comp_id); return -1; }

    // Allocate genser_struct and go_pose in HAL shared memory
    haldata->kins = env->hal->malloc(env->hal->ctx, sizeof(genser_struct));
    haldata->pos  = env->hal->malloc(env->hal->ctx, sizeof(go_pose));
    if (!haldata->kins || !haldata->pos) goto fail;

    int rc = 0;

    // DH parameter pins (6 joints)
    for (int i = 0; i < 6; i++) {
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &haldata->a[i], g_comp_id, "%s.A-%d", name, i);
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &haldata->alpha[i], g_comp_id, "%s.ALPHA-%d", name, i);
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &haldata->d[i], g_comp_id, "%s.D-%d", name, i);
        rc |= gomc_hal_pin_s32_newf(env->hal, GOMC_HAL_IN,
            &haldata->unrotate[i], g_comp_id, "%s.unrotate-%d", name, i);
        if (rc < 0) goto fail;
        *haldata->a[i] = 0;
        *haldata->alpha[i] = 0;
        *haldata->d[i] = 0;
        *haldata->unrotate[i] = 0;
    }

    // Iteration control pins
    rc |= gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_OUT,
        &haldata->last_iterations, g_comp_id, "%s.last-iterations", name);
    rc |= gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IN,
        &haldata->max_iterations, g_comp_id, "%s.max-iterations", name);
    if (rc < 0) goto fail;

    *haldata->last_iterations = 0;
    *haldata->max_iterations = GENSER_DEFAULT_MAX_ITERATIONS;

    // Set default DH parameters
    A(0) = DEFAULT_A1;      ALPHA(0) = DEFAULT_ALPHA1;  D(0) = DEFAULT_D1;
    A(1) = DEFAULT_A2;      ALPHA(1) = DEFAULT_ALPHA2;  D(1) = DEFAULT_D2;
    A(2) = DEFAULT_A3;      ALPHA(2) = DEFAULT_ALPHA3;  D(2) = DEFAULT_D3;
    A(3) = DEFAULT_A4;      ALPHA(3) = DEFAULT_ALPHA4;  D(3) = DEFAULT_D4;
    A(4) = DEFAULT_A5;      ALPHA(4) = DEFAULT_ALPHA5;  D(4) = DEFAULT_D5;
    A(5) = DEFAULT_A6;      ALPHA(5) = DEFAULT_ALPHA6;  D(5) = DEFAULT_D6;

    // Switch pins
    rc = sk_create_switch_pins(env->hal, g_comp_id, &g_sw);
    if (rc < 0) goto fail;

    env->hal->ready(env->hal->ctx, g_comp_id);

    rc = kins_api_register(env->api, name, &genser_callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name, "kins_api_register failed: %d", rc);
        goto fail;
    }

    genser_cmod.Destroy = genser_destroy;
    *out = &genser_cmod;
    return 0;

fail:
    g_hal->exit(g_hal->ctx, g_comp_id);
    return -1;
}
