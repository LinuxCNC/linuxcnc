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
    struct shm_status sm; 

    page_size = sysconf(_SC_PAGESIZE);
    shmdrv_loaded = shmdrv_available();

    if (argc > 1)
	rtapi_instance = atoi(argv[1]);

    sm.driver_fd = shmdrv_driver_fd();
    sm.key = OS_KEY(GLOBAL_KEY, rtapi_instance);
    sm.size = sizeof(global_data_t);
    sm.flags = 0;

    if (!shmdrv_attach(&sm, (void **)&global_data))
	printf("global_data attached %p hal_size=%d\n", 
	       global_data, global_data->hal_size);

    if ((global_data !=  NULL) && (global_data->magic != GLOBAL_MAGIC)) {
	printf("global_data MAGIC wrong: %x %x\n", global_data->magic, GLOBAL_MAGIC);
    }

    sm.driver_fd = shmdrv_driver_fd();
    sm.key = OS_KEY(RTAPI_KEY, rtapi_instance);
    sm.size = sizeof(rtapi_data_t);
    sm.flags = 0;

    if (!shmdrv_attach(&sm, (void **)&rtapi_data))
	printf("rtapi_data attached%p\n", rtapi_data);

    if (rtapi_data && (rtapi_data->magic != RTAPI_MAGIC)) {
	    printf("rtapi_data MAGIC wrong: %x\n", rtapi_data->magic);
    }

    sm.driver_fd = shmdrv_driver_fd();
    sm.key = OS_KEY(HAL_KEY, rtapi_instance);
    sm.size = global_data->hal_size; // sizeof(hal_data_t);
    sm.flags = 0;

    if (!shmdrv_attach(&sm, (void **)&hal_data))
	printf("hal_data attached%p\n", hal_data);

    if (hal_data && (hal_data->version != HAL_VER)) {
	    printf("hal_data HAL_VER wrong: %x\n", hal_data->version);
    }

    if (!(global_data||rtapi_data||hal_data)) {
	printf("nothing to attach to!\n");
	exit(1);
    }

    do {
	if (nanosleep(&looptime, &looptime))
	    break;

	if (global_data && (global_data->mutex != gm)) {
	    printf("global_data->mutex: %ld\n", global_data->mutex);
	    gm = global_data->mutex;
	}
	if (rtapi_data && (rtapi_data->ring_mutex != rrm)) {
	    printf("rtapi_data->ring_mutex: %ld\n", rtapi_data->ring_mutex);
	    rrm = rtapi_data->ring_mutex;
	}
	if (rtapi_data && (rtapi_data->mutex != rm)) {
	    printf("rtapi_data->mutex: %ld\n", rtapi_data->mutex);
	    rm = rtapi_data->mutex;
	}
	if (hal_data && (hal_data->mutex != hm)) {
	    printf("hal_data->mutex: %ld\n", hal_data->mutex);
	    hm = hal_data->mutex;
	}

    } while (1);

    exit(0);
}
