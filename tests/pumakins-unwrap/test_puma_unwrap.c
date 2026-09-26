/* Unit test for pumakins joint 4/6 turn unwrap (issue #4585).
 *
 * pumakinsInverse() takes th4 and th6 straight from atan2(), which always
 * lands in (-180, 180].  When the wrist crosses a half turn the returned
 * joint jumps a full turn in one call, and every trajectory planner takes
 * that jump at face value.  The fix brings th4 and th6 back within a half
 * turn of the joints passed in, so sweeping either joint through +-180
 * must come back continuous.
 *
 * The test forward-kinematics a joint set, feeds the pose back through the
 * inverse with the same joints as the continuity reference, and checks the
 * round trip returns the same joints as it walks joints 4 and 6 across the
 * +-180 boundary.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>

#include <rtapi.h>
#include <hal.h>
#include <kinematics.h>

/* HAL stubs: enough of the hal library for pumaKinematicsSetup() to hand
   out the default PUMA560 link lengths without a running HAL.  hal_real_t
   is an opaque reference whose mapped value sits at offset 0, so a plain
   zeroed buffer behind hal_set_real() is all a pin needs here. */
void *hal_malloc(long int size)
{
    return malloc(size);
}

int hal_pin_new_real(int compid, hal_pdir_t dir, hal_real_t *ref,
                     rtapi_real def, const char *fmt, ...)
{
    (void)compid; (void)dir; (void)fmt;
    *ref = calloc(1, 16);
    hal_set_real(*ref, def);
    return 0;
}

int pumaKinematicsSetup(const int comp_id, const char *coordinates,
                        kparms *kp);
int pumaKinematicsForward(const double *joint, EmcPose *world,
                          const KINEMATICS_FORWARD_FLAGS *fflags,
                          KINEMATICS_INVERSE_FLAGS *iflags);
int pumaKinematicsInverse(const EmcPose *world, double *joint,
                          const KINEMATICS_INVERSE_FLAGS *iflags,
                          KINEMATICS_FORWARD_FLAGS *fflags);

static int failures;

/* degrees the inverse may drift from the joint set that made the pose;
   the round trip is exact maths, this only covers floating point noise */
#define TOL 1e-6

static void check_roundtrip(const double *j)
{
    EmcPose world;
    double out[6];
    KINEMATICS_FORWARD_FLAGS fflags = 0;
    KINEMATICS_INVERSE_FLAGS iflags = 0;
    double worst = 0;
    int i;

    if (pumaKinematicsForward(j, &world, &fflags, &iflags)) {
        printf("FAIL: forward failed at j4=%g j6=%g\n", j[3], j[5]);
        failures++;
        return;
    }
    memcpy(out, j, sizeof(out));
    if (pumaKinematicsInverse(&world, out, &iflags, &fflags)) {
        printf("FAIL: inverse failed at j4=%g j6=%g\n", j[3], j[5]);
        failures++;
        return;
    }
    for (i = 0; i < 6; i++) {
        double err = fabs(out[i] - j[i]);
        if (err > worst) { worst = err; }
    }
    if (worst > TOL) {
        printf("FAIL: round trip at j4=%g j6=%g: worst joint error %g deg",
               j[3], j[5], worst);
        printf(" (got");
        for (i = 0; i < 6; i++) { printf(" %g", out[i]); }
        printf(")\n");
        failures++;
    }
}

int main(void)
{
    kparms kp;
    double j[6] = { 20.0, -20.0, 100.0, 150.0, 35.0, 140.0 };

    memset(&kp, 0, sizeof(kp));
    kp.halprefix = "pumakins";
    if (pumaKinematicsSetup(0, "xyzabc", &kp)) {
        printf("FAIL: pumaKinematicsSetup\n");
        return 1;
    }

    /* walk joint 4 from 150 to 210 and joint 6 from 140 to 200, straight
       through the +-180 atan2 boundary on both; every step must round trip
       back to the commanded joints, not to a value a full turn away */
    for (j[3] = 150.0, j[5] = 140.0; j[3] <= 210.0; j[3] += 2.5, j[5] += 2.5) {
        check_roundtrip(j);
    }

    /* and back down across the boundary the other way */
    for (j[3] = 210.0, j[5] = 200.0; j[3] >= 150.0; j[3] -= 2.5, j[5] -= 2.5) {
        check_roundtrip(j);
    }

    if (failures) {
        printf("%d pumakins unwrap checks failed\n", failures);
        return 1;
    }
    printf("all pumakins unwrap checks passed\n");
    return 0;
}
