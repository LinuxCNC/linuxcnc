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
MATHOP(tan, "fptan")

extern __inline double atan2 (double __y, double __x) {
    register long double __value;
    __asm __volatile__ ("fpatan" : "=t" (__value) : "0" (__x), "u" (__y) : "st(1)");
    return __value;
}

extern __inline double acos (double __x) {
    return atan2(sqrt(1.0 - __x * __x), __x);
}

#ifndef __GNUC_PREREQ
/* Convenience macro to test the versions of glibc and gcc. */
/*  taken from include/features.h */
#if defined __GNUC__ && defined __GNUC_MINOR__
# define __GNUC_PREREQ(maj, min) \
	((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
# define __GNUC_PREREQ(maj, min) 0
#endif
#endif /* __GNUC_PREREQ */


#ifndef HUGE_VAL
/* IEEE positive infinity (-HUGE_VAL is negative infinity).  */
/*  taken from include/bits/huge_val.h */
#if __GNUC_PREREQ(3,3)
# define HUGE_VAL	(__builtin_huge_val())
#elif __GNUC_PREREQ(2,96)
# define HUGE_VAL	(__extension__ 0x1.0p2047)
#else
# define HUGE_VAL \
  (__extension__							      \
   ((union { unsigned __l __attribute__((__mode__(__DI__))); double __d; })   \
    { __l: 0x7ff0000000000000ULL }).__d)
#endif
#endif /* HUGE_VAL */

#endif /* RTAPI_MATH_I386_H */
