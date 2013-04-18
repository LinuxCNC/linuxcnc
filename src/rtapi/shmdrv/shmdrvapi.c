
// userland API for shmdrv

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "config.h"		// build configuration
#include "rtapi.h"		// these functions
#include "rtapi_common.h"
#include "rtapi/shmdrv/shmdrv.h"
#include "shmdrv.h"

int shmdrv_debug;

int shmdrv_available(void)
{
    struct stat st;
    return !stat("/dev/" DEVICE_NAME, &st);
}

int shmdrv_driver_fd(void)
{
    int driver_fd = open("/dev/" DEVICE_NAME, O_RDWR);
    if (driver_fd < 0) {
	if (shmdrv_debug)
	    perror("cant open shared memory driver /dev/" DEVICE_NAME " - not loaded?");
	return -EPERM;
    }
    return driver_fd;
}

static int fd_ok(struct shm_status *shmstat)
{
    struct stat st;
    int retval = fstat(shmstat->driver_fd, &st);
    if (retval) {
	if (shmdrv_debug)
	    fprintf(stderr,"%s:%d: %s\n", __FUNCTION__, __LINE__,
		    strerror(-retval));
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
	if (shmdrv_debug)
	    fprintf(stderr,"%s:%d: %s\n", __FUNCTION__, __LINE__,
		    strerror(-retval));
	return errno;
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
    printf("size = %d\n", sm->size);
    printf("act_size = %d\n", sm->act_size);
    printf("n_uattach = %d\n", sm->n_uattach);
    printf("n_kattach = %d\n", sm->n_kattach);
    printf("creator = %d \n", sm->creator);
    printf("flags = %d/0x%x\n", sm->flags, sm->flags);
}

int shm_common_new(int key, int size, int instance, void **shmptr, int create)
{
    struct shm_status sm;
    int retval;
    int is_new = 0;

    if (shmdrv_loaded) {
	// use shmdrv kernel driver
	sm.driver_fd = shmdrv_driver_fd();
	sm.key = key;
	sm.size = size;
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
	close(sm.driver_fd);
	return is_new;

    } else {

	// use POSIX shared memory

	int shmfd;
	char segment_name[LINELEN];
	sprintf(segment_name, SHM_FMT, instance, key);

	if (create && ((shmfd = shm_open(segment_name, 
					 (O_CREAT | O_EXCL | O_RDWR),
					 (S_IREAD | S_IWRITE))) > 0)) {
	    // initial creation
	    ftruncate(shmfd, size);
	    is_new = 1;
	} else if((shmfd = shm_open(segment_name, O_RDWR,
				    (S_IREAD | S_IWRITE))) < 0) {
	    // just attach, and that failed:
	    return -errno;
	}
	if((*shmptr = mmap(0, size, (PROT_READ | PROT_WRITE),
			   MAP_SHARED, shmfd, 0)) == MAP_FAILED) {
	    close(shmfd);
	    return -errno;
	}
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
