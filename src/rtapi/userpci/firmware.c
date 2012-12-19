/********************************************************************
 *  Description: firmware.c
 *  Provides functions for loading firmware when running kernel 
 *  modules in user space, emulating part of
 *  #include <linux/firmware.h>
 *
 *  Author(s): Charles Steinkuehler
 *  License: GNU LGPL Version 2.1 or (at your option) any later version.
 *
 *  Last change: 
 *  2012-Dec-17 Charles Steinkuehler
 *              Initial version
 ********************************************************************/

/********************************************************************
 *  This file is part of LinuxCNC RTAPI / HAL
 *
 *  Copyright (C) 2012  Charles Steinkuehler
 *                      <charles AT steinkuehler DOT net>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
 *  ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
 *  TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
 *  harming persons must have provisions for completely removing power
 *  from all motors, etc, before persons enter any danger area.  All
 *  machinery must be designed to comply with local and national safety
 *  codes, and the authors of this software can not, and do not, take
 *  any responsibility for such compliance.
 *
 *  This code was written as part of the LinuxCNC RTAPI project.  For 
 *  more information, go to www.linuxcnc.org.
 ********************************************************************/

#include "config_module.h"
#include RTAPI_INC_FIRMWARE_H

#include <stdlib.h>     // malloc/free
#include <stdio.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Look for firmware file in a few hard-coded locations:
//  /lib/firmware/(uname -r)/
//  /lib/firmware/
int request_firmware(const struct firmware **fw, const char *name,
                     struct device *device) {

    struct firmware *lfw;
    struct utsname sysinfo;
    char *basepath = "/lib/firmware";
    char path[256];
    struct stat st;
    int r;

    /* Allocate and initialize a firmware struct */
    lfw = malloc(sizeof(*lfw));
    if (lfw == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR, "Out of memory\n");
        return -ENOMEM;
    }
    memset((void*)lfw, 0, sizeof(*lfw));
    lfw->data = NULL;

    /* Try to open the kernel-specific file */
    r = uname(&sysinfo);
    if (r >= 0) {
    	snprintf(path, sizeof(path), "/%s/%s/%s", basepath, sysinfo.release, name);
	lfw->fd = open(path, O_RDONLY);
    }

    /* If we don't have a valid file descriptor yet, try an alternate location */
    if (lfw->fd < 0) {
    	snprintf(path, sizeof(path), "/%s/%s", basepath, name);
	lfw->fd = open(path, O_RDONLY);
    }

    /* If we don't have a valid file descriptor by here, it's an error */
    if (lfw->fd < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "Could not locate firmware \"%s\". (%s)\n",
			path, strerror(errno));
	return -ENOENT;
    }

    /* We've found and oepned the file, now let's get the size */
    if (stat(path, &st) == 0) {
        lfw->size = st.st_size;
    }
    else {
	rtapi_print_msg(RTAPI_MSG_ERR, "Could not determine size of file \"%s\". (%s)\n",
			path, strerror(errno));
	return -1;
    }

    lfw->data = mmap(NULL, lfw->size, PROT_READ, MAP_PRIVATE, lfw->fd, 0);

    if (lfw->data == NULL || lfw->data == MAP_FAILED) {
	if (lfw->data == NULL)
	    munmap((void*)lfw->data, lfw->size);
	rtapi_print_msg(RTAPI_MSG_ERR, "Failed to mmap file %s\n", path);
	return -1;
    }

    *fw = lfw;
    return 0;
}

void release_firmware(const struct firmware *fw) {
    munmap((void*)fw->data, fw->size);
    close(fw->fd);
    free((void*)fw);
}



