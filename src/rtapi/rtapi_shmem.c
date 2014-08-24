/********************************************************************
* Description:  rtapi_shmem.c
*
*               This file, 'rtapi_shmem.c', implements the shared
*               memory-related functions for realtime modules.  See
*               rtapi.h for more info.
*
*     Copyright 2006-2013 Various Authors
* 
*     This program is free software; you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation; either version 2 of the License, or
*     (at your option) any later version.
* 
*     This program is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
* 
*     You should have received a copy of the GNU General Public License
*     along with this program; if not, write to the Free Software
*     Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
********************************************************************/

#include "config.h"		// build configuration
#include "rtapi.h"		// these functions
#include "rtapi_common.h"
#include "rtapi/shmdrv/shmdrv.h"

#ifdef BUILD_SYS_USER_DSO
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>          //shm_open
#include <stdlib.h>		/* rand_r() */
#include <unistd.h>		/* getuid(), getgid(), sysconf(),
				   ssize_t, _SC_PAGESIZE */

#else  /* BUILD_SYS_KBUILD */
#  ifdef ULAPI
#    include <stdio.h>          // perror
#    include <sys/time.h>
#    include <sys/resource.h>
#    include "rtapi/shmdrv/shmdrv.h"
#  endif
#endif

#define SHM_PERMISSIONS	0666

#ifdef BUILD_SYS_KBUILD
#  ifdef RTAPI
#    define MODULE_STATE REALTIME
#    define OUR_API "RTAPI"
#  else
#    define MODULE_STATE USERSPACE
#    define OUR_API "ULAPI"
#  endif
#endif

#if defined(BUILD_SYS_KBUILD) && defined(ULAPI)
static void check_memlock_limit(const char *where);
#endif


#ifdef BUILD_SYS_KBUILD
void *shmem_addr_array[RTAPI_MAX_SHMEMS + 1];
#endif


#ifdef BUILD_SYS_USER_DSO
/***********************************************************************
*                           USERLAND THREADS                           *
************************************************************************/

int _rtapi_shmem_new_inst(int userkey, int instance, int module_id, unsigned long int size) {
    shmem_data *shmem;
    int i, ret, actual_size;
    int is_new = 0;
    int key = OS_KEY(userkey, instance);
    static int page_size;

    if (!page_size)
	page_size = sysconf(_SC_PAGESIZE);


    rtapi_mutex_get(&(rtapi_data->mutex));
    for (i = 1 ; i < RTAPI_MAX_SHMEMS; i++) {
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

    // redefine size == 0 to mean 'attach only, dont create'
    actual_size = size;
    ret = shm_common_new(key, &actual_size, instance, &shmem->mem, size > 0);
    if (ret > 0)
	is_new = 1;
    if (ret < 0) {
	 rtapi_mutex_give(&(rtapi_data->mutex));
	 rtapi_print_msg(RTAPI_MSG_ERR,
			 "shm_common_new:%d failed key=0x%x size=%ld\n",
			 instance, key, size);
	 return ret;
    }
    // a non-zero size was given but it didn match what we found:
    if (size && (actual_size != size)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rtapi_shmem_new:%d 0x8.8%x: requested size %ld and actual size %d dont match\n",
			instance, key, size, actual_size);
    }
    /* Touch each page by either zeroing the whole mem (if it's a new
       SHM region), or by reading from it. */
    if (is_new) {
	memset(shmem->mem, 0, size);
    } else {
	unsigned int i;

	for (i = 0; i < size; i += page_size) {
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
    shmem->size = actual_size;
    shmem->key = key;
    shmem->count = 1;
    shmem->instance = instance;

    rtapi_mutex_give(&(rtapi_data->mutex));

    /* return handle to the caller */
    return i;
}

int _rtapi_shmem_getptr_inst(int handle, int instance, void **ptr, unsigned long *size) {
    shmem_data *shmem;
    if (handle < 1 || handle >= RTAPI_MAX_SHMEMS)
	return -EINVAL;

    shmem = &shmem_array[handle];

    /* validate shmem handle */
    if (shmem->magic != SHMEM_MAGIC)
	return -ENOENT;

    /* pass memory address back to caller */
    *ptr = shmem->mem;
    if (size)
	*size = shmem->size;
    return 0;
}

int _rtapi_shmem_delete_inst(int handle, int instance, int module_id) {
    shmem_data *shmem;
    int retval = 0;

    if(handle < 1 || handle >= RTAPI_MAX_SHMEMS)
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

    retval = shm_common_detach(shmem->size, shmem->mem);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d ERROR: munmap(0x%8.8x) failed: %s\n",
			instance, shmem->key, strerror(-retval));
    }

    // XXX: probably shmem->mem should be set to NULL here to avoid
    // references to already unmapped segments (and find them early)

    /* free the shmem structure */
    shmem->magic = 0;
    rtapi_mutex_give(&(rtapi_data->mutex));
    return retval;
}

