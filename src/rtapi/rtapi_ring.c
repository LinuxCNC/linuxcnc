/********************************************************************
* Description:  rtapi_ring.c
*
*               This file, 'rtapi_ring.c', implements the ringbuffer
*               support function as far as they are not defined as 
*               inline macros in rtapi_ring.h. See rtapi_ring.h for 
*               more info.
*
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

#if defined(BUILD_SYS_USER_DSO)
#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>		/* shmget() */

void *ring_addr_array[RTAPI_MAX_RINGS + 1];
#endif

// rtapi_data->ring_mutex is a private lock for ring operations.
// Since RTAPI mutexes are not recursive, layering locking RTAPI functions
// ontop of each other requires separate locks to avoid deadlocks on 
// intra-RTAPI calls.
static void ring_autorelease_mutex(void *variable)
{
    if (rtapi_data != NULL) {

#if 0
	// this is very likely a programming error: a scope
	// was exited and the mutex not held there
	if (!test_bit(0, &(rtapi_data->ring_mutex))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "ring_autorelease_mutex: mutex not set!\n");
	}
#endif
	rtapi_mutex_give(&(rtapi_data->ring_mutex));
    } else
	// and this too
	rtapi_print_msg(RTAPI_MSG_ERR,
			"ring_autorelease_mutex: rtapi_data == NULL!\n");
}

int _rtapi_ring_new(size_t size, size_t sp_size, int module_id, int flags)
{
    ring_data *rdptr  __attribute__((cleanup(ring_autorelease_mutex)));
    int i;
    ringheader_t *rhptr;
    size_t total_size;

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

    // make total allocation fit ringheader, ringbuffer and scratchpad
    total_size = rtapi_ring_memsize( flags, size, sp_size);

    rdptr->key = RTAPI_RING_SHM_KEY + i;
    rdptr->owner = module_id;

    // allocate an RTAPI shm segment owned by the allocating module
    if ((rdptr->shmem_id = _rtapi_shmem_new(rdptr->key, module_id, total_size)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_ring_new: rtapi_shmem_new(0x%8.8x,%d,%d) failed: %d\n",
			rdptr->key, module_id,
			total_size, rdptr->shmem_id);
	return  -ENOMEM;
    }

    // map the segment now so we can fill in the ringheader details
    if (_rtapi_shmem_getptr(rdptr->shmem_id, (void **)&rhptr)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"_rtapi_ring_new: rtapi_shmem_getptr failed %d\n",
			rdptr->shmem_id);
	return -ENOMEM;
    }

#if defined(BUILD_SYS_USER_DSO)
    ring_addr_array[i] = rhptr; // record local mapping
#endif

    rtapi_ringheader_init(rhptr, flags, size, sp_size);

    // once refcount is 0 in rtapi_ring_detach(), the 
    // shm segment may be freed and a corresponding HAL name deleted
    // the usage assumption is that the module creating the ring
    // will detach on rtapi_app_exit() and hence delete the ring.
    rhptr->refcount = 1;

    // mark as committed
    rdptr->magic = RING_MAGIC;

    return i; // return handle to the caller
}

int _rtapi_ring_attach(int handle, ringbuffer_t *rbptr, int module_id)
{
    ring_data *rdptr  __attribute__((cleanup(ring_autorelease_mutex)));
    ringheader_t *rhptr;

    rtapi_mutex_get(&(rtapi_data->ring_mutex));
    if (handle < 0 || handle >= RTAPI_MAX_RINGS)
	return -EINVAL;
    rdptr = &ring_array[handle];

    if (rdptr->magic == RING_MAGIC) {

#if defined(BUILD_SYS_KBUILD)
	int shmid, retval;

	// the ringbuffer exists, but this module has not yet
	// attached this ringbuffer.

	rdptr->key = RTAPI_RING_SHM_KEY + handle;
	if ((shmid = _rtapi_shmem_new(rdptr->key, module_id, 0)) < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "rtapi_ring_attach(): rtapi_shmem_new(key "
			    "0x%8.8x owner %d ) failed:  %d\n",
			    rdptr->key, module_id, shmid);
	    return  -ENOMEM;
	}

	if ((retval = _rtapi_shmem_getptr(shmid, (void **)&rhptr))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "_rtapi_ring_attach(%d): rtapi_shmem_getptr failed %d\n",
			    handle, retval);
	    return -ENOMEM;
	}
