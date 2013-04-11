#ifndef EMBSHMEM_H
#define EMBSHMEM_H

#ifdef __KERNEL__
extern int smemdrv_get_memory(int length, void **pointer);
#else // ! __KERNEL__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
extern void *ShmemGet(int length);
extern void ShmemRelease(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !__KERNEL__

#endif // EMBSHMEM_H