int _rtapi_shmem_exists(int userkey) {
    return shm_common_exists(userkey);
}

#else  /* BUILD_SYS_KBUILD */
/***********************************************************************
*                            KERNEL THREADS                            *
************************************************************************/

int _rtapi_shmem_new_inst(int userkey, int instance, int module_id, unsigned long int size) {
    int n, retval;
    int shmem_id;
    shmem_data *shmem;
    struct shm_status sm;

    int key = OS_KEY(userkey, instance);

    /* key must be non-zero, and also cannot match the key that RTAPI uses */
    if ((key == 0) || (key == OS_KEY(RTAPI_KEY, instance))) {
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: bad shmem key: 0x%x\n",
			key);
	return -EINVAL;
    }
    /* get the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    /* validate module_id */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: bad module ID: %d\n",
			module_id);
	return -EINVAL;
    }
    if (module_array[module_id].state != MODULE_STATE) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: ERROR: not a " OUR_API " module ID: %d\n",
			module_id);
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }

    /* check if a block is already open for this key */
    for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
	if (shmem_array[n].key == key) {
	    /* found a match */
	    shmem_id = n;
	    shmem = &(shmem_array[n]);
	    /* is it big enough? */
	    if (shmem->size < size) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		rtapi_print_msg(RTAPI_MSG_ERR,
				"RTAPI: ERROR: shmem size mismatch: request: %ld actual: %ld\n",
				size, shmem->size);
		return -EINVAL;
	    }
	    /* is this module already using it? */
	    // redefine size == 0 to mean 'attach only, dont create'
	    if (rtapi_test_bit(module_id, shmem->bitmap) && (size > 0)) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		rtapi_print_msg(RTAPI_MSG_WARN,
				"RTAPI: Warning: shmem %d key 0x%x already mapped\n",
				n, key);
		return -EEXIST;
	    }
	    /* yes, has it been mapped into kernel space? */
#ifdef RTAPI
	    if (shmem->rtusers == 0) {
#endif
		/* no, map it and save the address */
		sm.key = key;
		sm.size = size;
		sm.flags = 0;
#ifdef ULAPI
		sm.driver_fd = shmdrv_driver_fd();
#endif
		retval = shmdrv_attach(&sm, &shmem_addr_array[shmem_id]);
		if (retval < 0) {
		    rtapi_mutex_give(&(rtapi_data->mutex));
		    rtapi_print_msg(RTAPI_MSG_ERR,
				    "shmdrv attached failed key=0x%x size=%ld\n", key, size);
		    return retval;
		}
		if (shmem_addr_array[shmem_id] == NULL) {
		    rtapi_print_msg(RTAPI_MSG_ERR,
				    "RTAPI: ERROR: failed to map shmem %d\n", shmem_id);
		    rtapi_mutex_give(&(rtapi_data->mutex));
#ifdef ULAPI
		    check_memlock_limit("failed to map shmem");
#endif
		    return -ENOMEM;
		}
#ifdef RTAPI
	    }
