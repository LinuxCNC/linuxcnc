/********************************************************************
* Description:  rtapi_shmem.c
*
*               This file, 'rtapi_shmem.c', implements the shared
*               memory-related functions for realtime modules.  See
*               rtapi.h for more info.
********************************************************************/

#include "config.h"		// build configuration
#include "rtapi.h"		// these functions
#include "rtapi_common.h"

#ifdef MODULE
#include <linux/module.h>	/* EXPORT_SYMBOL */
#endif

#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>		/* shmget() */
#include <stdlib.h>		/* rand_r() */
#include <unistd.h>		/* getuid(), getgid(), sysconf(),
				   ssize_t, _SC_PAGESIZE */

#define SHMEM_MAGIC   25453	/* random numbers used as signatures */
#define SHM_PERMISSIONS	0666


/***********************************************************************
*                           USERLAND THREADS                           *
************************************************************************/

int rtapi_shmem_new(int key, int module_id, unsigned long int size)
{
    shmem_data *shmem;
    struct shmid_ds d;
    int i, ret;
    int is_new = 0;

    rtapi_mutex_get(&(rtapi_data->mutex));
    for (i=0 ; i < RTAPI_MAX_SHMEMS; i++) {
	if (shmem_array[i].magic == SHMEM_MAGIC && shmem_array[i].key == key) {
	    shmem_array[i].count ++;
	    rtapi_mutex_give(&(rtapi_data->mutex));
	    return i;
	}
	if (shmem_array[i].magic != SHMEM_MAGIC)
	    break;
    }
    if (i == RTAPI_MAX_SHMEMS) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_shmem_new failed due to RTAPI_MAX_SHMEMS\n");
	return -ENOMEM;
    }
    shmem = &shmem_array[i];

    /* now get shared memory block from OS */

    // try to attach
    shmem->id = shmget((key_t)key, size, SHM_PERMISSIONS);
    if (shmem->id == -1) {  
	if (errno == ENOENT) {
	    // nope, doesnt exist - create
	    shmem->id = shmget((key_t)key, size, SHM_PERMISSIONS | IPC_CREAT);
	    is_new = 1;
	}
	if (shmem->id == -1) {
	    rtapi_mutex_give(&(rtapi_data->mutex));
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "Failed to allocate shared memory, "
			    "key=0x%x size=%ld\n", key, size);
	    return -ENOMEM;
	}
    } 
  
    // get actual user/group and drop to ruid/rgid so removing is
    // always possible
    if ((ret = shmctl(shmem->id, IPC_STAT, &d)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_shmem_new: shm_ctl(key=0x%x, IPC_STAT) "
			"failed: %d '%s'\n", 
			key, errno, strerror(errno));      
    } else {
	// drop permissions of shmseg to real userid/group id
	if (!d.shm_perm.uid) { // uh, root perms 
	    d.shm_perm.uid = getuid();
	    d.shm_perm.gid = getgid();
	    if ((ret = shmctl(shmem->id, IPC_SET, &d)) < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"rtapi_shmem_new: shm_ctl(key=0x%x, IPC_SET) "
				"failed: %d '%s'\n", 
				key, errno, strerror(errno));      
	    } 
	}
    }
    /* and map it into process space */
    shmem->mem = shmat(shmem->id, 0, 0);
    if ((ssize_t) (shmem->mem) == -1) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_shmem_new: shmat(%d) failed: %d '%s'\n",
			shmem->id, errno, strerror(errno));
	return -errno;
    }
    /* Touch each page by either zeroing the whole mem (if it's a new
       SHM region), or by reading from it. */
    if (is_new) {
	memset(shmem->mem, 0, size);
    } else {
	unsigned int i, pagesize;
      
	pagesize = sysconf(_SC_PAGESIZE);
	for (i = 0; i < size; i += pagesize) {
	    unsigned int x = *(volatile unsigned int *)
		((unsigned char *)shmem->mem + i);
	    /* Use rand_r to clobber the read so GCC won't optimize it
	       out. */
	    rand_r(&x);
	}
    }

    /* label as a valid shmem structure */
    shmem->magic = SHMEM_MAGIC;
    /* fill in the other fields */
    shmem->size = size;
    shmem->key = key;
    shmem->count = 1;

    rtapi_mutex_give(&(rtapi_data->mutex));

    /* return handle to the caller */
    return i;
}


int rtapi_shmem_getptr(int handle, void **ptr)
{
    shmem_data *shmem;
    if (handle < 0 || handle >= RTAPI_MAX_SHMEMS)
	return -EINVAL;

    shmem = &shmem_array[handle];

    /* validate shmem handle */
    if (shmem->magic != SHMEM_MAGIC)
	return -EINVAL;

    /* pass memory address back to caller */
    *ptr = shmem->mem;
    return 0;
}


int rtapi_shmem_delete(int handle, int module_id)
{
    struct shmid_ds d;
    int r1, r2;
    shmem_data *shmem;

    if(handle < 0 || handle >= RTAPI_MAX_SHMEMS)
	return -EINVAL;

    rtapi_mutex_get(&(rtapi_data->mutex));
    shmem = &shmem_array[handle];

    /* validate shmem handle */
    if (shmem->magic != SHMEM_MAGIC) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }

    shmem->count --;
    if(shmem->count) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_DBG,
			"rtapi_shmem_delete: handle=%d module=%d key=0x%x:  "
			"%d remaining users\n", 
			handle, module_id, shmem->key, shmem->count);
	return 0;
    }

    /* unmap the shared memory */
    r1 = shmdt(shmem->mem);
    if (r1 < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_shmem_delete: shmdt(key=0x%x) "
			"failed: %d '%s'\n",
			shmem->key, errno, strerror(errno));      
    }
    /* destroy the shared memory */
    r2 = shmctl(shmem->id, IPC_STAT, &d);
    if (r2 < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_shmem_delete: shm_ctl(0x%x, IPC_STAT) "
			"failed: %d '%s'\n", 
			shmem->key, errno, strerror(errno));      
    }
    if(r2 == 0 && d.shm_nattch == 0) {
	r2 = shmctl(shmem->id, IPC_RMID, &d);
	if (r2 < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "rtapi_shmem_delete: shm_ctl(0x%x, IPC_RMID) "
			    "failed: %d '%s'\n", 
			    shmem->key, errno, strerror(errno));      
	}
    }  

    /* free the shmem structure */
    shmem->magic = 0;

    rtapi_mutex_give(&(rtapi_data->mutex));

    if ((r1 != 0) || (r2 != 0))
	return -EINVAL;
    return 0;
}



