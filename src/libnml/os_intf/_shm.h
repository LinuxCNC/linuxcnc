#ifndef _SHAREDMEM_H
#define _SHAREDMEM_H

/* Purely local shared memory functions-- converting System V shared
   memory to POSIX shared memory required something intermediate in
   System V IPC that returned a file descriptor suitable for mmap(),
   and that something doesn't exist. */

#include <sys/types.h>		/* key_t */
#include <stddef.h>		/* size_t */

#ifndef KEY_T_DEFINED
#define KEY_T_DEFINED
#endif /* KEY_T_DEFINED */

typedef struct {
    int id;
    void *addr;
    int create_errno;
    size_t size;
    int count;
    int created;
    int key;
    char name[64];
} shm_t;

#ifdef __cplusplus
extern "C" {
#endif
    extern shm_t *rcs_shm_open(key_t key, size_t size, int oflag,
	/* int mode */ ...);
    extern int rcs_shm_close(shm_t * shm);
    extern int rcs_shm_delete(shm_t * shm);
    extern int rcs_shm_nattch(shm_t * shm);

#ifdef __cplusplus
}
#endif
#endif