#else
	// in-process attach - already mapped
	rhptr = ring_addr_array[handle];
	if (rhptr == NULL) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "_rtapi_ring_attach(%d): BUG rhptr == NULL\n",
			    handle);
	    return -ENOMEM;
	}
#endif
    } else {
#if defined(BUILD_SYS_KBUILD)
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_ring_attach: invalid ring handle %d\n",
			handle);
	return -ENOMEM;
#else
	// not yet attached, or non-existent
	// test if the shm segment exists, else fail.
	key_t key = OS_KEY((RTAPI_RING_SHM_KEY + handle));

	if ((shmget(key, 1, 0) == -1) && (errno == ENOENT)) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "rtapi_ring_attach: invalid ring handle %d\n",
			    handle);
	    return -EINVAL;
	}
	// attach the shm segment; since we just tested for existence
	// use size 0
	rdptr->key = RTAPI_RING_SHM_KEY + handle;
	if ((rdptr->shmem_id = _rtapi_shmem_new(rdptr->key, module_id, 0)) < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "rtapi_ring_attach(): rtapi_shmem_new(key "
			    "0x%8.8x owner %d ) failed:  %d\n",
			    rdptr->key, module_id, rdptr->shmem_id);
	    return  -ENOMEM;
	}

	// map the actual ring buffer & record in process-local mapping
	if (_rtapi_shmem_getptr(rdptr->shmem_id, (void **)&ring_addr_array[handle])) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "_rtapi_ring_attach: rtapi_shmem_getptr failed %d\n",
			    rdptr->shmem_id);
	    return -ENOMEM;
	}
	rhptr = ring_addr_array[handle];
	rdptr->magic = RING_MAGIC;
#endif
    }

    // record usage in ringheader
    rhptr->refcount++;
    rtapi_print_msg(RTAPI_MSG_DBG,
		    "rtapi_ring_attach: handle=%d module=%d key=0x%x:  "
		    "now %d users\n",
		    handle, module_id, rdptr->key, rhptr->refcount);
    // fill in ringbuffer_t
    rtapi_ringbuffer_init(rhptr, rbptr);
    return 0;
}

// returns < 0 on error or the remaining refcount for the ring
int _rtapi_ring_detach(int handle, int module_id)
{
    ring_data *rdptr __attribute__((cleanup(ring_autorelease_mutex)));
    ringheader_t *rhptr;

    rtapi_mutex_get(&(rtapi_data->ring_mutex));
    if(handle < 0 || handle >= RTAPI_MAX_RINGS)
	return -EINVAL;

    rdptr = &ring_array[handle];

    // validate ring handle
    if (rdptr->magic != RING_MAGIC)
	return -EINVAL;

    // retrieve ringheader address to decrease refcount
    if (_rtapi_shmem_getptr(rdptr->shmem_id, (void **)&rhptr)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"_rtapi_ring_detach: rtapi_shmem_getptr failed %d\n",
			rdptr->shmem_id);
	return -ENOMEM;
    }
    rhptr->refcount--;

    if (rhptr->refcount) {
	// ring is still referenced.
	rtapi_print_msg(RTAPI_MSG_DBG,
			"rtapi_ring_detach: handle=%d module=%d key=0x%x:  "
			"%d remaining users\n",
			handle, module_id, rdptr->key, rhptr->refcount);
	return rhptr->refcount;
    } else {
	rtapi_print_msg(RTAPI_MSG_DBG,
			"rtapi_ring_detach: handle=%d module=%d key=0x%x:  "
			"refcount 0, releasing\n",
			handle, module_id, rdptr->key);

    }
    // release shm segment if use count dropped to zero:
    if (_rtapi_shmem_delete(rdptr->shmem_id, rdptr->owner)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"_rtapi_ring_detach: rtapi_shmem_delete failed %d/%d\n",
			rdptr->shmem_id, rdptr->owner);
    }

    // if we get here, the last using module detached
    // so free the ring_data entry.
    rdptr->magic = 0;
    return 0;
}

