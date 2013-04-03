/********************************************************************
* Description:  rtapi_ring.c
*
*               This file, 'rtapi_ring.c', implements the ringbuffer
*               support functions for realtime modules, as far as they
*               are not defined as inline macros in rtapi_ring.h. See
*               rtapi_ring.h for more info.
*
*               Conceptually this is layered onto of the rtapi_shm*
*               functions and should not contain any flavor-specific code.
*
*               Note however that for kernel thread systems the
*               sequencing of shared memory creation still applies;
*               a ring must be created by an RT (in this case kernel)
*               module first before it can be accessed in userland.
********************************************************************/

#include "config.h"		// build configuration
#include "rtapi.h"		// these functions
#include "rtapi_common.h"
#include "rtapi_ring.h"

int _rtapi_ring_new(int size, int sp_size, int module_id, int flags)
{
    ring_data *rdptr __attribute__((cleanup(rtapi_autorelease_mutex)));
    size_t tmp_scratchpad_size, tmp_size;
    int req_size;     // size of ringbuffer_t.buf - depends on alignment per mode
    int i;
    ringheader_t *rhptr;

    rtapi_mutex_get(&(rtapi_data->mutex));

    // find a free slot
    for (i = 0 ; i < RTAPI_MAX_RINGS; i++) {
	if (ring_array[i].magic != RING_MAGIC)
	    break;
    }
    if (i == RTAPI_MAX_RINGS) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_ring_new failed due to RTAPI_MAX_RINGS exceeded\n");
	return -ENOMEM;
    }

    rdptr = &ring_array[i];

    // allocating a new ring.
    if (flags & MODE_STREAM) {
	// stream mode buffers need to be a power of two sized
	req_size = next_power_of_two(size);
    } else {
	// default to MODE_RECORD
	// round up buffer size to closest upper alignment boundary
	req_size = size_aligned(size);
    }

    // max-align the scratchpad
    tmp_scratchpad_size = size_aligned(sp_size);
    // make total allocation fit both ringbuffer and scratchpad
    tmp_size = req_size + tmp_scratchpad_size;

    // record creating module
    rdptr->owner = module_id;

    // allocate an RTAPI shm segment owned by the allocating module
    rdptr->key = RTAPI_RING_SHM_KEY + i;
    if ((rdptr->handle = _rtapi_shmem_new(rdptr->key,
					 rdptr->owner,
					 tmp_size)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_ring_new: rtapi_shmem_new(0x%8.8x,%d,%d) failed: %d\n",
			rdptr->key, rdptr->owner,
			tmp_size, rdptr->handle);
	return  -ENOMEM;
    }

    // map the segment now so we can fill in the
    // ringheader details
    if (_rtapi_shmem_getptr(rdptr->handle, (void **)&rhptr)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"_rtapi_ring_new: rtapi_shmem_getptr failed %d\n",
			rdptr->handle);
	return -ENOMEM;
    }

    // now that we have the shm alive, fill it in the details
    rhptr->scratchpad_size = tmp_scratchpad_size;
    rhptr->size = tmp_size;

    // init the ringheader - mode independent part
    rhptr->rmutex = rhptr->wmutex = 0;
    rhptr->reader = rhptr->writer = 0;
    rhptr->head = rhptr->tail = 0;

    // mode-dependent init
    if (flags &  MODE_STREAM) {
	rhptr->is_stream = 1;
	rhptr->size_mask = rhptr->size -1;
    } else {
	// default to MODE_RECORD
	rhptr->generation = 0;
    }

    // prime use count
    rdptr->count = 1;
    // mark as allocated
    rdptr->magic = RING_MAGIC;

    // return handle to the caller
    return i;
}

int _rtapi_ring_attach(int handle, ringbuffer_t *rbptr, int module_id)
{
    ring_data *rdptr;
    ringheader_t *rhptr;
    int retval;

    if (handle < 0 || handle >= RTAPI_MAX_RINGS)
	return -EINVAL;

    rdptr = &ring_array[handle];

    // validate ring handle
    if (rdptr->magic != RING_MAGIC)
	return -EINVAL;

    // attach the shm segment
    // attach the shm segment; since it is guaranteed to exist if a
    // valid hal_ring_t entry exists, use size 0 since we only attach here
    if ((retval = _rtapi_shmem_new(rdptr->key,
				  module_id,
				  0)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_ring_attach(): rtapi_shmem_new(key "
			"0x%8.8x owner %d ) failed:  %d\n",
			rdptr->key, module_id, retval);
	return  -ENOMEM;
    }

    // shm handles must agree (cant use assert()) here, this might be in-kernel)
    if (retval != rdptr->handle) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_ring_attach(): BUG: retval %d != rdptr->handle %d\n",
			retval, rdptr->handle);
	return -EINVAL;
    }
    // map the actual ring buffer
    if (_rtapi_shmem_getptr(rdptr->handle, (void **)&rhptr)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"_rtapi_ring_attach: rtapi_shmem_getptr failed %d\n",
			rdptr->handle);
	return -ENOMEM;
    }

    // pass address of ringheader to caller
    rbptr->header = rhptr;

    // set pointer to scratchpad if used - stored
    // right behind buffer area
    if (rhptr->scratchpad_size)
	rbptr->scratchpad = &rhptr->buf + rhptr->scratchpad_size;
    else
	rbptr->scratchpad = NULL;

    return 0;
}

// return -EINVAL if not a valid handle
// else return the RTAPI reference count
int _rtapi_ring_refcount(int handle)
{
    ring_data *rdptr __attribute__((cleanup(rtapi_autorelease_mutex)));

    rtapi_mutex_get(&(rtapi_data->mutex));
    if(handle < 0 || handle >= RTAPI_MAX_RINGS)
	return -EINVAL;

    rdptr = &ring_array[handle];
    if (rdptr->magic != RING_MAGIC)
	return -EINVAL;
    return rdptr->count;
}

int _rtapi_ring_detach(int handle, int module_id)
{
    ring_data *rdptr __attribute__((cleanup(rtapi_autorelease_mutex)));

    rtapi_mutex_get(&(rtapi_data->mutex));
    if(handle < 0 || handle >= RTAPI_MAX_RINGS)
	return -EINVAL;

    rdptr = &ring_array[handle];

    // validate ring handle
    if (rdptr->magic != RING_MAGIC)
	return -EINVAL;

    rdptr->count --;
    if(rdptr->count) {
	// ring is still referenced.
	rtapi_print_msg(RTAPI_MSG_DBG,
			"rtapi_ring_detach: handle=%d module=%d key=0x%x:  "
			"%d remaining users\n",
			handle, module_id, rdptr->key, rdptr->count);
	return 0;
    }
    // release RTAPI segment if use count dropped to zero:
    if (_rtapi_shmem_delete(rdptr->handle, rdptr->owner)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"_rtapi_ring_detach: rtapi_shmem_delete failed %d/%d\n",
			rdptr->handle, rdptr->owner);
    }
    // free the ring structure
    rdptr->magic = 0;

    return 0;
}

