/*
 * Checks of the posemath parallel and anti-parallel vector tests.
 *
 * Salvaged from the retired meson-only unit_tests/tp/test_blendmath.c
 * (originally by Robert W. Ellenberg). Loaded after tpmod, which exports
 * the two functions; a failed check fails the load, and a failed load
 * fails the test.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 */

#include <rtapi.h>
#include <rtapi_app.h>
#include <rtapi_errno.h>
#include <rtapi_math.h>
#include <hal.h>
#include <posemath.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("posemath parallel check");

/* the tolerance the planner's blends use, ANGLE_EPSILON */
#define ANGLE_EPSILON 1e-6
#define ANGLE_EPSILON_SQ (ANGLE_EPSILON * ANGLE_EPSILON)

static int comp_id = -1;
static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        rtapi_print_msg(RTAPI_MSG_ERR, "parallelcheck: FAIL line %d: %s\n", \
                        __LINE__, #cond); \
        failures++; \
    } \
} while (0)

#define CHECK_FALSE(cond) CHECK(!(cond))

static void test_pmCartCartParallel_numerical(void)
{
    PmCartesian u0 = {1, 0, 0};
    PmCartesian u_close = {cos(ANGLE_EPSILON), sin(ANGLE_EPSILON), 0};
    int k;

    CHECK(pmCartCartParallel(&u0, &u0, ANGLE_EPSILON_SQ));
    CHECK_FALSE(pmCartCartParallel(&u0, &u_close, 0.0));

    /* Test that the tolerance makes sense */
    CHECK_FALSE(pmCartCartParallel(&u0, &u_close, 0.5 * ANGLE_EPSILON_SQ));
    CHECK(pmCartCartParallel(&u0, &u_close, 1.5 * ANGLE_EPSILON_SQ));

    /* Try a bunch of other angles including anti-parallel */
    for (k = 1; k <= 7; ++k) {
        PmCartesian u_far = {cos(PM_PI_4 * k), sin(PM_PI_4 * k), 0};
        CHECK_FALSE(pmCartCartParallel(&u0, &u_far, ANGLE_EPSILON_SQ));
    }
}

static void test_pmCartCartAntiParallel_numerical(void)
{
    PmCartesian u0 = {1, 0, 0};
    PmCartesian u_close = {-cos(ANGLE_EPSILON), sin(ANGLE_EPSILON), 0};
    PmCartesian u_opposite = {-1, 0, 0};
    int k;

    CHECK(pmCartCartAntiParallel(&u0, &u_opposite, ANGLE_EPSILON_SQ));
    CHECK_FALSE(pmCartCartAntiParallel(&u0, &u_close, 0.0));

    /* Test that the tolerance makes sense */
    CHECK_FALSE(pmCartCartAntiParallel(&u0, &u_close, 0.5 * ANGLE_EPSILON_SQ));
    CHECK(pmCartCartAntiParallel(&u0, &u_close, 1.5 * ANGLE_EPSILON_SQ));

    /* Try a bunch of other angles including anti-parallel */
    for (k = 1; k <= 7; ++k) {
        PmCartesian u_far = {-cos(PM_PI_4 * k), sin(PM_PI_4 * k), 0};
        CHECK_FALSE(pmCartCartAntiParallel(&u0, &u_far, ANGLE_EPSILON_SQ));
    }
}

int rtapi_app_main(void)
{
    comp_id = hal_init("parallelcheck");
    if (comp_id < 0) { return comp_id; }

    test_pmCartCartParallel_numerical();
    test_pmCartCartAntiParallel_numerical();

    if (failures) {
        rtapi_print_msg(RTAPI_MSG_ERR, "parallelcheck: %d check(s) failed\n",
                        failures);
        hal_exit(comp_id);
        return -ERANGE;
    }

    rtapi_print("parallelcheck: all parallel checks passed\n");
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
