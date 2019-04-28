#include "tp_debug.h"
#include "greatest.h"
#include "blendmath.h"
#include "tp_types.h"
#include "math.h"
#include "rtapi.h"

/* Expand to all the definitions that need to be in
   the test runner's main file. */
GREATEST_MAIN_DEFS();

// KLUDGE fix link error the ugly way
void rtapi_print_msg(msg_level_t level, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    printf(fmt, args);
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

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();      /* command-line arguments, initialization. */
    RUN_SUITE(blendmath);   /* run a suite */
    GREATEST_MAIN_END();        /* display results */
 }
