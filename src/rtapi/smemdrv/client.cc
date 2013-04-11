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

#include "embshmem.h"
#include "testmod/testembshmem.h"

int main(int argc, char **argv)
{
    struct ShmemStruct *shmem;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s [a|b]\n", argv[0]);
        exit(1);
    }

    shmem = (struct ShmemStruct *)ShmemGet(sizeof(struct ShmemStruct));
    if (!shmem) {
        fprintf(stderr, "Couldn't allocate shared memory.  Error %d\n", errno);
        perror(NULL);
        exit(1);
    }
    printf("Got shared memory - address %p\n", shmem);

    if (argv[1][0] == 'a') {
        shmem->countA = 1;
        while (shmem->countA < 10000) {
            static int lastC = 0;
            while(shmem->countC == lastC) ;
            lastC = shmem->countC;
            shmem->countA = lastC + 1;
            if ((lastC % 1) == 0) {
                printf("%d\n", lastC);
            }
        }
    } else {
        while (shmem->countB < 10000) {
            static int lastA = 0;
            while(shmem->countA == lastA) ;
            lastA = shmem->countA;
            shmem->countB = lastA + 1;
            if ((lastA % 1) == 0) {
                printf("%d\n", lastA);
            }
        }

    }

    ShmemRelease();
}
