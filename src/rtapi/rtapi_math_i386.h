/* GPL, etc -- based loosely on mathinline.h */
#ifndef RTAPI_MATH_I386_H
#define RTAPI_MATH_I386_H

#define MATHOP(f,i) extern __inline double f(double __x) { \
    register double __result; \
    __asm __volatile__ (i : "=t" (__result) : "0" (__x)); \
    return __result; \
}

MATHOP(sin, "fsin")
MATHOP(cos, "fcos")
MATHOP(fabs, "fabs")
MATHOP(sqrt, "fsqrt")

extern __inline double tan (double __x) {
    register long double __value;
    register long double __value2 __attribute__ ((__unused__));
    __asm __volatile__ ("fptan" : "=t" (__value2), "=u" (__value) : "0" (__x));
    return __value;
}

extern __inline double atan2 (double __y, double __x) {
    register long double __value;
    __asm __volatile__ ("fpatan" : "=t" (__value) : "0" (__x), "u" (__y) : "st(1)");
    return __value;
}

extern __inline double atan (double __y) {
    return atan2(__y, 1.);
}

extern __inline double asin (double __x) {
    return atan2(__x, sqrt (1.0 - __x * __x));
}

extern __inline double acos (double __x) {
    return atan2(sqrt(1.0 - __x * __x), __x);
}
#endif /* RTAPI_MATH_I386_H */
