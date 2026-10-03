#include <float.h>
#include <math.h>
#include <stdio.h>
#include "gomath.h"

/* gomath and this independent oracle both use double-precision libm. */
#if defined(GO_REAL_FLOAT)
#define TIGHT (16.0 * FLT_EPSILON)
#else
#define TIGHT 1.0e-12
#endif
#define NEAR (2.0 * GO_REAL_EPSILON + TIGHT)

typedef double matrix[3][3];
static int failures;

static void multiply(const matrix a, const matrix b, matrix result)
{
    int i, j, k;
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j) {
            result[i][j] = 0;
            for (k = 0; k < 3; ++k)
                result[i][j] += a[i][k] * b[k][j];
        }
}

/* Deliberately do not use any gomath forward conversions as the oracle. */
static void rotation(double roll, double pitch, double yaw, matrix result)
{
    matrix rx = {{1, 0, 0}, {0, cos(roll), -sin(roll)},
                 {0, sin(roll), cos(roll)}};
    matrix ry = {{cos(pitch), 0, sin(pitch)}, {0, 1, 0},
                 {-sin(pitch), 0, cos(pitch)}};
    matrix rz = {{cos(yaw), -sin(yaw), 0}, {sin(yaw), cos(yaw), 0},
                 {0, 0, 1}};
    matrix temp;
    multiply(rz, ry, temp);
    multiply(temp, rx, result);
}

static void fail(const char *name, const char *api, const char *what,
                 double actual, double expected)
{
    fprintf(stderr, "%s, %s: %s %.17g (expected %.17g)\n",
            name, api, what, actual, expected);
    ++failures;
}

static void check_result(const char *name, const char *api, int status,
                         double roll, double pitch, double yaw,
                         const matrix input, int pole, double expected_roll,
                         double tolerance, int outside)
{
    matrix result;
    double error = 0;
    int i, j;
    if (status != GO_RESULT_OK) {
        fail(name, api, "return code", status, GO_RESULT_OK);
        return;
    }
    if (!isfinite(roll) || !isfinite(pitch) || !isfinite(yaw)) {
        fprintf(stderr, "%s, %s: non-finite angles (%g, %g, %g)\n",
                name, api, roll, pitch, yaw);
        ++failures;
        return;
    }
    rotation(roll, pitch, yaw, result);
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            error = fmax(error, fabs(result[i][j] - input[i][j]));
    if (error > tolerance)
        fail(name, api, "matrix error exceeds tolerance:", error, tolerance);

    if (pole) {
        /* These fields are explicitly assigned, including for float builds. */
        double canonical_pitch = (go_real)(pole * GO_PI_2);
        if (pitch != canonical_pitch)
            fail(name, api, "canonical pitch", pitch, canonical_pitch);
        if (yaw != 0)
            fail(name, api, "canonical yaw", yaw, 0);
        error = fabs(remainder(roll - expected_roll, GO_2_PI));
        if (error > TIGHT)
            fail(name, api, "roll error modulo 2*pi:", error, TIGHT);
    }
    if (outside) {
        /* NEAR alone could hide an incorrectly snapped 2*epsilon case. */
        double distance = GO_PI_2 - fabs(pitch);
        if (distance <= GO_REAL_EPSILON)
            fail(name, api, "outside pitch distance must exceed epsilon:",
                 distance, GO_REAL_EPSILON);
        if (fabs(yaw) < TIGHT)
            fail(name, api, "outside yaw unexpectedly snapped:", yaw, TIGHT);
    }
}

static void check_matrix(const char *name, const matrix input, int pole,
                         double expected_roll, double tolerance, int outside)
{
    /* go_mat stores columns, whereas the oracle uses row-major arrays. */
    go_mat m = {{input[0][0], input[1][0], input[2][0]},
                {input[0][1], input[1][1], input[2][1]},
                {input[0][2], input[1][2], input[2][2]}};
    go_rpy rpy = {NAN, NAN, NAN};
    go_zyx zyx = {NAN, NAN, NAN};
    int status = go_mat_rpy_convert(&m, &rpy);
    check_result(name, "go_mat_rpy_convert", status, rpy.r, rpy.p, rpy.y,
                 input, pole, expected_roll, tolerance, outside);
    status = go_mat_zyx_convert(&m, &zyx);
    check_result(name, "go_mat_zyx_convert", status, zyx.x, zyx.y, zyx.z,
                 input, pole, expected_roll, tolerance, outside);
}

