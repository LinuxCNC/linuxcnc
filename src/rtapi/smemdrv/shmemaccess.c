/*
 * This library proivides access routines to allocate and release the shared memory block.
 *
 * Copyright (c) Embrisk Ltd 2012.
 * This is public domain software with no warranty.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#include "rtapishm.h"
static const char *devName = "/dev/shm0";
static int shmemLength;
static void *shmem;


#if 0

struct rtapishm_ioctlmsg {
    int  id;
    size_t size;
    void *addr;
};

#define RTAPISHM_IOC_MAGIC    'r'

#define IOC_RTAPISHM_EXISTS   _IOR(RTAPISHM_IOC_MAGIC, 1, int)
#define IOC_RTAPISHM_CREATE   _IOR(RTAPISHM_IOC_MAGIC, 2, struct rtapishm_ioctlmsg)
#define IOC_RTAPISHM_ATTACH   _IOW(RTAPISHM_IOC_MAGIC, 3, struct rtapishm_ioctlmsg)
#define IOC_RTAPISHM_DELETE   _IOR(RTAPISHM_IOC_MAGIC, 4, struct rtapishm_ioctlmsg)
#endif

/*
 * Get a pointer to the shared memory block of specified length.
 */
void *ShmemGet(int length)
{
    FILE *fp;
    int fd;
    int arg;
    struct rtapishm_ioctlmsg sm; 

    fp = fopen(devName, "r+");
    if (!fp) {
        fprintf(stderr, "Unable to open file '%s'\n", devName);
        exit(1);
    }
    fd = fileno(fp);

    arg = 47;
    if (ioctl(fd, IOC_RTAPISHM_EXISTS, &arg)) {
	perror("IOC_RTAPISHM_EXISTS");
    }
    sm.id = 123;
    sm.size = 252000;
    sm.addr = NULL;

    if (ioctl(fd, IOC_RTAPISHM_CREATE, &sm)) {
	perror("IOC_RTAPISHM_CREATE");
    }

    if (ioctl(fd, IOC_RTAPISHM_ATTACH, &sm)) {
	perror("IOC_RTAPISHM_ATTACH");
    }
    printf("attach id=%d size=%d\n", sm.id, sm.size);

    shmem = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd, 0);
    if (shmem == (void *)MAP_FAILED) {
        fprintf(stderr, "Couldn't allocate shared memory.  Error %d\n", errno);
        perror(NULL);
        shmem = NULL;
        return(NULL);
    }
    printf("Got shared memory - address %p\n", shmem);

    shmemLength = length;
    fclose(fp);
    return(shmem);
}

/*
 * Release the previously allocated shared memory block.
 */
void ShmemRelease(void)
{
    if (shmem) {
        munmap(shmem, shmemLength);
    }
}
