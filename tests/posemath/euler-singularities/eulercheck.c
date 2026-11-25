/* Exercise the posemath conversions exported by tpmod.
 *
 * The oracle constructs Rz(yaw) Ry(pitch) Rx(roll) independently of
 * posemath.  At a pole the Euler angles are not unique, so check both
 * the reconstructed rotation and posemath's canonical representation.
 */
#include <rtapi.h>
#include <rtapi_app.h>
#include <rtapi_math.h>
#include <hal.h>
#include <posemath.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("posemath Euler singularity regression checks");

#define TIGHT 1.0e-12

typedef double matrix[3][3];
static int comp_id = -1;
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
    rtapi_print_msg(RTAPI_MSG_ERR,
                   "eulercheck: FAIL %s, %s: %s %.17g (expected %.17g)\n",
                   name, api, what, actual, expected);
    ++failures;
}

static void check_result(const char *name, const char *api, int status,
                         double roll, double pitch, double yaw,
                         const matrix input, int pole, double expected_roll,
                         double tolerance, int outside, double fuzz)
{
    matrix result;
    double error = 0;
    int i, j;
    if (status != PM_OK) {
        fail(name, api, "return code", status, PM_OK);
        return;
    }
    if (!isfinite(roll) || !isfinite(pitch) || !isfinite(yaw)) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                       "eulercheck: FAIL %s, %s: non-finite angles (%g, %g, %g)\n",
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
        /* Both fields are assigned exactly by the singularity branch. */
        double canonical_pitch = pole * PM_PI_2;
        if (pitch != canonical_pitch)
            fail(name, api, "canonical pitch", pitch, canonical_pitch);
        if (yaw != 0)
            fail(name, api, "canonical yaw", yaw, 0);
        error = fabs(atan2(sin(roll - expected_roll),
                           cos(roll - expected_roll)));
        if (error > TIGHT)
            fail(name, api, "roll error modulo 2*pi:", error, TIGHT);
    }
    if (outside) {
        double distance = PM_PI_2 - fabs(pitch);
        if (distance <= fuzz)
            fail(name, api, "outside pitch distance must exceed fuzz:",
                 distance, fuzz);
        if (fabs(yaw) < TIGHT)
            fail(name, api, "outside yaw unexpectedly snapped:", yaw, TIGHT);
    }
}

static void check_matrix(const char *name, const matrix input, int pole,
                         double expected_roll, double tolerance,
                         int outside, int use_zyx, double fuzz)
{
    /* PmRotationMatrix stores columns; the oracle uses row-major arrays. */
    PmRotationMatrix m = {{input[0][0], input[1][0], input[2][0]},
                          {input[0][1], input[1][1], input[2][1]},
                          {input[0][2], input[1][2], input[2][2]}};
    int status;
    if (use_zyx) {
        PmEulerZyx zyx = {nan(""), nan(""), nan("")};
        status = pmMatZyxConvert(&m, &zyx);
        check_result(name, "pmMatZyxConvert", status, zyx.x, zyx.y, zyx.z,
                     input, pole, expected_roll, tolerance, outside, fuzz);
    } else {
        PmRpy rpy = {nan(""), nan(""), nan("")};
        status = pmMatRpyConvert(&m, &rpy);
        check_result(name, "pmMatRpyConvert", status, rpy.r, rpy.p, rpy.y,
                     input, pole, expected_roll, tolerance, outside, fuzz);
    }
}

static void check_quaternion(const char *name, const matrix input, int pole,
                             double roll, int sign, int use_zyx, double fuzz)
{
    /* Ry(pole*pi/2) Rx(roll): analytically normalized, without posemath. */
    double c = sign * cos(roll / 2) / sqrt(2.0);
    double s = sign * sin(roll / 2) / sqrt(2.0);
    PmQuaternion q = {c, s, pole * c, -pole * s};
    double norm = q.s * q.s + q.x * q.x + q.y * q.y + q.z * q.z;
    int status;
    if (fabs(norm - 1) > TIGHT)
        fail(name, "quaternion input", "squared norm", norm, 1);
    if (use_zyx) {
        PmEulerZyx zyx = {nan(""), nan(""), nan("")};
        status = pmQuatZyxConvert(&q, &zyx);
        check_result(name, "pmQuatZyxConvert", status, zyx.x, zyx.y, zyx.z,
                     input, pole, roll, TIGHT, 0, fuzz);
    } else {
        PmRpy rpy = {nan(""), nan(""), nan("")};
        status = pmQuatRpyConvert(&q, &rpy);
        check_result(name, "pmQuatRpyConvert", status, rpy.r, rpy.p, rpy.y,
                     input, pole, roll, TIGHT, 0, fuzz);
    }
}

int rtapi_app_main(void)
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
    int use_zyx, pole, side, sign;
    char name[160];
    matrix input;

    comp_id = hal_init("eulercheck");
    if (comp_id < 0)
        return comp_id;

    for (use_zyx = 0; use_zyx <= 1; ++use_zyx) {
        double fuzz = use_zyx ? ZYX_Y_FUZZ : RPY_P_FUZZ;
        for (pole = -1; pole <= 1; pole += 2)
            for (i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i) {
                double roll = pairs[i][0] * TO_RAD;
                double yaw = pairs[i][1] * TO_RAD;
                double canonical_roll = roll - pole * yaw;
                double s = sin(canonical_roll), c = cos(canonical_roll);
                /* Exact pole: zero entries must not come from cos(pi/2). */
                matrix exact = {{0, pole * s, pole * c},
                                {0, c, -s}, {-pole, 0, 0}};
                rtapi_snprintf(name, sizeof(name),
                               "exact pole=%+d roll=%g yaw=%g",
                               pole, pairs[i][0], pairs[i][1]);
                check_matrix(name, exact, pole, canonical_roll, TIGHT,
                             0, use_zyx, fuzz);

                for (sign = -1; sign <= 1; sign += 2) {
                    rtapi_snprintf(name, sizeof(name),
                                   "quaternion sign=%+d pole=%+d pair=%u",
                                   sign, pole, i);
                    check_quaternion(name, exact, pole, canonical_roll,
                                     sign, use_zyx, fuzz);
                }
                for (j = 0; j < sizeof(offsets) / sizeof(offsets[0]); ++j)
                    for (side = -1; side <= 1; side += 2) {
                        double delta = side * offsets[j] * fuzz;
                        int inside = offsets[j] < 1;
                        double tolerance = inside ? 2 * fuzz + TIGHT : TIGHT;
                        rotation(roll, pole * PM_PI_2 + delta, yaw, input);
                        rtapi_snprintf(name, sizeof(name),
                                       "near pole=%+d pair=%u delta=%+.2g*fuzz",
                                       pole, i, side * offsets[j]);
                        check_matrix(name, input, inside ? pole : 0,
                                     canonical_roll, tolerance, !inside,
                                     use_zyx, fuzz);
                    }
            }
        for (i = 0; i < sizeof(ordinary) / sizeof(ordinary[0]); ++i) {
            rotation(ordinary[i][0] * TO_RAD, ordinary[i][1] * TO_RAD,
                     ordinary[i][2] * TO_RAD, input);
            rtapi_snprintf(name, sizeof(name), "ordinary rotation=%u", i);
            check_matrix(name, input, 0, 0, TIGHT, 0, use_zyx, fuzz);
        }
    }
    if (failures) {
        rtapi_print_msg(RTAPI_MSG_ERR, "eulercheck: %d check(s) failed\n",
                       failures);
        hal_exit(comp_id);
        return -ERANGE;
    }
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}
