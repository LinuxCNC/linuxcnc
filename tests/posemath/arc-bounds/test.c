#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "posemath.h"

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        exit(1);
    }
}

static void near(double value, double expected, const char *message)
{
    if (!isfinite(value) || fabs(value - expected) > 1e-11) {
        fprintf(stderr, "%s: %.17g != %.17g\n", message, value, expected);
        exit(1);
    }
}

static PmCircle make_circle(PmCartesian start, PmCartesian end,
        PmCartesian center, PmCartesian normal, int turn)
{
    PmCircle circle;
    require(pmCircleInit(&circle, &start, &end, &center, &normal, turn) == PM_OK,
            "circle initialization failed");
    return circle;
}

static double coordinate(PmCartesian p, int axis)
{
    return axis == 0 ? p.x : axis == 1 ? p.y : p.z;
}

/* An independent containment check against the path evaluator, also checking
   that the bounds are tight to the resolution of the reference points. */
static void check_points(PmCircle circle)
{
    PmCartesian min, max, point;
    double seen_min[3] = {DBL_MAX, DBL_MAX, DBL_MAX};
    double seen_max[3] = {-DBL_MAX, -DBL_MAX, -DBL_MAX};
    int i, axis;
    const int count = 4000;
    require(pmCircleBounds(&circle, &min, &max) == PM_OK, "bounds failed");
    for (i = 0; i <= count; i++) {
        require(pmCirclePoint(&circle, circle.angle * i / count, &point) == PM_OK,
                "point failed");
        for (axis = 0; axis < 3; axis++) {
            double value = coordinate(point, axis);
            double tolerance = 1e-10 * (1 + fabs(value));
            if (value < coordinate(min, axis) - tolerance
                    || value > coordinate(max, axis) + tolerance) {
                fprintf(stderr, "point %d axis %d: %.17g outside [%.17g, %.17g], "
                        "angle %.17g spiral %.17g\n", i, axis, value,
                        coordinate(min, axis), coordinate(max, axis),
                        circle.angle, circle.spiral);
                exit(1);
            }
            seen_min[axis] = fmin(seen_min[axis], value);
            seen_max[axis] = fmax(seen_max[axis], value);
        }
    }
    for (axis = 0; axis < 3; axis++) {
        double speed = circle.radius + fabs(circle.spiral)
            + (fabs(circle.spiral) + fabs(coordinate(circle.rHelix, axis))) / circle.angle;
        double tolerance = speed * circle.angle / count + 1e-9;
        require(seen_min[axis] - coordinate(min, axis) <= tolerance,
                "lower bound is unnecessarily wide");
        require(coordinate(max, axis) - seen_max[axis] <= tolerance,
                "upper bound is unnecessarily wide");
    }
}

static unsigned random_state = 3839;
static double random_unit(void)
{
    random_state = random_state * 1664525U + 1013904223U;
    return (random_state >> 8) / 16777216.0;
}

