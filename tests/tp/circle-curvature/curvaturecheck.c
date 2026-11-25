/* Check the production TP radius estimate after tpmod has been loaded. */
#include <rtapi.h>
#include <rtapi_app.h>
#include <rtapi_math.h>
#include <hal.h>
#include <posemath.h>

MODULE_LICENSE("GPL");

extern double pmCircleEffectiveMinRadius(const PmCircle *circle);

static int comp_id = -1;

static int check_radius(const char *name, const PmCircle *circle, double expected)
{
    double actual = pmCircleEffectiveMinRadius(circle);
    if (!isfinite(actual) || fabs(actual - expected) > 1e-9) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: radius %.12g, expected %.12g\n",
                        name, actual, expected);
        return -1;
    }
    return 0;
}

int rtapi_app_main(void)
{
    const double pi = 3.14159265358979323846;
    PmCircle circle = {0};
    circle.radius = 10.0;
    circle.angle = 2.0 * pi;
    circle.rHelix.z = 20.0;

    comp_id = hal_init("curvaturecheck");
    if (comp_id < 0) {
        return comp_id;
    }

    /* R + (rise / sweep)^2 / R, independent of command subdivision. */
    double expected = 10.0 + (20.0 / (2.0 * pi)) * (20.0 / (2.0 * pi)) / 10.0;
    if (check_radius("one turn", &circle, expected)) {
        goto fail;
    }
    circle.angle = pi / 2.0;
    circle.rHelix.z = 5.0;
    if (check_radius("quarter turn", &circle, expected)) {
        goto fail;
    }
    circle.rHelix.z = -5.0;
    if (check_radius("reverse rise", &circle, expected)) {
        goto fail;
    }

    /* Below one radian, the old radius is the tighter limit. */
    circle.angle = 0.5;
    circle.rHelix.z = 1.0;
    if (check_radius("short sweep", &circle, 10.1)) {
        goto fail;
    }

    hal_ready(comp_id);
    return 0;

fail:
    hal_exit(comp_id);
    return -1;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}