#endif

	    // increase refcount only if not already used
	    if (!rtapi_test_bit(module_id, shmem->bitmap)) {
#ifdef ULAPI
		shmem->ulusers++;
#else  /* RTAPI */
		shmem->rtusers++;
#endif  /* RTAPI */
	    }
	    /* update usage data */
	    rtapi_set_bit(module_id, shmem->bitmap);

	    /* announce another user for this shmem */
	    rtapi_print_msg(RTAPI_MSG_DBG,
		"RTAPI: shmem %02d 0x%x opened by module %02d\n",
			    shmem_id, key, module_id);
	    /* done */
	    rtapi_mutex_give(&(rtapi_data->mutex));
	    return shmem_id;
	}
    }
    /* find empty spot in shmem array */
    n = 1;
    while ((n <= RTAPI_MAX_SHMEMS) && (shmem_array[n].key != 0)) {
	rtapi_print_msg(RTAPI_MSG_DBG, OUR_API ": shmem %d occupied \n",n);
	n++;
    }
    if (n > RTAPI_MAX_SHMEMS) {
	/* no room */
	rtapi_mutex_give(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: reached shmem limit %d\n",
	    n);
	return -EMFILE;
    }
    /* we have space for the block data */
    rtapi_print_msg(RTAPI_MSG_DBG, OUR_API ": using new shmem %d key 0x%x\n",
		    n,key);
    shmem_id = n;
    shmem = &(shmem_array[n]);

    /* get shared memory block from OS and save its address */
    sm.key = key;
    sm.size = size;
    sm.flags = 0;
#ifdef ULAPI
    sm.driver_fd = shmdrv_driver_fd();
#endif
    retval = shmdrv_create(&sm);
    if (retval < 0) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,"shmdrv create failed key=0x%x size=%ld\n", key, size);
	return retval;
    }
    retval = shmdrv_attach(&sm, &shmem_addr_array[shmem_id]);
    if (retval < 0) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,"shmdrv attached failed key=0x%x size=%ld\n", key, size);
	return retval;
    }
    if (shmem_addr_array[shmem_id] == NULL) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: ERROR: could not create shmem %d key 0x%x\n",
			n, key);
	return -ENOMEM;
    }
    /* the block has been created, update data */
    rtapi_set_bit(module_id, shmem->bitmap);
    shmem->key = key;
#ifdef RTAPI
    shmem->rtusers = 1;
    shmem->ulusers = 0;
#else /* ULAPI */
    shmem->rtusers = 0;
    shmem->ulusers = 1;
#endif  /* ULAPI */
    shmem->size = size;
    shmem->magic = SHMEM_MAGIC;
    shmem->instance = instance;
    rtapi_data->shmem_count++;

    /* zero the first word of the shmem area */
    *((long int *) (shmem_addr_array[shmem_id])) = 0;
    /* announce the birth of a brand new baby shmem */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"RTAPI: shmem %02d created by module %02d key: 0x%x size: %lu\n",
	shmem_id, module_id, key, size);

    /* and return the ID to the proud parent */
    rtapi_mutex_give(&(rtapi_data->mutex));
    return shmem_id;
}

#ifdef ULAPI
static void check_memlock_limit(const char *where) {
    static int checked=0;
    struct rlimit lim;
    int result;
    if(checked) return;
    checked=1;

    result = getrlimit(RLIMIT_MEMLOCK, &lim);
    if(result < 0) { perror("getrlimit"); return; }
    if(lim.rlim_cur == (rlim_t)-1) return; // unlimited
    if(lim.rlim_cur >= RLIMIT_MEMLOCK_RECOMMENDED) return; // limit is at least recommended
    rtapi_print_msg(RTAPI_MSG_ERR,
        "RTAPI: Locked memory limit is %luKiB, recommended at least %luKiB.\n"
        "This can cause the error '%s'.\n"
        "For more information, see\n"
        "\thttp://wiki.linuxcnc.org/cgi-bin/emcinfo.pl?LockedMemory\n",
        (unsigned long)lim.rlim_cur/1024, RLIMIT_MEMLOCK_RECOMMENDED/1024, where);
    return;
}
#endif /* ULAPI */


int _rtapi_shmem_delete_inst(int shmem_id, int instance, int module_id) {
    shmem_data *shmem;
    int manage_lock, retval;
#ifdef RTAPI
    struct shm_status sm;
#endif

    /* validate shmem ID */
    if ((shmem_id < 1) || (shmem_id > RTAPI_MAX_SHMEMS)) {
	return -EINVAL;
    }
    /* point to the shmem's data */
    shmem = &(shmem_array[shmem_id]);
    /* is the block valid? */
    if (shmem->key == 0) {
	return -EINVAL;
    }
    /* validate module_id */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	return -EINVAL;
    }
    if (module_array[module_id].state != MODULE_STATE) {
	return -EINVAL;
    }
    /* is this module using the block? */
    if (rtapi_test_bit(module_id, shmem->bitmap) == 0) {
	return -EINVAL;
    }
    /* check if we need to manage the mutex */
    manage_lock = (shmem->magic != SHMEM_MAGIC_DEL_LOCKED);
    /* if no magic delete lock held is set, get the mutex */
    if (manage_lock) rtapi_mutex_get(&(rtapi_data->mutex));
    /* OK, we're no longer using it */
    rtapi_clear_bit(module_id, shmem->bitmap);
