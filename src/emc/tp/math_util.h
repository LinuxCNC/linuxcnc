#ifndef MATH_UTIL_H
#define MATH_UTIL_H

static inline long max(long y, long x) {
    return y > x ? y : x;
}

static inline long min(long y, long x) {
    return y < x ? y : x;
}

static inline double signum(double x) {
    return (x > 0.0) ? 1.0 : (x < 0.0) ? -1.0 : 0.0;
}

double fsign(double f);

int clip_min(double * const x, double min);

int clip_max(double * const x, double max);

double saturate(double x, double max);

double bound(double x, double max, double min);

int sat_inplace(double * const x, double max);

double hypot2(double x, double y);
double hypot3(double x, double y, double z);
double hypot4(double w, double x, double y, double z);

#endif // MATH_UTIL_H
