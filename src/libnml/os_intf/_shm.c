/********************************************************************
* Description: _shm.c
*   C implementation of rcslib shared memory API
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/
extern int instance_no; // exported from cms.cc

#include "_shm.h"
#include "rcs_print.hh"
#include <stdio.h>		/* NULL */
#include <linux/posix_types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>

#if defined(qnx) && !defined(USE_POSIX_SHAREDMEM)
#define USE_POSIX_SHAREDMEM 1
#endif

#ifdef USE_POSIX_SHAREDMEM
#include <fcntl.h>
#include <sys/mman.h>
#else
#include <sys/shm.h>
#endif

#include <string.h>

static int shmems_created_list[100];
static int shmems_created_list_initialized = 0;

shm_t *rcs_shm_open(key_t key, size_t size, int oflag, /* int mode */ ...)
{
    va_list ap;
    int mode;
    int shmflg = 0;
    shm_t *shm;
#ifdef USE_POSIX_SHAREDMEM
    int existed_before = 0;
#if HAVE_FSTAT
    struct stat statbuf;
#endif
#else
    struct shmid_ds shared_mem_info;

    int pid;
    int i;
#endif

    va_start(ap, oflag);
    if (oflag) {
	mode = va_arg(ap, int);
	shmflg |= mode;
    }
    va_end(ap);

#ifdef USE_POSIX_SHAREDMEM
    rcs_print_debug(PRINT_SHARED_MEMORY_ACTIVITY,
	"rcs_shm_open(key=%d(0x%X),size=%d(0x%X),oflag=%d)\n",
	key, key, size, size, oflag);

    if (key == 0) {
	rcs_print_error("rcs_shm_open(%d(0x%X), %d(0x%X), %d(0x%X)): error\n",
	    key, key, size, size, oflag, oflag);
	rcs_print_error("RCS Shared Memory key may not be zero.\n");
	return NULL;
    }

    shm = (shm_t *) calloc(sizeof(shm_t), 1);
    if (NULL == shm) {
	rcs_print_error("rcs_shm_open: calloc failed\n");
	return NULL;
    }
    shm->create_errno = 0;
    shm->addr = NULL;
    shm->key = key;
    shm->size = size;
#ifdef POSIX_SHMEM_NAME_PREFIX
    strncpy(shm->name, POSIX_SHMEM_NAME_PREFIX, 64);
    sprintf(shm->name + strlen(shm->name), "/rcs_shm%d-%d", key,instance_no);
#else
    sprintf(shm->name, "/rcs_shm%d-%d", key,instance_no);
#endif

    shm->id = 0;
    errno = 0;

    if (oflag) {
	oflag = O_CREAT;
	shm->id = shm_open(shm->name, O_RDWR, 0777);
    }

    /* Create a new memory object */
    if (shm->id <= 0) {
	shm->id = shm_open(shm->name, oflag | O_RDWR, 0777);
	if (shm->id == -1) {
	    rcs_print_error("shm_open(%s,%d(0x%X),%d(0x%X)) failed:%s %d\n",
		shm->name, oflag | O_RDWR, oflag | O_RDWR,
		mode, mode, strerror(errno), errno);
	    shm->create_errno = errno;
	    return shm;
	}
	existed_before = (oflag == 0);
    } else {
	existed_before = 1;
    }

    shm->created = (oflag != 0);

    /* Set the memory object's size */
    if (!existed_before) {
	if (ftruncate(shm->id, size + 16) == -1) {
	    rcs_print_error("ftruncate(%d,%d): %s %d\n",
		shm->id, (size + 16), strerror(errno), errno);
	    shm->create_errno = errno;
	    return shm;
	}
    }
#if HAVE_FSTAT
    else {
	if (-1 == fstat(shm->id, &statbuf)) {
	    rcs_print_error("fstat failed. (errno=%d) %s\n",
		errno, strerror(errno));
	    shm->create_errno = errno;
	    return shm;
	}
	if (statbuf.st_size != size + 16) {
	    rcs_print_error
		("Shared memory buffer %s already exists but has the wrong size of % d instead of the expected size of % d. \n ",
		shm->name, statbuf.st_size, size + 16);
	    shm->create_errno = -1;
	    return shm;
	}
    }
#endif

    /* Map the memory object */
    shm->addr = mmap(0, size + 16,
	PROT_READ | PROT_WRITE, MAP_SHARED, shm->id, 0);
    if (shm->addr == MAP_FAILED) {
	rcs_print_error
	    ("mmap(0,%d,PROT_READ | PROT_WRITE, MAP_SHARED,%d,0) failed: %s %d\n",
	    shm->id, size, strerror(errno), errno);
	shm->create_errno = errno;
    }
    shm->size = size;
    if (oflag & O_CREAT && !existed_before) {
	*((int *) ((char *) shm->addr + size)) = 0;
    } else {
	(*((int *) ((char *) shm->addr + size)))++;
    }

    return (shm);
#else

    rcs_print_debug
	(PRINT_SHARED_MEMORY_ACTIVITY,
	"rcs_shm_open(key=%d(0x%X),size=%zu(0x%zX),oflag=%d)\n",
	key, key, size, size, oflag);

    if (key == 0) {
	rcs_print_error("rcs_shm_open(%d(0x%X), %zu(0x%zX), %d(0x%X)): error\n",
	    key, key, size, size, oflag, oflag);
	rcs_print_error("RCS Shared Memory key may not be zero.\n");
	return NULL;
    }
#if defined(O_CREAT)
#if O_CREAT != IPC_CREAT
    if ((oflag & O_CREAT) && !(oflag & IPC_CREAT)) {
	oflag &= ~(O_CREAT);
	oflag |= IPC_CREAT;
    }
#endif
#endif

    if (oflag) {
	shmflg |= IPC_CREAT;
    }

    shm = (shm_t *) calloc(sizeof(shm_t), 1);
    if (NULL == shm) {
	rcs_print_error("rcs_shm_open: calloc failed\n");
	return NULL;
    }
    shm->create_errno = 0;
    shm->addr = NULL;

    key += instance_no * 1000;
    shm->key = key;
    errno = 0;

    shm->size = size;
    //fprintf(stderr, "rcs_shm_open _shm key=%d instance_no=%d\n", key, instance_no);
    if ((shm->id = shmget(key , (int) size, shmflg)) == -1) {
	shm->create_errno = errno;
	rcs_print_error("shmget(%d(0x%X),%zd,%d) failed: (errno = %d): %s\n",
	    key, key, size, shmflg, errno, strerror(errno));
	switch (errno) {
	case EEXIST:
	    rcs_print_error
		("A shared memory buffer for this key already exists.\n");
	    break;

	case EINVAL:
	    rcs_print_error
		("Either the size is too big or the shared memory buffer already exists but is of the wrong size.\n");
	    break;

	case ENOSPC:
	    rcs_print_error
		("The system imposed limit on the maximum number of shared memory segments has been exceeded.\n");
	    break;

	case ENOENT:
	    rcs_print_error
		("No shared memory buffer exists for this key and the IPC_CREAT was not given.\n");
	    break;
	}
	return (shm);
    }

    /* map shmem area into local address space */
    shmflg = 0;
    shmflg &= ~SHM_RDONLY;
    if ((shm->addr = (void *) shmat(shm->id, 0, shmflg)) == (void *) -1) {
	shm->create_errno = errno;
	rcs_print_error("shmat(%d,0,%d) failed:(errno = %d): %s\n", shm->id,
	    shmflg, errno, strerror(errno));
	rcs_print_error("key = %d (0x%X)\n", key, key);
	shm->addr = NULL;
	return (shm);
    }

    /* Check to see if I am the creator of this shared memory buffer. */
    if (shmctl(shm->id, IPC_STAT, &shared_mem_info) < 0) {
	rcs_print_error("shmctl error: %d %s\n", errno, strerror(errno));
	return shm;
    }

    /* If oflag was not set this process couldn't be the creator. */
    if (!oflag) {
	return shm;
    }

    if (!shmems_created_list_initialized) {
	memset(shmems_created_list, 0, 100 * sizeof(int));
	shmems_created_list_initialized = 1;
    } else {
	for (i = 0; i < 100; i++) {
	    if (shmems_created_list[i] == key) {
		return shm;
	    }
	}
    }

    pid = (int) getpid();
    if (pid <= 0) {
	rcs_print_error("getpid error: %d %s\n", errno, strerror(errno));
	return shm;
    }
    shm->created = (shared_mem_info.shm_cpid == pid);
#ifdef linux_2_4_0
    shm->created = 1;
#endif
    if (shm->created) {
	for (i = 0; i < 100; i++) {
	    if (shmems_created_list[i] <= 0) {
		shmems_created_list[i] = shm->key;
		break;
	    }
	}
    }
    return shm;
#endif
}

