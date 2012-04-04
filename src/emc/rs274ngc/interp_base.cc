#include "interp_base.hh"
#include <dlfcn.h>
#include <limits.h>
#include <config.h>
#include <stdio.h>

InterpBase::~InterpBase() {}

InterpBase *interp_from_shlib(const char *shlib) {
    fprintf(stderr, "interp_from_shlib(%s)\n", shlib);
    dlopen(NULL, RTLD_GLOBAL);
    void *interp_lib = dlopen(shlib, RTLD_NOW);
    if(!interp_lib) {
	fprintf(stderr, "emcTaskInit: could not open interpreter '%s': %s\n", shlib, dlerror());
	char relative_interp[PATH_MAX];
	snprintf(relative_interp, sizeof(relative_interp), "%s/%s",
	    EMC2_HOME "/lib/emc2", shlib);
	interp_lib = dlopen(relative_interp, RTLD_NOW);
    }
    if(!interp_lib) {
	fprintf(stderr, "emcTaskInit: could not open interpreter '%s': %s\n", shlib, dlerror());
	return 0;
    }
    typedef InterpBase* (*Constructor)();
    Constructor constructor = (Constructor)dlsym(interp_lib, "makeInterp");
    if(!constructor) {
	fprintf(stderr, "emcTaskInit: could not get symbol makeInterp from interpreter '%s': %s\n", shlib, dlerror());
	return 0;
    }
    InterpBase *pinterp = constructor();
    if(!pinterp) {
	fprintf(stderr, "emcTaskInit: makeInterp() returned NULL from interpreter '%s'\n", shlib);
	return 0;
    }
    return pinterp;
}
