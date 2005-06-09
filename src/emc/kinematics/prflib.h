#ifndef PRFLIB_H
#define PRFLIB_H

#define ARRAY_SIZE 6
#ifndef N
#define N ARRAY_SIZE
#endif

extern double vector_dot(double *a, double *b);

extern double vector_len(double *a);

extern double vector_sqr(double *a);

extern void vector_scale(double *v, double *result, double scale);

extern double prf_blend((double *v, double vel, double max_vel, double max_accel);

extern void prf_cbrt(double *a, double *result);

extern void prf_copy(double *a, double *result);

extern void prf_cube(double *a, double *result);

extern void prf_multiply(double *a, double *b, double *result);

extern void prf_negate(double *a, double *result);

extern void prf_normalize(double *a, double *result);

extern void prf_pow(double *a, double *result, double p);

extern void prf_scale(double *a, double *result, double scale);

extern void prf_sqr(double *a, double *result);

extern void prf_sqrt(double *a, double *result);

extern void prf_sum(double *a, double *b, double *result);

extern void prf_sub(double *a, double *b, double *result);

extern void prf_interpolate(double *a, double *b, double *result, double d);

#endif