int rcs_shm_close(shm_t * shm)
{
#ifdef USE_POSIX_SHAREDMEM
    int nattch;
    if (shm == 0) {
	return -1;
    }
    if (shm->addr > 0) {
	nattch = rcs_shm_nattch(shm);
	(*((int *) ((char *) shm->addr + shm->size)))--;
	if (munmap(shm->addr, shm->size + 16) == -1) {
	    rcs_print_error("munmap(%p,%d) failed. %s %d\n",
		shm->addr, shm->size, strerror(errno), errno);
	    return -1;
	}
	shm->addr = NULL;
	if (shm->id > 0) {
	    if (close(shm->id) == -1) {
		rcs_print_error("close(%d) failed. %s %d\n",
		    shm->id, strerror(errno), errno);
	    }
	}
	if (nattch <= 1) {
	    shm_unlink(shm->name);
	}
	shm->id = 0;
    }
#else
    struct shmid_ds shared_mem_info;

    int i;

    /* check for invalid ptr */
    if (shm == NULL)
	return -1;

    rcs_print_debug(PRINT_SHARED_MEMORY_ACTIVITY,
	"rcs_shm_close(shm->key=%d(0x%X),shm->size=%zu(0x%zX),shm->addr=%p)\n",
	shm->key, shm->key, shm->size, shm->size, shm->addr);

    /* detach from shmem */
    shmdt((char *) shm->addr);

    /* remove OS shmem if there are no attached processes */
    if (rcs_shm_nattch(shm) == 0) {
	shmctl(shm->id, IPC_RMID, &shared_mem_info);
    }

    if (shm->created && shmems_created_list_initialized) {
	for (i = 0; i < 100; i++) {
	    if (shmems_created_list[i] == shm->key) {
		shmems_created_list[i] = 0;
		break;
	    }
	}
    }
#endif

    free(shm);

    return 0;
}

