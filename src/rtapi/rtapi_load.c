#include "rtapi.h"
#include "rtapi_kdetect.h"
#include "rtapi_flavor.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/utsname.h>
#include <time.h>
#include <assert.h>


typedef struct rtapi_flavor *(*flavor_init)(const char *);  // rtapi_flavor_init() signature

struct rtapi_flavor *flavor;  // all RTAPI calls go through here

int main(int argc, char **argv)
{
    const char *object = "flavor1.so";   // change as per autodetect/param
    const char *errmsg;
    void *flavor_lib;
    flavor_init func;

    if ((flavor_lib = dlopen(object, RTLD_LAZY)) == NULL) {
	errmsg = dlerror();
	fprintf(stderr,"dlopen(%s): %s\n", object, errmsg ? errmsg : "NULL");
	exit(1);
    }

    if ((func = (flavor_init) dlsym(flavor_lib, "rtapi_flavor_init")) != NULL) {
	flavor = func("called from main");
	assert(flavor != NULL);
	fprintf(stderr,"rtapi_flavor.name = '%s'\n", flavor->name);
	fprintf(stderr,"rtapi_flavor.prio = %d\n", flavor->prio);
	fprintf(stderr,"rtapi_flavor.func1() = %d\n", flavor->func1());
    }

    dlclose(flavor_lib);
    exit(0);
}
