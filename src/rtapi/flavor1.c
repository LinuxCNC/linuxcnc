

#include "rtapi.h"
#include "rtapi_flavor.h"
#include <stdio.h>


static int some_func1(void)
{
   fprintf(stderr,"%s:%s\n", __FILE__, __FUNCTION__);
   return 4711;
}


static struct rtapi_flavor flavor = {
    .name = "as found in " __FILE__,
    .prio = 123,
    .func1 = some_func1
};


// the only non-static definition per flavor:

struct rtapi_flavor *rtapi_flavor_init(const char *yada)
{
    fprintf(stderr,"%s:%s %s\n", __FILE__, __FUNCTION__, yada);
    return &flavor;
}
