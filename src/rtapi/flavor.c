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
    }

    flavor = default_flavor();
    if (!flavor) {
	    fprintf(stderr,"%s: could not detect default flavor\n", progname);
	    exit(1);
    }

    if (argc == 1) {
	printf("%s\n", flavor->name);
    } else if (strcmp(argv[1],"-m") == 0) {
	printf("%s\n", flavor->mod_ext);
    } else if (strcmp(argv[1],"-b") == 0) {
	printf("%s\n", flavor->build_sys);
    } else {
	fprintf(stderr, "Unknown option '%s'\n",argv[1]);
	exit(1);
    }
    exit(0);
}