int rcs_shm_delete(shm_t * shm)
{
#ifdef USE_POSIX_SHAREDMEM
    if (shm == 0) {
	return -1;
    }
    if (shm->addr > 0) {
	(*((int *) ((char *) shm->addr + shm->size)))--;
	if (munmap(shm->addr, shm->size + 16) == -1) {
	    rcs_print_error("munmap(%p,%d) failed. %s %d\n",
		shm->addr, shm->size, strerror(errno), errno);
	    return -1;
	}
	shm->addr = NULL;
	if (shm->id > 0) {
	    if (close(shm->id) == -1) {
		rcs_print_error("close(%d) failed. %s %d\n",
		    shm->id, strerror(errno), errno);
	    }
	}
	shm->id = 0;
    }
    shm_unlink(shm->name);
#else
    struct shmid_ds shared_mem_info;

    /* check for invalid ptr */
    if (shm == NULL)
	return -1;

    /* detach from shmem */
    shmdt((char *) shm->addr);

    /* remove OS shmem regardless of whether there are attached processes */
    shmctl(shm->id, IPC_RMID, &shared_mem_info);
#endif

    free(shm);

    return 0;
}

int rcs_shm_nattch(shm_t * shm)
{
#ifdef USE_POSIX_SHAREDMEM
    if (shm == 0) {
	return -1;
    }
    if (shm->addr == 0) {
	return -1;
    }
    return *((int *) (((char *) shm->addr) + shm->size)) + 1;
#else
    struct shmid_ds shared_mem_info;
    int err;
    /* check for invalid ptr */
    if (shm == NULL)
	return -1;

    /* get the status of shared memory */
    err = shmctl(shm->id, IPC_STAT, &shared_mem_info);

    if(err == -1) {
        rcs_print_error("rcs_shm_nattch: shmctl failed: %s\n",
                strerror(errno));
        return 0;
    }

    return shared_mem_info.shm_nattch;
#endif

}
