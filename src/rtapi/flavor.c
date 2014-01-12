/* Copyright (C) 2012, 2013 Michael Haberler <license AT mah DOT priv DOT at>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

// inspect the current default flavor.

#include "rtapi.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include "rtapi.h"
#include "rtapi_compat.h"

int shmdrv_loaded;
size_t page_size;

int main(int argc, char **argv)
{
    char *progname = argv[0];
    flavor_ptr flavor;

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
