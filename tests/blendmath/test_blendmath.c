/*
 * Unit tests for blendmath utility functions.
 *
 * Salvaged from the retired meson-only unit_tests/tp/test_blendmath.c
 * (originally by Robert W. Ellenberg) and converted from the greatest
 * framework to plain C so it can build and run as a runtest.
 */

#include <math.h>
#include <stdio.h>
#include <stdarg.h>

#include <rtapi.h>
#include <posemath.h>
#include "tp/blendmath.h"
#include "tp/tp_types.h"

/* kludge: satisfy rtapi_print_msg references without an rtapi app */
void rtapi_print_msg(msg_level_t level, const char *fmt, ...)
{
    va_list args;

    (void)level;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

#define CHECK_FALSE(cond) CHECK(!(cond))

static void test_pmCartCartParallel_numerical(void)
{
    PmCartesian u0 = {1, 0, 0};
    PmCartesian u_close = {cos(TP_ANGLE_EPSILON), sin(TP_ANGLE_EPSILON), 0};
    int k;

    CHECK(pmCartCartParallel(&u0, &u0, TP_ANGLE_EPSILON_SQ));
    CHECK_FALSE(pmCartCartParallel(&u0, &u_close, 0.0));

    /* Test that the tolerance makes sense */
    CHECK_FALSE(pmCartCartParallel(&u0, &u_close, 0.5 * TP_ANGLE_EPSILON_SQ));
    CHECK(pmCartCartParallel(&u0, &u_close, 1.5 * TP_ANGLE_EPSILON_SQ));

    /* Try a bunch of other angles including anti-parallel */
    for (k = 1; k <= 7; ++k) {
        PmCartesian u_far = {cos(PM_PI_4 * k), sin(PM_PI_4 * k), 0};
        CHECK_FALSE(pmCartCartParallel(&u0, &u_far, TP_ANGLE_EPSILON_SQ));
    }
}

static void test_pmCartCartAntiParallel_numerical(void)
{
    PmCartesian u0 = {1, 0, 0};
    PmCartesian u_close = {-cos(TP_ANGLE_EPSILON), sin(TP_ANGLE_EPSILON), 0};
    PmCartesian u_opposite;
    int k;

    pmCartScalMult(&u0, -1.0, &u_opposite);
    CHECK(pmCartCartAntiParallel(&u0, &u_opposite, TP_ANGLE_EPSILON_SQ));
    CHECK_FALSE(pmCartCartAntiParallel(&u0, &u_close, 0.0));

    /* Test that the tolerance makes sense */
    CHECK_FALSE(pmCartCartAntiParallel(&u0, &u_close, 0.5 * TP_ANGLE_EPSILON_SQ));
    CHECK(pmCartCartAntiParallel(&u0, &u_close, 1.5 * TP_ANGLE_EPSILON_SQ));

    /* Try a bunch of other angles including anti-parallel */
    for (k = 1; k <= 7; ++k) {
        PmCartesian u_far = {-cos(PM_PI_4 * k), sin(PM_PI_4 * k), 0};
        CHECK_FALSE(pmCartCartAntiParallel(&u0, &u_far, TP_ANGLE_EPSILON_SQ));
    }
}

int main(void)
{
    test_pmCartCartParallel_numerical();
    test_pmCartCartAntiParallel_numerical();

    if (failures) {
        printf("%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("all blendmath checks passed\n");
    return 0;
}
