/********************************************************************
 * Copyright (C) 2012, 2013 Michael Haberler <license AT mah DOT priv DOT at>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ********************************************************************/

// userland API for shmdrv and common Posix shm operations

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "config.h"		// build configuration
#include "rtapi.h"
#include "rtapi/shmdrv/shmdrv.h"
#include "shmdrv.h"

static const char *driver_name = "/dev/" DEVICE_NAME;

int shmdrv_loaded;
static long page_size;

int shm_common_init(void)
{
    page_size = sysconf(_SC_PAGESIZE);
    shmdrv_loaded = shmdrv_available();
    return 0;
}

int shmdrv_available(void)
{
    struct stat st;
    return !stat(driver_name, &st);
}

int shmdrv_driver_fd(void)
{
    struct stat st;
    int retval;
    int driver_fd = open(driver_name, O_RDWR);

    if (driver_fd < 0) {
    	retval = stat(driver_name, &st);
	if (retval) {
	    fprintf(stderr,"shmdrv_driver_fd: error opening %s : %s\n",
		    driver_name, strerror(errno));
	    return -errno;
	} else {
	    // exists, but cant open - likely permissions
	    // leave some time for udev to get its act together
	    int retries = 10;
	    while (access(driver_name,  R_OK|W_OK)) {
		struct timespec nap = {0, 300 * 1000 * 1000};
		nanosleep(&nap, NULL);
		if (!retries--) {
		    fprintf(stderr,
			    "shmdrv_driver_fd: cant access %s "
			    "- permission denied; shmdrv.rules not installed?\n",
			    driver_name);
		    return -EPERM;
		}
	    }
	}
    }
    return driver_fd;
}

static int fd_ok(struct shm_status *shmstat)
{
    struct stat st;
    int retval = fstat(shmstat->driver_fd, &st);
    if (retval) {
	return 0;
    }
    return S_ISCHR(st.st_mode);
}

int shmdrv_status(struct shm_status *shmstat)
{
    int retval = fd_ok(shmstat);
    if (!retval)
	return -ENOENT;
    return ioctl(shmstat->driver_fd, IOC_SHM_STATUS, shmstat);
}

int shmdrv_create(struct shm_status *shmstat)
{
    int retval = fd_ok(shmstat);
    if (!retval)
	return -ENOENT;
    return ioctl(shmstat->driver_fd, IOC_SHM_CREATE, shmstat);
}

int shmdrv_attach(struct shm_status *shmstat, void **shm)
{
    int retval = fd_ok(shmstat);
    if (!retval)
	return -ENOENT;
    retval = ioctl(shmstat->driver_fd, IOC_SHM_ATTACH, shmstat);
    if (retval)
	return retval;

    *shm = mmap(NULL, shmstat->size, 
		PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, 
		shmstat->driver_fd, 0);
    if (*shm == (void *)MAP_FAILED) {
	return -errno;
    }
    return 0;
}

int shmdrv_detach(struct shm_status *shmstat, void *shm)
{
    int retval = munmap(shm, shmstat->size);
    if (retval)
	perror("munmap");
    return retval;
}

int shmdrv_gc(void)
{
    int retval;
    int fd = shmdrv_driver_fd();
    if (fd < 0)
	return fd;
    retval = ioctl(fd, IOC_SHM_GC, NULL);
    close(fd);
    return retval;
}

void shmdrv_print_status(struct shm_status *sm, const char *tag)
{
    printf("%skey = %d (0x%x)\n", tag, sm->key,sm->key);
    printf("id = %d\n", sm->id);
    printf("size = %zu\n", sm->size);
    printf("act_size = %zu\n", sm->act_size);
    printf("n_uattach = %d\n", sm->n_uattach);
    printf("n_kattach = %d\n", sm->n_kattach);
    printf("creator = %d \n", sm->creator);
    printf("flags = %d/0x%x\n", sm->flags, sm->flags);
}