int main(void)
{
    PmCartesian zero = {0, 0, 0}, z = {0, 0, 1};
    PmCartesian start = {1, 0, 0}, end = {0, 1, 0}, min, max;
    PmCircle circle = make_circle(start, end, zero, z, 0);
    require(pmCircleBounds(&circle, &min, &max) == PM_OK, "quarter circle failed");
    near(min.x, 0, "quarter minimum X");
    near(min.y, 0, "quarter minimum Y");
    near(max.x, 1, "quarter maximum X");
    near(max.y, 1, "quarter maximum Y");
    check_points(circle);

    /* Exact tangency remains legal with the motion layer's 1e-12 epsilon,
       even when coordinates are much larger than that absolute tolerance. */
    for (int i = 0; i < 2; i++) {
        double radius = i ? 1e9 : 1000;
        PmCartesian center = {radius, 0, 0};
        PmCartesian tangent = {2 * radius, 0, 0};
        circle = make_circle(tangent, tangent, center, z, 0);
        require(pmCircleBounds(&circle, &min, &max) == PM_OK, "tangent circle failed");
        require(min.x >= -1e-12 && max.x <= 2 * radius + 1e-12,
                "legal tangent circle was expanded past its limit");
        near(min.y, -radius, "tangent minimum Y");
        near(max.y, radius, "tangent maximum Y");
    }

    /* Clockwise takes the other three quarters and crosses negative limits. */
    circle = make_circle(start, end, zero, z, -1);
    require(pmCircleBounds(&circle, &min, &max) == PM_OK, "clockwise bounds failed");
    near(min.x, -1, "clockwise minimum X");
    near(min.y, -1, "clockwise minimum Y");
    check_points(circle);

    /* A narrow arc must not inherit the bounds of its entire circle. */
    start = (PmCartesian){cos(0.2), sin(0.2), 0};
    end = (PmCartesian){cos(0.3), sin(0.3), 0};
    circle = make_circle(start, end, zero, z, 0);
    require(pmCircleBounds(&circle, &min, &max) == PM_OK, "short arc failed");
    near(min.x, end.x, "short arc minimum X");
    near(max.x, start.x, "short arc maximum X");

    /* The spiral's X maximum lies between the cardinal directions: at pi/4. */
    start = (PmCartesian){1 - PM_PI / 4, 0, 0};
    end = (PmCartesian){0, 1 + PM_PI / 4, 0};
    circle = make_circle(start, end, zero, z, 0);
    require(pmCircleBounds(&circle, &min, &max) == PM_OK, "spiral bounds failed");
    near(max.x, sqrt(0.5), "spiral interior maximum");
    check_points(circle);
    circle = make_circle(end, start, zero, z, -1);
    require(pmCircleBounds(&circle, &min, &max) == PM_OK, "inward spiral failed");
    near(max.x, sqrt(0.5), "inward spiral interior maximum");
    check_points(circle);

    /* A tilted helix has extrema shifted from the planar cardinal angles. */
    PmCartesian normal = {0, sqrt(0.5), sqrt(0.5)};
    start = (PmCartesian){1, 0, 0};
    end = (PmCartesian){1, PM_PI * sqrt(0.5) / 5, PM_PI * sqrt(0.5) / 5};
    circle = make_circle(start, end, zero, normal, 0);
    require(pmCircleBounds(&circle, &min, &max) == PM_OK, "tilted helix failed");
    double theta = acos(-0.1);
    near(max.y, (sin(theta) + theta / 10) * sqrt(0.5), "tilted helix maximum Y");
    check_points(circle);

    /* Work must not scale with the number of turns. */
    circle = make_circle(start, start, zero, z, INT_MAX);
    require(pmCircleBounds(&circle, &min, &max) == PM_OK, "many-turn circle failed");
    near(min.x, -1, "many-turn minimum X");
    near(max.y, 1, "many-turn maximum Y");
    circle.spiral = 1;
    circle.rHelix.z = 10;
    require(pmCircleBounds(&circle, &min, &max) == PM_OK, "many-turn spiral failed");
    require(min.x < -1.99999999 && max.x > 1.99999999,
            "many-turn spiral missed the last turn");
    near(min.z, 0, "many-turn helix minimum Z");
    near(max.z, 10, "many-turn helix maximum Z");

    /* Reproducible varied planes, directions, helices and inward/outward
       spirals exercise the general geometry independently of machine setup. */
    for (int i = 0; i < 200; i++) {
        PmCartesian center = {random_unit() * 10, random_unit() * 10, random_unit() * 10};
        normal = (PmCartesian){random_unit() - 0.5, random_unit() - 0.5, random_unit() - 0.5};
        start = (PmCartesian){center.x + 1 + random_unit() * 10,
            center.y + random_unit() * 10, center.z + random_unit() * 10};
        end = (PmCartesian){center.x + random_unit() * 20 - 10,
            center.y + random_unit() * 20 - 10, center.z + random_unit() * 20 - 10};
        circle = make_circle(start, end, center, normal, i % 9 - 4);
        check_points(circle);
    }

    require(pmCircleBounds(NULL, &min, &max) != PM_OK, "null circle accepted");
    circle.angle = 0;
    require(pmCircleBounds(&circle, &min, &max) != PM_OK, "zero angle accepted");
    circle.angle = NAN;
    require(pmCircleBounds(&circle, &min, &max) != PM_OK, "NaN angle accepted");
    circle.angle = 1;
    circle.radius = 0;
    require(pmCircleBounds(&circle, &min, &max) != PM_OK, "zero radius accepted");
    circle.radius = 1;
    circle.spiral = -2;
    require(pmCircleBounds(&circle, &min, &max) != PM_OK, "negative end radius accepted");
    circle.spiral = 0;
    circle.center.x = INFINITY;
    require(pmCircleBounds(&circle, &min, &max) != PM_OK, "infinite center accepted");
    puts("arc bounds passed");
    return 0;
}
