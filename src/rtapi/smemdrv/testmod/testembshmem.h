/*
 * This is the structure that is overlaid on the shared memory block.
 *
 * Copyright (c) Embrisk Ltd 2012.
 * This is public domain software with no warranty.
 */
#ifndef TEST_EMBSHMEM_H
#define TEST_EMBSHMEM_H

struct ShmemStruct {
    int countA;
    int countB;
    int countC;
};

#endif // TEST_EMBSHMEM_H
