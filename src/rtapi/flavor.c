#include "rtapi.h"
#include "rtapi_kdetect.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include "rtapi.h"
#include "rtapi_support.h"
#include "rtapi_flavor.h"

int shmdrv_loaded;
size_t page_size;

int main(int argc, char **argv)
{
    char *fname = getenv("FLAVOR");
    char *progname = argv[0];
    flavor_ptr f, flavor;

    if (fname && ((flavor = flavor_byname(fname)) == NULL)) {
	fprintf(stderr, "%s: FLAVOR=%s: no such flavor -- valid flavors are:\n",
		progname, fname);
	f = flavors;
	while (f->name) {
	    fprintf(stderr, "\t%s\n", f->name);
	    f++;
	}
	exit(1);
    } else {
	flavor = default_flavor();
	if (flavor) {
	    printf("%s\n", flavor->name);
	} else {
	    fprintf(stderr,"%s: could not deteact default flavor\n", progname);
	    exit(1);
	}
    }
    exit(0);
}
