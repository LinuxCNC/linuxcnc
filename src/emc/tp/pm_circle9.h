#ifndef PM_CIRCLE9_H
#define PM_CIRCLE9_H

#include "posemath.h"
#include "emcpose.h"
#include "pm_line9.h"

/**
 * Spiral arc length approximation by quadratic fit.
 */
typedef struct {
    double b0;                  /* 2nd order coefficient */
    double b1;                  /* 1st order coefficient */
    double total_planar_length; /* total arc length in plane */
    int spiral_in;              /* flag indicating spiral is inward,
                                   rather than outward */
} SpiralArcLengthFit;

typedef struct {
    PmCircle xyz;
    PmCartLine abc;
    PmCartLine uvw;
    SpiralArcLengthFit fit;
    double xyz_ratio;
    double abc_ratio;
    double uvw_ratio;
    double total_length;
} PmCircle9;

typedef struct {
    double v_max;
    double acc_ratio;
} PmCircleLimits;

double pmCircle9LengthAndRatios(PmCircle9 * const circ9);

int pmCircle9Init(PmCircle9 * const circ9,
    PmVector const * const start,
    PmVector const * const end,
    PmCartesian const * const center,
    PmCartesian const * const normal,
    int turn, double expected_angle_rad);

double pmCircle9Length(PmCircle9 const * const circ9);

int pmCircle9Point(PmCircle9 const * const circ9, double progress, PmVector * const point);

int pmCircle9Cut(PmCircle9 * const circ, double new_pt_progress, SegmentToKeepType keep_pt);

int pmCircle9TangentVector(PmCircle9 const * const circ9, double progress, PmVector * const out);

int pmCircleTangentVector(
    PmCircle const * const circle,
    double angle_in,
    PmCartesian * const out);

PmCircleLimits pmCircleActualMaxVel(
    PmCircle const * const circle,
    double v_max3,
    double a_max3);

int findSpiralArcLengthFit(PmCircle const * const circle,
    SpiralArcLengthFit * const fit, double angle_tolerance);

double spiralEffectiveRadius(PmCircle const * circle);

double pmCircleEffectiveMinRadius(const PmCircle *circle);

int pmCircleAngleFromParam(
    PmCircle const * const circle,
    SpiralArcLengthFit const * const fit,
    double t,
    double * const angle);

int pmCircleAngleFromProgress(
    PmCircle const * const circle,
    SpiralArcLengthFit const * const fit,
    double progress,
    double * const angle);

double pmCircle9VLimit(const PmCircle9 * const circle, double v_target, double v_limit_linear, double v_limit_angular);

#endif // PM_CIRCLE9_H