#ifdef ULAPI
    shmem->ulusers--;

    if ((shmem->ulusers == 0) && (shmem->rtusers == 0)) {
	// shmdrv can detach unused shared memory from userland too
	// this will munmap() the segment causing a drop in uattach refcount
	// and eventual free by garbage collect in shmdrv
	retval = shm_common_detach(shmem->size, shmem_addr_array[shmem_id]);
	if (retval) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "ULAPI:%d ERROR: shm_common_detach(%02d) failed: %s\n",
			    rtapi_instance, shmem_id, strerror(-retval));
	}
    }
    /* unmap the block */
    shmem_addr_array[shmem_id] = NULL;
#else /* RTAPI */
    shmem->rtusers--;
#endif  /* RTAPI */
    /* is somebody else still using the block? */
    if ((shmem->ulusers > 0) || (shmem->rtusers > 0)) {
	/* yes, we're done for now */
	rtapi_print_msg(RTAPI_MSG_DBG,
			"RTAPI: shmem %02d key 0x%x closed by module %02d\n",
			shmem_id, shmem->key, module_id);
	if (manage_lock) rtapi_mutex_give(&(rtapi_data->mutex));
	return 0;
    }

#ifdef RTAPI
    /* no other realtime users, free the shared memory from kernel space */
    shmem_addr_array[shmem_id] = NULL;
    shmem->rtusers = 0;
    /* are any user processes using the block? */
    if (shmem->ulusers > 0) {
	/* yes, we're done for now */
	rtapi_print_msg(RTAPI_MSG_DBG,
			"RTAPI: shmem %02d key 0x%x unmapped by module %02d\n",
			shmem_id,shmem->key, module_id);
	if (manage_lock) rtapi_mutex_give(&(rtapi_data->mutex));
	return 0;
    }

    /* no other users at all, this ID is now free */
    sm.key = shmem->key;
    sm.size = shmem->size;
    sm.flags = 0;
    if ((retval = shmdrv_detach(&sm)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d ERROR: shmdrv_detach(%x,%d) fail: %d\n",
			rtapi_instance, sm.key, sm.size, retval);
    }
#endif  /* RTAPI */


    /* update the data array and usage count */
    shmem->size = 0;
    rtapi_data->shmem_count--;
    /* release the lock if needed, print a debug message and return */
    if (manage_lock) rtapi_mutex_give(&(rtapi_data->mutex));
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: shmem %02d key 0x%x freed by module %02d\n",
		    shmem_id, shmem->key, module_id);
    shmem->key = 0;
    return 0;
}

int _rtapi_shmem_getptr_inst(int shmem_id, int instance, void **ptr,
			     unsigned long int *size) {
    /* validate shmem ID */
    if ((shmem_id < 1) || (shmem_id > RTAPI_MAX_SHMEMS)) {
	return -EINVAL;
    }
    /* is the block mapped? */
    if (shmem_addr_array[shmem_id] == NULL) {
	return -ENOENT;
    }
    /* pass memory address back to caller */
    *ptr = shmem_addr_array[shmem_id];
    return 0;
}

int _rtapi_shmem_exists(int userkey) {
    struct shm_status sm;
    sm.key = userkey;

    return !shmdrv_status(&sm); 
}

#endif  /* BUILD_SYS_KBUILD */


// implement rtapi_shmem_* calls  in terms of _rtapi_shmem_*_inst()

int _rtapi_shmem_new(int userkey, int module_id, unsigned long int size) {
    return _rtapi_shmem_new_inst(userkey, rtapi_instance, module_id, size);
}

int _rtapi_shmem_getptr(int handle, void **ptr, unsigned long int *size) {
    return _rtapi_shmem_getptr_inst(handle, rtapi_instance, ptr, size);
}

int _rtapi_shmem_delete(int handle, int module_id) {
    return _rtapi_shmem_delete_inst(handle, rtapi_instance, module_id);
}