static void check_quaternion(const char *name, const matrix input, int pole,
                             double roll, int sign)
{
    /* Ry(pole*pi/2) Rx(roll): analytically normalized, without gomath. */
    double c = sign * cos(roll / 2) / sqrt(2.0);
    double s = sign * sin(roll / 2) / sqrt(2.0);
    go_quat q = {c, s, pole * c, -pole * s};
    double norm = (double)q.s * q.s + (double)q.x * q.x
                + (double)q.y * q.y + (double)q.z * q.z;
    go_rpy rpy = {NAN, NAN, NAN};
    go_zyx zyx = {NAN, NAN, NAN};
    int status;
    if (fabs(norm - 1) > TIGHT)
        fail(name, "quaternion input", "squared norm", norm, 1);
    status = go_quat_rpy_convert(&q, &rpy);
    check_result(name, "go_quat_rpy_convert", status, rpy.r, rpy.p, rpy.y,
                 input, pole, roll, TIGHT, 0);
    status = go_quat_zyx_convert(&q, &zyx);
    check_result(name, "go_quat_zyx_convert", status, zyx.x, zyx.y, zyx.z,
                 input, pole, roll, TIGHT, 0);
}

int main(void)
{
    /* Sum/difference angles cover all quadrants and wrap past +/-pi. */
    static const double pairs[][2] = {
        {30, 20}, {120, 25}, {-125, -40}, {-40, 15},
        {170, 40}, {-150, 60}, {91.377, 88.623}
    };
    static const double offsets[] = {0.25, 0.5, 2, 10};
    static const double ordinary[][3] = {
        {0, 0, 0}, {30, 20, -40}, {-170, -60, 135},
        {143, 67, -112}, {-85, 42, 178}, {75, -35, -125},
        {180, 0, -90}
    };
    unsigned int i, j;
    int pole, side, sign;
    char name[160];
    matrix input;

    for (pole = -1; pole <= 1; pole += 2)
        for (i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i) {
            double roll = GO_TO_RAD(pairs[i][0]);
            double yaw = GO_TO_RAD(pairs[i][1]);
            double canonical_roll = roll - pole * yaw;
            double s = sin(canonical_roll), c = cos(canonical_roll);
            /* Exact pole: zero entries must not come from cos(pi/2). */
            matrix exact = {{0, pole * s, pole * c},
                            {0, c, -s}, {-pole, 0, 0}};
            snprintf(name, sizeof(name), "exact pole=%+d roll=%g yaw=%g",
                     pole, pairs[i][0], pairs[i][1]);
            check_matrix(name, exact, pole, canonical_roll, TIGHT, 0);

            for (sign = -1; sign <= 1; sign += 2) {
                snprintf(name, sizeof(name), "quaternion sign=%+d pole=%+d pair=%u",
                         sign, pole, i);
                check_quaternion(name, exact, pole, canonical_roll, sign);
            }
            for (j = 0; j < sizeof(offsets) / sizeof(offsets[0]); ++j)
                for (side = -1; side <= 1; side += 2) {
                    double delta = side * offsets[j] * GO_REAL_EPSILON;
                    int inside = offsets[j] < 1;
                    rotation(roll, pole * GO_PI_2 + delta, yaw, input);
                    snprintf(name, sizeof(name),
                             "near pole=%+d pair=%u delta=%+.2g*epsilon",
                             pole, i, side * offsets[j]);
                    check_matrix(name, input, inside ? pole : 0,
                                 canonical_roll, NEAR, !inside);
                }
        }
    for (i = 0; i < sizeof(ordinary) / sizeof(ordinary[0]); ++i) {
        rotation(GO_TO_RAD(ordinary[i][0]), GO_TO_RAD(ordinary[i][1]),
                 GO_TO_RAD(ordinary[i][2]), input);
        snprintf(name, sizeof(name), "ordinary rotation=%u", i);
        check_matrix(name, input, 0, 0, TIGHT, 0);
    }
    if (failures)
        return 1;
    puts("gomath Euler singularities: OK");
    return 0;
}
