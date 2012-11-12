#if defined(RTAPI_XENOMAI_USER)
extern RT_TASK ostask_array[];
extern RT_TASK *ostask_self[];
#endif

#if defined(RTAPI_XENOMAI_KERNEL)

#define MASTER_HEAP "rtapi-heap"

extern RT_TASK *ostask_array[];
#endif

