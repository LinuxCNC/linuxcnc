/*
 * This program is used to test the shared memory driver.  There are two instances of this
 * ('a' and 'b') that pass information between themselves and a kernel module using the shared
 * memory.
 *
 * Copyright (c) Embrisk Ltd 2012.
 * This is public domain software with no warranty.
 */
 
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "shmdrv.h"
#include "testmod/testembshmem.h"
static const char *devName = "/dev/" DEVICE_NAME;

static int fd;
int key = 4711;
int size;

int main(int argc, char **argv)
{
    FILE *fp;
    struct shm_ioctlmsg sm; 
    void *shmem;

    if (argc > 1) {
	key = atoi(argv[1]);
	if (argc > 2)
	    size = atoi(argv[2]);
    }

    fp = fopen(devName, "r+");
    if (!fp) {
        fprintf(stderr, "Unable to open file '%s'\n", devName);
        exit(1);
    }
    fd = fileno(fp);

    sm.key = key;
    if (ioctl(fd, IOC_SHM_STATUS, &sm)) {
	perror("IOC_SHM_STATUS");
    } else {
	printf("status key = %d \n", key);
	printf("id = %d \n", sm.id);
	printf("size = %d \n", sm.size);
	printf("actsize = %d \n", sm.act_size);
	printf("n_uattach = %d \n", sm.n_uattach);
	printf("n_kattach = %d \n", sm.n_kattach);
	printf("creator = %d \n", sm.creator);
    }

    if (size) {
	sm.key = key;
	sm.size = size;
	if (ioctl(fd, IOC_SHM_CREATE, &sm)) {
	    perror("IOC_SHM_CREATE");
	} else {
	    printf("create key = %d \n", key);
	    printf("id = %d \n", sm.id);
	    printf("size = %d \n", sm.size);
	    printf("actsize = %d \n", sm.act_size);
	    printf("n_uattach = %d \n", sm.n_uattach);
	    printf("n_kattach = %d \n", sm.n_kattach);
	    printf("creator = %d \n", sm.creator);

	    shmem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, 
			 fd, 0);
	    if (shmem == (void *)MAP_FAILED) {
		fprintf(stderr, "Couldn't allocate shared memory.  Error %d\n", errno);
		perror(NULL);
		shmem = NULL;
		exit(1);
	    }
	    printf("Got shared memory - address %p\n", shmem);
	    sleep(3600);
	}
    } else {
	sm.key = key;
	if (ioctl(fd, IOC_SHM_ATTACH, &sm)) {
	    perror("IOC_SHM_ATTACH");
	} else {
	    printf("attach key = %d \n", key);
	    printf("id = %d \n", sm.id);
	    printf("size = %d \n", sm.size);
	    printf("n_uattach = %d \n", sm.n_uattach);
	    printf("n_kattach = %d \n", sm.n_kattach);
	    printf("creator = %d \n", sm.creator);
	}

	shmem = mmap(NULL, sm.size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd, 0);
	if (shmem == (void *)MAP_FAILED) {
	    fprintf(stderr, "Couldn't mmap after attach error %d\n", errno);
	    perror(NULL);
	    shmem = NULL;
	    exit(1);
	}
	printf("attached shared memory - address %p\n", shmem);
	sleep(5);
	printf("detaching shared memory\n");
	if (munmap(shmem,sm.size))
	    perror("munmap");

    }

    fclose(fp);
    exit(0);
}
