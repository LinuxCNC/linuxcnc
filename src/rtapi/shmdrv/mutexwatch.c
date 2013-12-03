/********************************************************************
 * Copyright (C) 2012, 2013 Michael Haberler <license AT mah DOT priv DOT at>
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
 ********************************************************************/

// helper to watch mutexes 

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "config.h"

#include "rtapi.h"
#include "rtapi_common.h"
#include "rtapi_global.h"
#include "rtapi/shmdrv/shmdrv.h"
#include "hal.h"
#include "hal_priv.h"

global_data_t *global_data;
rtapi_data_t *rtapi_data;
hal_data_t *hal_data;

int shmdrv_loaded;
long page_size;

int gm = -1;
int rm = -1;
int rrm = -1;
int hm = -1;

int rtapi_instance = 0;

struct timespec looptime = {
    .tv_sec = 0,
    .tv_nsec = 1000 * 1000 * 100,
};

int main(int argc, char **argv)
{
    int globalkey,rtapikey,halkey,retval;
    int size;

    page_size = sysconf(_SC_PAGESIZE);
    shmdrv_loaded = shmdrv_available();

    if (argc > 1)
	rtapi_instance = atoi(argv[1]);

    globalkey = OS_KEY(GLOBAL_KEY, rtapi_instance);
    rtapikey = OS_KEY(RTAPI_KEY, rtapi_instance);
    halkey = OS_KEY(HAL_KEY, rtapi_instance);

    size = sizeof(global_data_t);
    retval = shm_common_new(globalkey, &size, 
			    rtapi_instance, (void **) &global_data, 0);
     if (retval < 0)
	 fprintf(stderr, "cannot attach global segment key=0x%x %s\n",
		globalkey, strerror(-retval));

    if (MMAP_OK(global_data) && (global_data->magic != GLOBAL_READY)) {
	printf("global_data MAGIC wrong: %x %x\n", global_data->magic, GLOBAL_READY);
    }

    size = sizeof(rtapi_data_t);
    retval = shm_common_new(rtapikey,  &size, 
			    rtapi_instance, (void **) &rtapi_data, 0);
    if (retval < 0)
	 fprintf(stderr, "cannot attach rtapi segment key=0x%x %s\n",
		rtapikey, strerror(-retval));

    if (MMAP_OK(rtapi_data) && (rtapi_data->magic != RTAPI_MAGIC)) {
	    printf("rtapi_data MAGIC wrong: %x\n", rtapi_data->magic);
    }

    if (MMAP_OK(global_data)) {
	size = global_data->hal_size;
	// global_data is needed for actual size of the HAL data segment
	retval = shm_common_new(halkey, &size, 
				rtapi_instance, (void **) &hal_data, 0);
	if (retval < 0)
	    fprintf(stderr, "cannot attach hal segment key=0x%x %s\n",
		    halkey, strerror(-retval));

	if (MMAP_OK(hal_data) && (hal_data->version != HAL_VER)) {
	    printf("hal_data HAL_VER wrong: %x\n", hal_data->version);
	}
    }

    if (!(MMAP_OK(global_data) || MMAP_OK(rtapi_data) || MMAP_OK(hal_data))) {
	printf("nothing to attach to!\n");
	exit(1);
    }

    do {
	if (nanosleep(&looptime, &looptime))
	    break;

	if (MMAP_OK(global_data) && (global_data->mutex != gm)) {
	    printf("global_data->mutex: %ld\n", global_data->mutex);
	    gm = global_data->mutex;
	}
	if (MMAP_OK(rtapi_data) && (rtapi_data->ring_mutex != rrm)) {
	    printf("rtapi_data->ring_mutex: %ld\n", rtapi_data->ring_mutex);
	    rrm = rtapi_data->ring_mutex;
	}
	if (MMAP_OK(rtapi_data) && (rtapi_data->mutex != rm)) {
	    printf("rtapi_data->mutex: %ld\n", rtapi_data->mutex);
	    rm = rtapi_data->mutex;
	}
	if (MMAP_OK(hal_data) && (hal_data->mutex != hm)) {
	    printf("hal_data->mutex: %ld\n", hal_data->mutex);
	    hm = hal_data->mutex;
	}

    } while (1);

    exit(0);
}
