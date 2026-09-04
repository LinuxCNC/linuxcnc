/*
 * twpcheck: the realtime half of the tilted work plane cross-check.
 *
 * Loaded after a kinematics module, it answers requests made over HAL
 * pins: for the joint values on its inputs it reports the module's tool
 * frame and work frame, and for the tool axis (and optionally tool x)
 * on its inputs it reports what kinematicsToolFrameInverse() finds, the
 * joint sets and the spin about the tool each needs, with the joints
 * named on the held pin kept where they are.  check.py drives it and
 * holds the python maths the answers are compared with.
 *
 * A request is made by raising the request pin; done follows it when
 * the answers are on the pins.
 *
 * Module parameters
 *   joints    joint count the module was loaded for
 *   ktype     switchkins type to select first, 0 for none
 */
#include <rtapi.h>
#include <rtapi_app.h>
#include <hal.h>
#include <emcmotcfg.h>
#include <posemath.h>
#include <kinematics.h>

MODULE_LICENSE("GPL");

static int joints = 6;
RTAPI_MP_INT(joints, "joint count the module under test was loaded for");
static int ktype = 0;
RTAPI_MP_INT(ktype, "switchkins type to select first");

static int comp_id = -1;

#define NSOL TOOL_FRAME_MAX_SOLUTIONS

static struct {
    hal_real_t j[EMCMOT_MAX_JOINTS];
    hal_real_t axis[3];
    hal_real_t xdir[3];
    hal_bool_t have_x;
    hal_uint_t held;            /* bit per joint the inverse may not move */
    hal_uint_t request;
    hal_uint_t done;
    hal_real_t tool[3][3];      /* [row][column], columns are the frame's axes */
    hal_real_t work[3][3];
    hal_sint_t frame_rc;
    hal_sint_t nsol;
    hal_real_t sol[NSOL][EMCMOT_MAX_JOINTS];
    hal_real_t spin[NSOL];
    hal_sint_t free[NSOL];
} *pins;

static void publish(hal_real_t out[3][3], const PmRotationMatrix *m)
{
    hal_set_real(out[0][0], m->x.x); hal_set_real(out[0][1], m->y.x); hal_set_real(out[0][2], m->z.x);
    hal_set_real(out[1][0], m->x.y); hal_set_real(out[1][1], m->y.y); hal_set_real(out[1][2], m->z.y);
    hal_set_real(out[2][0], m->x.z); hal_set_real(out[2][1], m->y.z); hal_set_real(out[2][2], m->z.z);
}

static void update(void *arg, long period)
{
    KINEMATICS_FORWARD_FLAGS ff = 0;
    PmRotationMatrix tool, work;
    PmCartesian axis, xdir;
    double j[EMCMOT_MAX_JOINTS];
    double sols[NSOL * EMCMOT_MAX_JOINTS];  /* rows of joints doubles, packed */
    double spin[NSOL];
    int freed[NSOL];
    int i, k, n, rc;
    (void)arg;
    (void)period;

    if (hal_get_ui32(pins->request) == hal_get_ui32(pins->done)) { return; }

    for (i = 0; i < EMCMOT_MAX_JOINTS; i++) {
        j[i] = i < joints ? hal_get_real(pins->j[i]) : 0.0;
    }

    rc = kinematicsToolFrame(j, &tool, &ff);
    if (!rc) { rc = kinematicsWorkFrame(j, &work, &ff); }
    hal_set_si32(pins->frame_rc, rc);
    if (!rc) {
        publish(pins->tool, &tool);
        publish(pins->work, &work);
    }

    axis.x = hal_get_real(pins->axis[0]);
    axis.y = hal_get_real(pins->axis[1]);
    axis.z = hal_get_real(pins->axis[2]);
    xdir.x = hal_get_real(pins->xdir[0]);
    xdir.y = hal_get_real(pins->xdir[1]);
    xdir.z = hal_get_real(pins->xdir[2]);
    n = -1;
    if (axis.x != 0 || axis.y != 0 || axis.z != 0) {
        n = kinematicsToolFrameInverse(&axis, hal_get_bool(pins->have_x) ? &xdir : NULL,
                                       j, hal_get_ui32(pins->held), sols, NSOL,
                                       freed, spin);
    }
    hal_set_si32(pins->nsol, n);
    for (k = 0; k < NSOL; k++) {
        for (i = 0; i < joints; i++) {
            hal_set_real(pins->sol[k][i], k < n ? sols[k * joints + i] : 0.0);
        }
        hal_set_real(pins->spin[k], k < n ? spin[k] : 0.0);
        hal_set_si32(pins->free[k], k < n ? freed[k] : 0);
    }

    hal_set_ui32(pins->done, hal_get_ui32(pins->request));
}

