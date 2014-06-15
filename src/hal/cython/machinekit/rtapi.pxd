# vim: sts=4 sw=4 et

cdef extern from "stdarg.h":
    ctypedef struct va_list:
        pass

cdef extern from "rtapi_shmkeys.h":
    cdef int DEFAULT_MOTION_SHMEM_KEY
    cdef int GLOBAL_KEY
    cdef int SCOPE_SHM_KEY
    cdef int HAL_KEY
    cdef int RTAPI_KEY
    cdef int RTAPI_RING_SHM_KEY


cdef extern from "rtapi_bitops.h":
    ctypedef unsigned long rtapi_atomic_type
    int RTAPI_BIT_TEST(rtapi_atomic_type *a, int b)
    int RTAPI_BIT(int b)

cdef extern from "rtapi.h":
    int rtapi_init(const char *name)
    int rtapi_exit(int comp_id)
    int rtapi_next_handle()

    void rtapi_mutex_give(unsigned long *mutex)
    void rtapi_mutex_get(unsigned long *mutex)
    int rtapi_mutex_try(unsigned long *mutex)

    long long int rtapi_get_time()
    long long int rtapi_get_clocks()

    int rtapi_shmem_new(int key, int module_id, unsigned long int size)
    int rtapi_shmem_delete(int shmem_id, int module_id)
    int rtapi_shmem_getptr(int shmem_id, void **ptr, unsigned long int *size)

    ctypedef int msg_level_t
    ctypedef void(*rtapi_msg_handler_t)(msg_level_t level, const char *fmt, va_list ap)
    ctypedef void (*rtapi_set_msg_handler_t)(rtapi_msg_handler_t)
    ctypedef rtapi_msg_handler_t (*rtapi_get_msg_handler_t)()

    cdef:
        int MSG_KERNEL
        int MSG_RTUSER
        int MSG_ULAPI

        int RTAPI_MSG_NONE
        int RTAPI_MSG_ERR
        int RTAPI_MSG_WARN
        int RTAPI_MSG_INFO
        int RTAPI_MSG_DBG
        int RTAPI_MSG_ALL


    void rtapi_set_msg_handler(rtapi_msg_handler_t handler)
    rtapi_msg_handler_t rtapi_get_msg_handler()
    int rtapi_set_logtag(const char *fmt, ...)
    const char *rtapi_get_logtag()
    void rtapi_print_msg(int level, const char *fmt, ...)
