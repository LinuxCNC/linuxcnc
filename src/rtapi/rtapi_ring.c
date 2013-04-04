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

// rtapi_data->ring_mutex is a private lock for ring operations
// since RTAPI mutexes are not recursive, layering of RTAPI calls 
// requires separate locks to avoid deadlocks on intra-RTAPI calls.
static void ring_autorelease_mutex(void *variable)
{
    if (rtapi_data != NULL)
	rtapi_mutex_give(&(rtapi_data->ring_mutex));
    else
	rtapi_print_msg(RTAPI_MSG_ERR,
			"ring_autorelease_mutex: rtapi_data == NULL!\n");
}

int _rtapi_ring_new(int size, int sp_size, int module_id, int flags)
{
    ring_data *rdptr  __attribute__((cleanup(ring_autorelease_mutex)));
    int ring_size;     // size of ringbuffer_t.buf - depends on alignment and mode
    int i, scratchpad_size;
    ringheader_t *rhptr;

    rtapi_mutex_get(&(rtapi_data->ring_mutex));

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

    if (flags & MODE_STREAM) {
	// stream mode buffers need to be a power of two sized
	ring_size = next_power_of_two(size);
    } else {
	// default to MODE_RECORD
	// round up buffer size to closest upper alignment boundary
	ring_size = size_aligned(size);
    }

    // max-align the scratchpad
    scratchpad_size = size_aligned(sp_size);
    // make total allocation fit ringheader, ringbuffer and scratchpad
    rdptr->total_size = sizeof(ringheader_t) + ring_size + scratchpad_size;
    rdptr->key = RTAPI_RING_SHM_KEY + i;
    rdptr->owner = module_id;

    // allocate an RTAPI shm segment owned by the allocating module
    if ((rdptr->handle = _rtapi_shmem_new(rdptr->key, module_id, rdptr->total_size)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_ring_new: rtapi_shmem_new(0x%8.8x,%d,%d) failed: %d\n",
			rdptr->key, module_id,
			rdptr->total_size, rdptr->handle);
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
    // in the ringheader
    rhptr->size = ring_size;
    rhptr->scratchpad_size = scratchpad_size;

    // init the ringheader - mode independent part
    rhptr->rmutex = rhptr->wmutex = 0;
    rhptr->reader = rhptr->writer = 0;  // FIXME right layer?
    rhptr->head = rhptr->tail = 0;

    // mode-dependent init
    if (flags &  MODE_STREAM) {
	rhptr->is_stream = 1;
	rhptr->size_mask = rhptr->size -1;
    } else {
	// default to MODE_RECORD
	rhptr->is_stream = 0;
	rhptr->generation = 0;
    }

    // prime use count
    rdptr->count = 1;
    // mark as committed
    rdptr->magic = RING_MAGIC;

    rtapi_mutex_give(&(rtapi_data->ring_mutex));

    return i; // return handle to the caller
}

int _rtapi_ring_attach(int handle, ringbuffer_t *ringbuffer_ptr, int module_id)
{
    ring_data *ring_data_ptr  __attribute__((cleanup(ring_autorelease_mutex)));
    ringheader_t *ringheader_ptr;

    if (handle < 0 || handle >= RTAPI_MAX_RINGS)
	return -EINVAL;

    ring_data_ptr = &ring_array[handle];
    ring_data_ptr->key = RTAPI_RING_SHM_KEY + handle;

    // attach the shm segment
    // attach the shm segment; since it is guaranteed to exist if a
    // valid hal_ring_t entry exists, use size 0 since we only attach here
    if ((ring_data_ptr->handle = _rtapi_shmem_new(ring_data_ptr->key,
				   module_id,
				   0)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_ring_attach(): rtapi_shmem_new(key "
			"0x%8.8x owner %d ) failed:  %d\n",
			ring_data_ptr->key, module_id, ring_data_ptr->handle);
	return  -ENOMEM;
    }
#if 0
    // shm handles must agree (cant use assert()) here, this might be in-kernel)
    if ( ring_data_ptr->handle != handle) {
	// FIXME this is the ring handle, not the shm handle!!
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_ring_attach(): BUG: retval %d != ring_data_ptr->handle %d\n",
			ring_data_ptr->handle, handle);
	return -EINVAL;
    }

#endif
    // map the actual ring buffer
    if (_rtapi_shmem_getptr(ring_data_ptr->handle, (void **)&ringheader_ptr)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"_rtapi_ring_attach: rtapi_shmem_getptr failed %d\n",
			ring_data_ptr->handle);
	return -ENOMEM;
    }

    ring_data_ptr->magic = RING_MAGIC;
    ring_data_ptr->count++;

    // pass address of ringheader to caller
    ringbuffer_ptr->header = ringheader_ptr;
    ringbuffer_ptr->buf = &ringheader_ptr->buf[0];

    // set pointer to scratchpad if used - stored
    // right behind buffer area
    if (ringheader_ptr->scratchpad_size)
	ringbuffer_ptr->scratchpad = &ringheader_ptr->buf + ringheader_ptr->scratchpad_size;
    else
	ringbuffer_ptr->scratchpad = NULL;

    return 0;
}

// return -EINVAL if not a successfully attached ring
// else return the RTAPI reference count
int _rtapi_ring_refcount(int handle)
{
    ring_data *rdptr __attribute__((cleanup(ring_autorelease_mutex)));

    rtapi_mutex_get(&(rtapi_data->ring_mutex));
    if(handle < 0 || handle >= RTAPI_MAX_RINGS)
	return -EINVAL;

    rdptr = &ring_array[handle];
    if (rdptr->magic != RING_MAGIC)
	return -EINVAL;
    return rdptr->count;
}

int _rtapi_ring_detach(int handle, int module_id)
{
    ring_data *rdptr __attribute__((cleanup(ring_autorelease_mutex)));

    if(handle < 0 || handle >= RTAPI_MAX_RINGS)
	return -EINVAL;

    rdptr = &ring_array[handle];

    // validate ring handle
    if (rdptr->magic != RING_MAGIC)
	return -EINVAL;

    rdptr->count--;
    if(rdptr->count) {
	// ring is still referenced.
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_ring_detach: handle=%d module=%d key=0x%x:  "
			"%d remaining users\n",
			handle, module_id, rdptr->key, rdptr->count);
	return 0;
    }
    // release shm segment if use count dropped to zero:
    if (_rtapi_shmem_delete(rdptr->handle, rdptr->owner)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"_rtapi_ring_detach: rtapi_shmem_delete failed %d/%d\n",
			rdptr->handle, rdptr->owner);
    }

    // if we get here, the last using module detached
    // so free the ring_data entry.
    rdptr->count = 0;
    rdptr->magic = 0;

    return 0;
}

