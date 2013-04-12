#ifndef SHMEM_H
#define SHMEM_H

#define DEVICE_NAME "shmdrv"

#ifdef __KERNEL__
extern int shm_get_memory(int length, void **pointer);


#define AUTHOR "Michael Haberler <git@mah.priv.at>"
#define DESCRIPTION "shared memory driver"
#define VERSION "0.1"

#define dbg(format, arg...)						\
    do {								\
	if (debug) pr_info(DEVICE_NAME ": %s: " format  "\n", __FUNCTION__ , ## arg);	\
    } while (0)

#define err(format, arg...)  pr_err(DEVICE_NAME ": " format "\n", ## arg)
#define info(format, arg...) pr_info(DEVICE_NAME ": " format "\n", ## arg)
#define warn(format, arg...) pr_warn(DEVICE_NAME ": " format "\n", ## arg)

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



struct shm_ioctlmsg {
    int  key;
    int id;
    size_t size;
    void *addr;
    int flags;
};

#define SHM_IOC_MAGIC    'r'

#define IOC_SHM_EXISTS   _IOR(SHM_IOC_MAGIC, 1, int)
#define IOC_SHM_CREATE   _IOR(SHM_IOC_MAGIC, 2, struct shm_ioctlmsg)
#define IOC_SHM_ATTACH   _IOW(SHM_IOC_MAGIC, 3, struct shm_ioctlmsg)
#define IOC_SHM_DELETE   _IOR(SHM_IOC_MAGIC, 4, struct shm_ioctlmsg)


#endif // SHMEM_H