int rtapi_app_main(void)
{
    static const char letter[3] = { 'x', 'y', 'z' };
    int i, k, r, res = 0;

    if (joints < 1 || joints > EMCMOT_MAX_JOINTS) { return -1; }

    comp_id = hal_init("twpcheck");
    if (comp_id < 0) { return comp_id; }

    pins = hal_malloc(sizeof(*pins));
    if (!pins) { hal_exit(comp_id); return -1; }

    for (i = 0; i < joints; i++) {
        res += hal_pin_new_real(comp_id, HAL_IN, &pins->j[i], 0.0, "twpcheck.j-%d", i);
    }
    for (i = 0; i < 3; i++) {
        res += hal_pin_new_real(comp_id, HAL_IN, &pins->axis[i], 0.0, "twpcheck.axis-%c", letter[i]);
        res += hal_pin_new_real(comp_id, HAL_IN, &pins->xdir[i], 0.0, "twpcheck.xdir-%c", letter[i]);
    }
    res += hal_pin_new_bool(comp_id, HAL_IN, &pins->have_x, 0, "twpcheck.have-x");
    res += hal_pin_new_ui32(comp_id, HAL_IN, &pins->held, 0, "twpcheck.held");
    res += hal_pin_new_ui32(comp_id, HAL_IN, &pins->request, 0, "twpcheck.request");
    res += hal_pin_new_ui32(comp_id, HAL_OUT, &pins->done, 0, "twpcheck.done");
    for (r = 0; r < 3; r++) {
        for (i = 0; i < 3; i++) {
            res += hal_pin_new_real(comp_id, HAL_OUT, &pins->tool[r][i], 0.0, "twpcheck.tool-%d%d", r, i);
            res += hal_pin_new_real(comp_id, HAL_OUT, &pins->work[r][i], 0.0, "twpcheck.work-%d%d", r, i);
        }
    }
    res += hal_pin_new_si32(comp_id, HAL_OUT, &pins->frame_rc, 0, "twpcheck.frame-rc");
    res += hal_pin_new_si32(comp_id, HAL_OUT, &pins->nsol, 0, "twpcheck.nsol");
    for (k = 0; k < NSOL; k++) {
        for (i = 0; i < joints; i++) {
            res += hal_pin_new_real(comp_id, HAL_OUT, &pins->sol[k][i], 0.0, "twpcheck.sol-%d-%d", k, i);
        }
        res += hal_pin_new_real(comp_id, HAL_OUT, &pins->spin[k], 0.0, "twpcheck.spin-%d", k);
        res += hal_pin_new_si32(comp_id, HAL_OUT, &pins->free[k], 0, "twpcheck.free-%d", k);
    }
    if (res) { hal_exit(comp_id); return -1; }

    if (ktype > 0 && kinematicsSwitchable()) {
        if (kinematicsSwitch(ktype)) { hal_exit(comp_id); return -1; }
    }

    if (hal_export_funct("twpcheck", update, NULL, 1, 0, comp_id)) {
        hal_exit(comp_id);
        return -1;
    }
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
