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

#ifdef BUILD_SYS_USER_DSO
#include <sys/ipc.h>		/* IPC_* */
#include <sys/shm.h>		/* shmget() */
#include <stdlib.h>		/* rand_r() */
#include <unistd.h>		/* getuid(), getgid(), sysconf(),
				   ssize_t, _SC_PAGESIZE */
#else  /* BUILD_SYS_KBUILD */
#  ifdef ULAPI
#    include <sys/time.h>
#    include <sys/resource.h>
#  endif
#endif


#define SHMEM_MAGIC   25453	/* random numbers used as signatures */
#define SHM_PERMISSIONS	0666

#ifdef BUILD_SYS_KBUILD
#  ifdef RTAPI
#    define MODULE_STATE REALTIME
#    define OUR_API "RTAPI"
#  else
#    define MODULE_STATE USERSPACE
#    define OUR_API "ULAPI"

#    define RECOMMENDED (20480*1024lu)
#  endif
#endif


/* prototypes for hooks the kernel thread systems must implement */
extern void *rtapi_shmem_new_realloc_hook(int shmem_id, int key,
					  unsigned long int size);
extern void *rtapi_shmem_new_malloc_hook(int shmem_id, int key,
					 unsigned long int size);
extern void rtapi_shmem_delete_hook(shmem_data *shmem,int shmem_id);
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

int _rtapi_shmem_new(int key, int module_id, unsigned long int size) {
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


int _rtapi_shmem_getptr(int handle, void **ptr) {
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


int _rtapi_shmem_delete(int handle, int module_id) {
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


#else  /* BUILD_SYS_KBUILD */
/***********************************************************************
*                            KERNEL THREADS                            *
************************************************************************/

int _rtapi_shmem_new(int key, int module_id, unsigned long int size) {
    int n;
    int shmem_id;
    shmem_data *shmem;

    /* key must be non-zero, and also cannot match the key that RTAPI uses */
    if ((key == 0) || (key == RTAPI_KEY)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: bad shmem key: %d\n",
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
				"RTAPI: ERROR: shmem size mismatch\n");
		return -EINVAL;
	    }
	    /* is this module already using it? */
	    if (test_bit(module_id, shmem->bitmap)) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		rtapi_print_msg(RTAPI_MSG_WARN,
				"RTAPI: Warning: shmem already mapped\n");
		return -EINVAL;
	    }
	    /* yes, has it been mapped into kernel space? */
#ifdef RTAPI
	    if (shmem->rtusers == 0) {
#endif
		/* no, map it and save the address */
		shmem_addr_array[shmem_id] = 
		    rtapi_shmem_new_realloc_hook(shmem_id, key, size);
		if (shmem_addr_array[shmem_id] == NULL) {
		    rtapi_print_msg(RTAPI_MSG_ERR,
				    "RTAPI: ERROR: failed to map shmem\n");
		    rtapi_mutex_give(&(rtapi_data->mutex));
#ifdef ULAPI
		    check_memlock_limit("failed to map shmem");
#endif
		    return -ENOMEM;
		}
#ifdef RTAPI
	    }
#endif
	    /* update usage data */
	    set_bit(module_id, shmem->bitmap);
#ifdef ULAPI
	    shmem->ulusers++;
#else  /* RTAPI */
	    shmem->rtusers++;
#endif  /* RTAPI */
	    /* announce another user for this shmem */
	    rtapi_print_msg(RTAPI_MSG_DBG,
		"RTAPI: shmem %02d opened by module %02d\n",
		shmem_id, module_id);
	    /* done */
	    rtapi_mutex_give(&(rtapi_data->mutex));
	    return shmem_id;
	}
    }
    /* find empty spot in shmem array */
    n = 1;
    while ((n <= RTAPI_MAX_SHMEMS) && (shmem_array[n].key != 0)) {
	rtapi_print_msg(RTAPI_MSG_ERR, OUR_API ": shmem %d occupuied \n",n);
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
    rtapi_print_msg(RTAPI_MSG_ERR, OUR_API ": using new shmem %d  \n",n);
    shmem_id = n;
    shmem = &(shmem_array[n]);

    /* get shared memory block from OS and save its address */

    shmem_addr_array[shmem_id] =
	rtapi_shmem_new_malloc_hook(shmem_id, key, size);

    if (shmem_addr_array[shmem_id] == NULL) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "RTAPI: ERROR: could not create shmem %d\n", n);
	return -ENOMEM;
    }
    /* the block has been created, update data */
    set_bit(module_id, shmem->bitmap);
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
    rtapi_data->shmem_count++;
    /* zero the first word of the shmem area */
    *((long int *) (shmem_addr_array[shmem_id])) = 0;
    /* announce the birth of a brand new baby shmem */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"RTAPI: shmem %02d created by module %02d, key: %d, size: %lu\n",
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
    if(lim.rlim_cur >= RECOMMENDED) return; // limit is at least recommended
    rtapi_print_msg(RTAPI_MSG_ERR,
        "RTAPI: Locked memory limit is %luKiB, recommended at least %luKiB.\n"
        "This can cause the error '%s'.\n"
        "For more information, see\n"
        "\thttp://wiki.linuxcnc.org/cgi-bin/emcinfo.pl?LockedMemory\n",
        (unsigned long)lim.rlim_cur/1024, RECOMMENDED/1024, where);
    return;
}
#endif /* ULAPI */


