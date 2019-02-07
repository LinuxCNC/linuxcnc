#include "tp_debug.h"
#include "spherical_arc.h"
#include "greatest.h"
#include "blendmath.h"
#include "tp_types.h"
#include "math.h"
#include "rtapi.h"
#include "motion_debug.h"

/* Expand to all the definitions that need to be in
   the test runner's main file. */
GREATEST_MAIN_DEFS();

#include "mock_rtapi.inc"

TEST arcInitFromPoints_simple() {
    SphericalArc arc;
    PmCartesian start = {0,0,0};
    PmCartesian end = {1,1,0};
    PmCartesian center = {0,1,0};

    int res = arcInitFromPoints(&arc, &start, &end, &center);
    ASSERT_FALSE(res);

    ASSERT(pmCartCartCompare(&arc.end, &end));
    ASSERT(pmCartCartCompare(&arc.start, &start));
    ASSERT_EQ(arc.radius, 1.0);
    PASS();
}

SUITE(arc_shape) {
    RUN_TEST(arcInitFromPoints_simple);
}

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(arc_shape);
    GREATEST_MAIN_END();
}
