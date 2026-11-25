/*
 * Checks of the posemath parallel and anti-parallel vector tests.
 *
 * Salvaged from the retired meson-only unit_tests/tp/test_blendmath.c
 * (originally by Robert W. Ellenberg). A plain program linked with
 * libposemath, which needs neither RTAPI nor ULAPI.
 */

#include <math.h>
#include <stdio.h>
#include <posemath.h>

/* the tolerance the planner's blends use, ANGLE_EPSILON */
#define ANGLE_EPSILON 1e-6
#define ANGLE_EPSILON_SQ (ANGLE_EPSILON * ANGLE_EPSILON)

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

int main(void)
{
    test_pmCartCartParallel_numerical();
    test_pmCartCartAntiParallel_numerical();

    if (failures) {
        printf("%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("all parallel checks passed\n");
    return 0;
}
