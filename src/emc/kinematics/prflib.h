#ifndef PRFLIB_H
#define PRFLIB_H

#define ARRAY_SIZE 6
#ifndef N
#define N ARRAY_SIZE
#endif

double vector_dot(double *a, double *b);

double vector_len(double *a);

double vector_sqr(double *a);

void vector_scale(double *v, double *result, double scale);

void prf_cbrt(double *a, double *result);

void prf_copy(double *a, double *result);

void prf_cube(double *a, double *result);

void prf_cubic(double *a, double *b, double *c, double *d, double *result, double t);

void prf_multiply(double *a, double *b, double *result);

void prf_negate(double *a, double *result);

void prf_normalize(double *a, double *result);

void prf_pow(double *a, double *result, double p);

void prf_quintic(double *a, double *b, double *c, double *d, double *f, double *result, double t);

void prf_scale(double *a, double *result, double scale);

void prf_sqr(double *a, double *result);

void prf_sqrt(double *a, double *result);

void prf_sum(double *a, double *b, double *result);

void prf_sub(double *a, double *b, double *result);

void prf_interpolate(double *a, double *b, double *result, double d);

#endif
