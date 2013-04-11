#ifndef RTAPISHMEM_H
#define RTPAISHMEM_H

#ifdef __KERNEL__
extern int rtapishm_get_memory(int length, void **pointer);

#define DEVICE_NAME "shm"
#define CLASS_NAME "rtapi"

#define AUTHOR "Michael Haberler <git@mah.priv.at>"
#define DESCRIPTION "rtapi shared memory driver"
#define VERSION "0.1"

#define dbg(format, arg...)						\
    do {								\
	if (debug) pr_info(CLASS_NAME ": %s: " format  "\n", __FUNCTION__ , ## arg);	\
    } while (0)

#define err(format, arg...)  pr_err(CLASS_NAME ": " format "\n", ## arg)
#define info(format, arg...) pr_info(CLASS_NAME ": " format "\n", ## arg)
#define warn(format, arg...) pr_warn(CLASS_NAME ": " format "\n", ## arg)

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


#endif // RTPAISHMEM_H
