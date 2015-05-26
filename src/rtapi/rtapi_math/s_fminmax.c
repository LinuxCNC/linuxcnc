#include "rtapi_math.h"

double rtapi_fmax(double __y, double __x) {
    return __y > __x ? __y : __x;
}

double rtapi_fmin(double __y, double __x) {
    return __y < __x ? __y : __x;
}
