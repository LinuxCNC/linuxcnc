#include "rtapi.h"
#include "vsnprintf.h"
#include <stdio.h>
#include <string.h>

int rtapi_snprintf(char *buf, unsigned long size, const char *fmt, ...)
{
    int retval;
    va_list ap;
    va_start(ap, fmt);
    retval = rtapi_vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return retval;
}

double vectors[] = {
    0.0, -0.0, 3.14, -3.14,
    100, 1e6, 1e300,
    1e-10, 1e-100, 1e-280, 1e-300,
    -__builtin_inf(), __builtin_inf(), __builtin_nan(""), 
};

int nvectors = sizeof(vectors)/sizeof(vectors[0]);

#include <sys/time.h>
#include <sys/resource.h>
double unow()
{
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_utime.tv_sec + ru.ru_utime.tv_usec*1e-6;
}

#define N (1000000)

int main(void)
{
    char buf1[1024], buf2[1024];
    int i, fail=0;
    for(i=0; i<nvectors; i++)
    {
	int j;
	double t0, t1;
	t0 = unow();
	for(j=0; j<N; j++)
	{
	    rtapi_snprintf(buf1, sizeof(buf1), "%f", vectors[i]);
	}
	t1 = unow();
	snprintf(buf2, sizeof(buf2), "%A", vectors[i]);
	printf("rtapi=%-30s libc=%-30s", buf1, buf2);
	if(strcasecmp(buf1, buf2)) {
	    fail++;
	    printf(" ****fail****");
	}
	printf(" %.1fns/it\n", (t1-t0)*1e9/N);
    }
    return fail ? 1 : 0;
}
