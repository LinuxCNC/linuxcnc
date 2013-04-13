

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

#include "testshmdrv.h"
#include "shmdrv.h"

int key = 4711;
int size;

int main(int argc, char **argv)
{
    struct shm_status sm; 
    void *shmem;

    shmdrv_debug = 1;
    if (argc > 1) {
	key = atoi(argv[1]);
	if (argc > 2)
	    size = atoi(argv[2]);
    }


    sm.key = key;
    sm.driver_fd = shmdrv_driver_fd();
    if (shmdrv_status(&sm) == 0) {
	shmdrv_print_status(&sm,"status");
	if (!shmdrv_attach(&sm, &shmem)) {
	    printf("Got existing shared memory - address %p\n", shmem);
	    sleep(3600);
	}
    } else {
	if (!size)
	    exit(0);
	sm.size = size;
	if (shmdrv_create(&sm)) {
	    perror("shmdrv_create");
	} else {
	    shmdrv_print_status(&sm, "post create\n");
	    if (!shmdrv_attach(&sm, &shmem)) {
		printf("attached newly created shared memory - address %p\n", shmem);
		sleep(3600);
	    }
	}
    }
    sleep(10);
    printf("detaching shared memory\n");
    if (shmdrv_detach(&sm, shmem)) {
	printf("detach failed\n");
    }
    exit(0);
}
