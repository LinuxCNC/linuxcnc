#ifndef RTAPISHMEM_H
#define RTPAISHMEM_H

#ifdef __KERNEL__
extern int rtapishm_get_memory(int length, void **pointer);
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

#endif // RTPAISHMEM_H