int _rtapi_shmem_delete(int shmem_id, int module_id) {
    shmem_data *shmem;
    int manage_lock;

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
    if (test_bit(module_id, shmem->bitmap) == 0) {
	return -EINVAL;
    }
    /* check if we need to manage the mutex */
    manage_lock = (shmem->magic != SHMEM_MAGIC_DEL_LOCKED);
    /* if no magic delete lock held is set, get the mutex */
    if (manage_lock) rtapi_mutex_get(&(rtapi_data->mutex));
    /* OK, we're no longer using it */
    clear_bit(module_id, shmem->bitmap);
#ifdef ULAPI
    shmem->ulusers--;
    /* unmap the block */
    shmem_addr_array[shmem_id] = NULL;
#else /* RTAPI */
    shmem->rtusers--;
#endif  /* RTAPI */
    /* is somebody else still using the block? */
    if ((shmem->ulusers > 0) || (shmem->rtusers > 0)) {
	/* yes, we're done for now */
	rtapi_print_msg(RTAPI_MSG_DBG,
	    "RTAPI: shmem %02d closed by module %02d\n", shmem_id, module_id);
	if (manage_lock) rtapi_mutex_give(&(rtapi_data->mutex));
	return 0;
    }
    /* no other realtime users, free the shared memory from kernel space */

    rtapi_shmem_delete_hook(shmem,shmem_id);

#ifdef RTAPI
    shmem_addr_array[shmem_id] = NULL;
    shmem->rtusers = 0;
    /* are any user processes using the block? */
    if (shmem->ulusers > 0) {
	/* yes, we're done for now */
	rtapi_print_msg(RTAPI_MSG_DBG,
	    "RTAPI: shmem %02d unmapped by module %02d\n", shmem_id,
	    module_id);
	if (manage_lock) rtapi_mutex_give(&(rtapi_data->mutex));
	return 0;
    }
    /* no other users at all, this ID is now free */
#endif  /* RTAPI */
    /* update the data array and usage count */
    shmem->key = 0;
    shmem->size = 0;
    rtapi_data->shmem_count--;
    /* release the lock if needed, print a debug message and return */
    if (manage_lock) rtapi_mutex_give(&(rtapi_data->mutex));
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: shmem %02d freed by module %02d\n",
	shmem_id, module_id);
    return 0;
}

int rtapi_shmem_getptr(int shmem_id, void **ptr) {
    /* validate shmem ID */
    if ((shmem_id < 1) || (shmem_id > RTAPI_MAX_SHMEMS)) {
	return -EINVAL;
    }
    /* is the block mapped? */
    if (shmem_addr_array[shmem_id] == NULL) {
	return -EINVAL;
    }
    /* pass memory address back to caller */
    *ptr = shmem_addr_array[shmem_id];
    return 0;
}

#ifdef EXPORT_SYMS
EXPORT_SYMBOL(_rtapi_shmem_new);
EXPORT_SYMBOL(_rtapi_shmem_delete);
EXPORT_SYMBOL(_rtapi_shmem_getptr);
#endif  /* RTAPI */
#endif  /* BUILD_SYS_KBUILD */
