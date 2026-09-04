/*
 * paritycheck: the realtime half of the parameter block parity test.
 *
 * Loaded after a kinematics module, it evaluates the module through the
 * classic entry points once, at load, and publishes the answers on HAL
 * pins: the forward pose, the inverse joints and the Jacobian.  check.py
 * then evaluates the same module through the non-realtime loader, which
 * goes through kinsDescribe() and the parameter block, and compares.
 *
 * Two flows.  With frompose=0 the input is a joint set: the pose is the
 * forward of it, the joints published are the inverse of that pose, and
 * the Jacobian is taken there.  With frompose=1 the input is a pose, for
 * the parallel machines whose forward wants a seed: the joints are its
 * inverse, the forward is run from the pose as seed, and the Jacobian is
 * taken there.
 *
 * Module parameters
 *   joints    joint count the module was loaded for
 *   ktype     switchkins type to select first, 0 for none
 *   frompose  0 or 1, as above
 *   pose      up to nine integers, the pose (frompose=1) or the forward
 *             seed (frompose=0)
 *   jnt       up to sixteen integers, the joint set (frompose=0) or the
 *             inverse seed (frompose=1)
 */
#include <rtapi.h>
#include <rtapi_app.h>
#include <hal.h>
#include <emcmotcfg.h>
#include <kinematics.h>

MODULE_LICENSE("GPL");

static int joints = 3;
RTAPI_MP_INT(joints, "joint count the module under test was loaded for");
static int ktype = 0;
RTAPI_MP_INT(ktype, "switchkins type to select first");
static int frompose = 0;
RTAPI_MP_INT(frompose, "1 to take the pose as the input");
static int pose[EMCMOT_MAX_AXIS] = { 0 };
RTAPI_MP_ARRAY_INT(pose, EMCMOT_MAX_AXIS, "pose, x y z a b c u v w");
static int jnt[EMCMOT_MAX_JOINTS] = { 10, 20, 30, 40, 50, 60, 70, 80, 90 };
RTAPI_MP_ARRAY_INT(jnt, EMCMOT_MAX_JOINTS, "joint values, from joint 0");

static int comp_id = -1;

static struct {
    hal_real_t fwd[EMCMOT_MAX_AXIS];
    hal_real_t inv[EMCMOT_MAX_JOINTS];
    hal_real_t jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS];
    hal_sint_t rc_fwd;
    hal_sint_t rc_inv;
    hal_sint_t rc_jac;
} *pins;

static const char letter[EMCMOT_MAX_AXIS] = { 'x','y','z','a','b','c','u','v','w' };

static double *coord(EmcPose *p, int a)
{
    switch (a) {
    case 0: return &p->tran.x;
    case 1: return &p->tran.y;
    case 2: return &p->tran.z;
    case 3: return &p->a;
    case 4: return &p->b;
    case 5: return &p->c;
    case 6: return &p->u;
    case 7: return &p->v;
    default: return &p->w;
    }
}

int rtapi_app_main(void)
{
    KINEMATICS_FORWARD_FLAGS fflags = 0;
    KINEMATICS_INVERSE_FLAGS iflags = 0;
    double q[EMCMOT_MAX_JOINTS], qi[EMCMOT_MAX_JOINTS];
    double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS];
    EmcPose P, F, seed;
    int a, j, res = 0;

    if (joints < 1 || joints > EMCMOT_MAX_JOINTS) { return -1; }

    comp_id = hal_init("paritycheck");
    if (comp_id < 0) { return comp_id; }

    pins = hal_malloc(sizeof(*pins));
    if (!pins) { hal_exit(comp_id); return -1; }

    for (a = 0; a < EMCMOT_MAX_AXIS; a++) {
        res += hal_pin_new_real(comp_id, HAL_OUT, &pins->fwd[a], 0.0,
                                "paritycheck.fwd-%c", letter[a]);
    }
    for (j = 0; j < joints; j++) {
        res += hal_pin_new_real(comp_id, HAL_OUT, &pins->inv[j], 0.0,
                                "paritycheck.inv-%d", j);
        for (a = 0; a < EMCMOT_MAX_AXIS; a++) {
            res += hal_pin_new_real(comp_id, HAL_OUT, &pins->jac[j][a], 0.0,
                                    "paritycheck.jac-%d-%c", j, letter[a]);
        }
    }
    res += hal_pin_new_si32(comp_id, HAL_OUT, &pins->rc_fwd, 0, "paritycheck.rc-fwd");
    res += hal_pin_new_si32(comp_id, HAL_OUT, &pins->rc_inv, 0, "paritycheck.rc-inv");
    res += hal_pin_new_si32(comp_id, HAL_OUT, &pins->rc_jac, 0, "paritycheck.rc-jac");
    if (res) { hal_exit(comp_id); return -1; }

    if (ktype > 0 && kinematicsSwitchable()) {
        if (kinematicsSwitch(ktype)) { hal_exit(comp_id); return -1; }
    }

    for (a = 0; a < EMCMOT_MAX_AXIS; a++) { *coord(&seed, a) = pose[a]; }
    for (j = 0; j < EMCMOT_MAX_JOINTS; j++) { q[j] = jnt[j]; qi[j] = jnt[j]; }

    // a switchable module's first forward after load restarts from the
    // pose it saved, which is nothing yet; take that call here so the one
    // measured starts from the seed like the loader's does
    F = seed;
    kinematicsForward(q, &F, &fflags, &iflags);
    fflags = 0; iflags = 0;

    if (frompose) {
        P = seed;
        hal_set_si32(pins->rc_inv, kinematicsInverse(&P, qi, &iflags, &fflags));
        F = seed;
        fflags = 0; iflags = 0;
        hal_set_si32(pins->rc_fwd, kinematicsForward(qi, &F, &fflags, &iflags));
        iflags = 0;
        hal_set_si32(pins->rc_jac, kinematicsJacobian(qi, &P, jac, &iflags));
    } else {
        F = seed;
        hal_set_si32(pins->rc_fwd, kinematicsForward(q, &F, &fflags, &iflags));
        iflags = 0; fflags = 0;
        hal_set_si32(pins->rc_inv, kinematicsInverse(&F, qi, &iflags, &fflags));
        iflags = 0;
        hal_set_si32(pins->rc_jac, kinematicsJacobian(qi, &F, jac, &iflags));
    }

    for (a = 0; a < EMCMOT_MAX_AXIS; a++) { hal_set_real(pins->fwd[a], *coord(&F, a)); }
    for (j = 0; j < joints; j++) {
        hal_set_real(pins->inv[j], qi[j]);
        for (a = 0; a < EMCMOT_MAX_AXIS; a++) { hal_set_real(pins->jac[j][a], jac[j][a]); }
    }

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
