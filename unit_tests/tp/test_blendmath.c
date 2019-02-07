#include "tp_debug.h"
#include "greatest.h"
#include "blendmath.h"
#include "tp_types.h"
#include "math.h"
#include "rtapi.h"
#include "joint_util.h"
#include "motion_debug.h"
struct emcmot_debug_t emcmotDebug = {0};

/* Expand to all the definitions that need to be in
   the test runner's main file. */
GREATEST_MAIN_DEFS();

// KLUDGE fix link error the ugly way
void rtapi_print_msg(msg_level_t level, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void rtapi_print(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

TEST pmCartCartParallel_numerical() {

    PmCartesian u0 = {1,0,0};
    PmCartesian u_close = {cos(TP_ANGLE_EPSILON), sin(TP_ANGLE_EPSILON), 0};

    ASSERT(pmCartCartParallel(&u0, &u0, TP_ANGLE_EPSILON_SQ));
    ASSERT_FALSE(pmCartCartParallel(&u0, &u_close, 0.0));

    // Test that the tolerance makes sense
    ASSERT_FALSE(pmCartCartParallel(&u0, &u_close, 0.5*TP_ANGLE_EPSILON_SQ));
    ASSERT(pmCartCartParallel(&u0, &u_close, 1.5*TP_ANGLE_EPSILON_SQ));

    // Try a bunch of other angles including anti-parallel
    for (double k=1; k <= 7; ++k) {
        PmCartesian u_far = {cos(PM_PI_4 * k), sin(PM_PI_4 * k), 0};
        ASSERT_FALSE(pmCartCartParallel(&u0, &u_far, TP_ANGLE_EPSILON_SQ));
    }

    PASS();
}

TEST pmCartCartAntiParallel_numerical() {

    PmCartesian u0 = {1,0,0};
    PmCartesian u_close = {-cos(TP_ANGLE_EPSILON), sin(TP_ANGLE_EPSILON), 0};

    PmCartesian u_opposite;
    pmCartScalMult(&u0, -1.0, &u_opposite);
    ASSERT(pmCartCartAntiParallel(&u0, &u_opposite, TP_ANGLE_EPSILON_SQ));
    ASSERT_FALSE(pmCartCartAntiParallel(&u0, &u_close, 0.0));

    // Test that the tolerance makes sense
    ASSERT_FALSE(pmCartCartAntiParallel(&u0, &u_close, 0.5*TP_ANGLE_EPSILON_SQ));
    ASSERT(pmCartCartAntiParallel(&u0, &u_close, 1.5*TP_ANGLE_EPSILON_SQ));

    // Try a bunch of other angles including anti-parallel
    for (double k=1; k <= 7; ++k) {
        PmCartesian u_far = {-cos(PM_PI_4 * k), sin(PM_PI_4 * k), 0};
        ASSERT_FALSE(pmCartCartAntiParallel(&u0, &u_far, TP_ANGLE_EPSILON_SQ));
    }

    PASS();
}

SUITE(blendmath) {
    RUN_TEST(pmCartCartParallel_numerical);
    RUN_TEST(pmCartCartAntiParallel_numerical);
}


TEST findMinNonZeroComplete() {
    PmCartesian normal_bounds = {3.0,1.0,2.0};

    double min_bound = findMinNonZero(&normal_bounds);
    ASSERT_EQ(min_bound, 1.0);
    PASS();
}

TEST findMinNonZeroPartial() {
    PmCartesian partial_bounds = {3.0,0.0,2.0};

    double min_bound = findMinNonZero(&partial_bounds);
    ASSERT_EQ(min_bound, 2.0);
    PASS();
}

SUITE(joint_utils) {
    RUN_TEST(findMinNonZeroComplete);
    RUN_TEST(findMinNonZeroPartial);
}


static const double a_max_mock = 10.0;
static const double v_max_mock = 2.0;
static const double cycle_time = 0.001;
static const double target_dist_mock = 2.0;

static const double SMALL_DISTANCE = 1e-10;
static const double SMALL_VELOCITY = 1e-12;

TEST checkEndCondition_zero_velocity()
{
    EndCondition ec_zero = checkEndCondition(cycle_time,
                                   0.0,
                                   target_dist_mock,
                                   0.0,
                                   0.0,
                                   a_max_mock
                                   );
    // When both initial and final velocity are zero, assume that at least one cycle is left
    ASSERT(ec_zero.dt >= cycle_time);

    EndCondition ec_small = checkEndCondition(cycle_time,
                                   0.0,
                                   target_dist_mock,
                                   SMALL_VELOCITY,
                                   0.0,
                                   a_max_mock
                                   );
    // when "average" velocity is very small and distance to go is large, at least one cycle is left
    ASSERT(ec_small.dt >= cycle_time);
    PASS();
}
TEST checkEndCondition_small_velocity()
{
    EndCondition ec_small_close = checkEndCondition(cycle_time,
                                   target_dist_mock - SMALL_VELOCITY*cycle_time / 2.0,
                                   target_dist_mock,
                                   SMALL_VELOCITY,
                                   0.0,
                                   a_max_mock
                                   );
    ASSERT(ec_small_close.dt < cycle_time);

    EndCondition ec_small_far = checkEndCondition(cycle_time,
                                   target_dist_mock - SMALL_VELOCITY*cycle_time * 2.0,
                                   target_dist_mock,
                                   SMALL_VELOCITY,
                                   0.0,
                                   a_max_mock
                                   );
    // when "average" velocity is very small and distance to go is large, at least one cycle is left
    ASSERT(ec_small_far.dt >= cycle_time);
    PASS();
}


TEST checkEndCondition_at_velocity()
{
    double v_final = 1.0;
    EndCondition ec_close = checkEndCondition(cycle_time,
                                   target_dist_mock - 0.001 + SMALL_DISTANCE,
                                   target_dist_mock,
                                   v_final,
                                   v_final,
                                   a_max_mock
                                   );
    ASSERT(ec_close.dt < cycle_time);
    ASSERT_IN_RANGEm("final velocity", ec_close.v_f, v_final - DOUBLE_FUZZ, v_final + DOUBLE_FUZZ);

    EndCondition ec_far = checkEndCondition(cycle_time,
                                   target_dist_mock - 0.001 - SMALL_DISTANCE,
                                   target_dist_mock,
                                   v_final,
                                   v_final,
                                   a_max_mock
                                   );
    ASSERT(ec_far.dt >= cycle_time);
    ASSERT_IN_RANGEm("final velocity", ec_close.v_f, v_final - DOUBLE_FUZZ, v_final + DOUBLE_FUZZ);
    PASS();
}

TEST checkEndCondition_below_final_velocity()
{
    const double v_final = v_max_mock;
    const double v_current = 1.0;
    const double nominal_cycle_dist = v_current * cycle_time;

    EndCondition ec_close = checkEndCondition(cycle_time,
                                   target_dist_mock - nominal_cycle_dist,
                                   target_dist_mock,
                                   v_current,
                                   v_max_mock,
                                   a_max_mock
                                   );
    ASSERT(ec_close.dt < cycle_time);
    ASSERT_IN_RANGEm("final velocity", ec_close.v_f, 1.0, v_final);
    PASS();
}

SUITE(tc_functions) {
    RUN_TEST(checkEndCondition_zero_velocity);
    RUN_TEST(checkEndCondition_small_velocity);
    RUN_TEST(checkEndCondition_at_velocity);
    RUN_TEST(checkEndCondition_below_final_velocity);
}

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();      /* command-line arguments, initialization. */
    RUN_SUITE(blendmath);
    RUN_SUITE(joint_utils);
    RUN_SUITE(tc_functions);
    GREATEST_MAIN_END();        /* display results */
}
