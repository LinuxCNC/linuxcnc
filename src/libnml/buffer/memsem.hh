/**********************************************************************
* File: memsem.hh
* Purpose: Provides function prototypes that let programmers
* use a block of memory to implement a mutual exclusion semaphore.
*
* With LynxOs and SunOs using semop is very inefficient if the semaphore will
* ussually be available. Other platforms may give you no semaphore operations.
*************************************************************************/

#ifndef MEMSEM_HH
#define MEMSEM_HH

#include "sem.hh"

/* Take the mutual exclusion semaphore. */
struct mem_access_object {
    void *data;
    long connection_number;
    long total_connections;
    double timeout;
    double sem_delay;
    int read_only;
    int split_buffer;
    char toggle_bit;
    RCS_SEMAPHORE *sem;
};

extern int mem_get_access(struct mem_access_object *mo);

/* Give up the mutual exclusion semaphore. */
extern int mem_release_access(struct mem_access_object *mo);

#endif