int shm_common_new(int key, int *size, int instance, void **shmptr, int create)
{
    struct shm_status sm;
    int retval;
    int is_new = 0;

    if (shmdrv_loaded) {
	// use shmdrv kernel driver
	sm.driver_fd = shmdrv_driver_fd();
	sm.key = key;
	sm.size = (size == NULL? 0: *size);
	sm.flags = 0;
	retval = shmdrv_status(&sm); // check if exists
	if (retval && !create) {
	    // didnt exist, but just attach requested, so fail
	    close(sm.driver_fd);
	    return -ENOENT;
	}
	if (retval) { // didnt exist, so create
	    retval = shmdrv_create(&sm); 
	    if (retval < 0) {
		return retval;
	    }
	    is_new = 1;
	}
	// now attach
	retval = shmdrv_attach(&sm, shmptr);
	if (retval < 0) {
	    close(sm.driver_fd);
	    return retval;
	}
	// if size was passed in as 0 (attach), fill in actual size
	if (size && (*size == 0)) 
	    *size = sm.size;
	close(sm.driver_fd);
	return is_new;

    } else {
	// use POSIX shared memory

	int shmfd, mmap_size;
	mode_t old_umask;
	char segment_name[LINELEN];
	if ((size == 0) || (*size == 0))
	    mmap_size = 0;
	else
	    mmap_size = *size;
	sprintf(segment_name, SHM_FMT, instance, key);
	old_umask = umask(0); //S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (create && ((shmfd = shm_open(segment_name, 
					 (O_CREAT | O_EXCL | O_RDWR),
 				(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP))) > 0)) {
	    // initial creation
	    if (fchown(shmfd, getuid(),getgid()))
		perror("fchown");
	    if (ftruncate(shmfd, mmap_size))
		perror("ftruncate");
	    is_new = 1;
	} else if((shmfd = shm_open(segment_name, O_RDWR,
				    (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP))) < 0) {
	    // just attach, and that failed:
	    umask(old_umask);
	    return -errno;
	} else {  // shmfd open
	    if (mmap_size == 0) {
		struct stat st;
		if (fstat(shmfd, &st)) {
		    perror("fstat");
		    return -errno;
		}
		mmap_size = st.st_size;
	    }
	}
	if((*shmptr = mmap(0, mmap_size, (PROT_READ | PROT_WRITE),
			   MAP_SHARED, shmfd, 0)) == MAP_FAILED) {
	    perror("shm_common_new:mmap");
	    close(shmfd);
	    umask(old_umask);
	    return -errno;
	}
	if (size)  // return actual shm size as determined in attach
	    *size = mmap_size;
	umask(old_umask);
	close(shmfd);
	return is_new;
    }
}

int shm_common_detach(int size, void *shmptr)
{
    if (munmap(shmptr, PAGESIZE_ALIGN(size)))
	return -errno;
    return 0;
}

int shm_common_exists(int key)
{
    struct shm_status sm;
    int retval;

    if (shmdrv_loaded) {
	memset(&sm, 0, sizeof(struct shm_status));
	sm.driver_fd = shmdrv_driver_fd();
	sm.key = key;
	sm.flags = 0;
	retval =  shmdrv_status(&sm);
	close(sm.driver_fd);
	return retval == 0;
    } else {
	int shmfd;
	char segment_name[LINELEN];

	sprintf(segment_name, SHM_FMT, INSTANCE_OF(key), key);
	if ((shmfd = shm_open(segment_name, O_RDWR,
			      (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP))) < 0) {
	    retval = 0;
	} else {
	    close(shmfd);
	    retval = 1;
	}
	return retval;
    }
}

// unlink entry in /dev/shm/linuxcnc-* when we know we're
// shutting down - reduce cleanup job for scripts/realtime
int shm_common_unlink(int key)
{
    if (shmdrv_loaded) {
	// a noop - the automatic garbage collection
	// will do this on last detach
	return 0;
    } else {
	char segment_name[LINELEN];

	sprintf(segment_name, SHM_FMT, INSTANCE_OF(key), key);
	return shm_unlink(segment_name);
    }
}
