// HAL memory allocator functions

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"

// part of public API
void *hal_malloc(long int size)
{
    void *retval;

    if (hal_data == 0) {
	HALERR("hal_malloc called before init");
	return 0;
    }
    /* get the mutex */
    rtapi_mutex_get(&(hal_data->mutex));
    /* allocate memory */
    retval = shmalloc_up(size);
    /* release the mutex */
    rtapi_mutex_give(&(hal_data->mutex));
    /* check return value */
    if (retval == 0) {
	HALDBG("hal_malloc() can't allocate %ld bytes", size);
    }
    return retval;
}

// HAL library internal use only

void *shmalloc_up(long int size)
{
    long int tmp_bot;
    void *retval;

    /* deal with alignment requirements */
    tmp_bot = hal_data->shmem_bot;
    if (size >= 8) {
	/* align on 8 byte boundary */
	tmp_bot = (tmp_bot + 7) & (~7);
    } else if (size >= 4) {
	/* align on 4 byte boundary */
	tmp_bot = (tmp_bot + 3) & (~3);
    } else if (size == 2) {
	/* align on 2 byte boundary */
	tmp_bot = (tmp_bot + 1) & (~1);
    }
    /* is there enough memory available? */
    if ((hal_data->shmem_top - tmp_bot) < size) {
	/* no */
	return 0;
    }
    /* memory is available, allocate it */
    retval = SHMPTR(tmp_bot);
    hal_data->shmem_bot = tmp_bot + size;
    hal_data->shmem_avail = hal_data->shmem_top - hal_data->shmem_bot;
    return retval;
}

void *shmalloc_dn(long int size)
{
    long int tmp_top;
    void *retval;

    /* tentatively allocate memory */
    tmp_top = hal_data->shmem_top - size;
    /* deal with alignment requirements */
    if (size >= 8) {
	/* align on 8 byte boundary */
	tmp_top &= (~7);
    } else if (size >= 4) {
	/* align on 4 byte boundary */
	tmp_top &= (~3);
    } else if (size == 2) {
	/* align on 2 byte boundary */
	tmp_top &= (~1);
    }
    /* is there enough memory available? */
    if (tmp_top < hal_data->shmem_bot) {
	/* no */
	return 0;
    }
    /* memory is available, allocate it */
    retval = SHMPTR(tmp_top);
    hal_data->shmem_top = tmp_top;
    hal_data->shmem_avail = hal_data->shmem_top - hal_data->shmem_bot;
    return retval;
}
