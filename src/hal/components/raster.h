#include "config.h"
#if defined(RTAPI_RTAI)
// These should use kstrtol etc really but the endptrs are used on return
long strtol(char *ptr, char **endptr, int base){
    return simple_strtol(ptr, endptr, base);
}

long strtoul(char *ptr, char **endptr, int base){
    return simple_strtoul(ptr, endptr, base);
}

double strtod(char* ptr, char **endptr){
    double val = 0;
    float f1 = 10.0;
    float f2 = 1.0;
    float sign = 1;

    while(*ptr){
        if (*ptr >= '0' && *ptr <= '9'){
                val *= f1;
                val += (double)((*ptr) - '0') * f2;
                if (f2 < 1) f2 /= 10;
        } else if (*ptr == '.' || *ptr == ','){
            f1 = 1;
            f2 = .1;
        } else if (*ptr == '-'){
            sign = -1;
        }
        ptr++;
    }
    val *= sign;
    *endptr = ptr;
    return val;
}
#else
#include <stdlib.h>
#endif
