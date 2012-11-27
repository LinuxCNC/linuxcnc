
#include "config.h"
#include "rtapi.h"
#include "rtapi_common.h"

/* fill out Xenomai-specific fields in rtapi_data */
void init_rtapi_data_hook(rtapi_data_t * data) {
    data->rt_wait_error = 0;
    data->rt_last_overrun = 0;
    data->rt_total_overruns = 0;

#if 0
    for (n = 0; n <= RTAPI_MAX_SHMEMS; n++) {
	memset(&shmem_heap_array[n].heap, 0, sizeof(shmem_heap_array[n]));
    }
#endif

}

