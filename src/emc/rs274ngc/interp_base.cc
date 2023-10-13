/*
 * Copyright (C) 2013 Jeff Epler <jepler@unpythonic.net>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "interp_base.hh"
#include <dlfcn.h>
#include <limits.h>
#include <config.h>
#include <stdio.h>

InterpBase::~InterpBase() {}

InterpBase *interp_from_shlib(const char *shlib) {
    void * interp_lib;
    char relative_interp[PATH_MAX];
    char const * interp_path;

    dlopen(NULL, RTLD_GLOBAL);

    if (shlib[0] ==  '/') {
        // The passed-in .so name is an absolute path, use it directly.
        interp_path = shlib;
    } else {
        // The passed-in .so name is a relative path or just a bare
        // filename, look for it in `${EMC2_HOME}/lib/linuxcnc`.
        snprintf(relative_interp, sizeof(relative_interp), "%s/%s", EMC2_HOME "/lib/linuxcnc", shlib);
        interp_path = relative_interp;
    }

    interp_lib = dlopen(interp_path, RTLD_NOW);
    if(!interp_lib) {
        fprintf(stderr, "emcTaskInit: could not open interpreter '%s': %s\n", interp_path, dlerror());
        return 0;
    }
    fprintf(stderr, "emcTaskInit: using custom interpreter '%s'\n", interp_path);

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
