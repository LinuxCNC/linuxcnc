#include "math_util.h"
#include "rtapi_math.h"

double fsign(double f)
{
    if (f>0) {
        return 1.0;
    } else if (f < 0) {
        return -1.0;
    } else {
        //Technically this should be NAN but that's a useless result for tp purposes
        return 0;
    }
}

/** Clip the input at the specified minimum (in place). */
int clip_min(double * const x, double min) {
    if ( *x < min ) {
        *x = min;
        return 1;
    }
    return 0;
}

/** Clip the input at the specified maximum (in place). */
int clip_max(double * const x, double max) {
    if ( *x > max ) {
        *x = max;
        return 1;
    }
    return 0;
}

/**
 * Saturate a value x to be within +/- max.
 */
double saturate(double x, double max) {
    if ( x > max ) {
        return max;
    }
    else if ( x < (-max) ) {
        return -max;
    }
    else {
        return x;
    }
}


/**
 * Apply bounds to a value x.
 */
double bound(double x, double max, double min) {
    if ( x > max ) {
        return max;
    }
    else if ( x < (min) ) {
        return min;
    }
    else {
        return x;
    }
}


/** In-place saturation function */
int sat_inplace(double * const x, double max) {
    if ( *x > max ) {
        *x = max;
        return 1;
    }
    else if ( *x < -max ) {
        *x = -max;
        return -1;
    }
    return 0;
}

double hypot2(double x, double y)
{
    return sqrt(x*x + y*y);
}

double hypot3(double x, double y, double z)
{
    return sqrt(x*x + y*y + z*z);
}

double hypot4(double w, double x, double y, double z)
{
    return sqrt(w*w + x*x + y*y + z*z);
}
