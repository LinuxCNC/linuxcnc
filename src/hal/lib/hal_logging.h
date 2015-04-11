#ifndef HAL_LOGGING_H
#define HAL_LOGGING_H

#include <rtapi.h>

// checking & logging shorthands
#define HALERR(fmt, ...)					\
    rtapi_print_loc(RTAPI_MSG_ERR,__FUNCTION__,__LINE__,	\
		    "HAL error:", fmt, ## __VA_ARGS__)
#define HALWARN(fmt, ...)					\
    rtapi_print_loc(RTAPI_MSG_WARN,__FUNCTION__,__LINE__,	\
		    "HAL WARNING:", fmt, ## __VA_ARGS__)
#define HALINFO(fmt, ...)					\
    rtapi_print_loc(RTAPI_MSG_INFO,__FUNCTION__,__LINE__,	\
		    "HAL info:", fmt, ## __VA_ARGS__)

#define HALDBG(fmt, ...)					\
    rtapi_print_loc(RTAPI_MSG_DBG,__FUNCTION__,__LINE__,	\
		    "HAL:", fmt, ## __VA_ARGS__)

#define HAL_ASSERT(x)						\
    do {							\
	if (!(x)) {						\
	    rtapi_print_loc(RTAPI_MSG_ERR,			\
			    __FUNCTION__,__LINE__,		\
			    "HAL error:",			\
			    "ASSERTION VIOLATED: '%s'", #x);	\
	}							\
    } while(0)


#define CHECK_HALDATA()					\
    do {						\
	if (hal_data == 0) {				\
	    rtapi_print_loc(RTAPI_MSG_ERR,		\
			    __FUNCTION__,__LINE__,	\
			    "HAL error:",		\
			    "called before init");	\
	    return -EINVAL;				\
	}						\
    } while (0)

#define CHECK_NULL(p)						\
    do {							\
	if (p == NULL) {					\
	    rtapi_print_loc(RTAPI_MSG_ERR,			\
			    __FUNCTION__,__LINE__,"HAL error:",	\
			    #p  " is NULL");			\
	    return -EINVAL;					\
	}							\
    } while (0)

#define CHECK_LOCK(ll)							\
    do {								\
	if (hal_data->lock & ll) {					\
	    rtapi_print_loc(RTAPI_MSG_ERR,				\
			    __FUNCTION__, __LINE__,"HAL error:",	\
			    "called while HAL is locked (%d)",		\
			    ll);					\
	    return -EPERM;						\
	}								\
    } while(0)


#define CHECK_STR(name)							\
    do {								\
	if ((name) == NULL) {						\
	    rtapi_print_loc(RTAPI_MSG_ERR,__FUNCTION__, __LINE__,"HAL error:", \
			    "argument '" # name  "' is NULL");		\
	    return -EINVAL;						\
	}								\
    } while(0)


#define CHECK_STRLEN(name, len)						\
    do {								\
	CHECK_STR(name);						\
	if (strlen(name) > len) {					\
	    rtapi_print_loc(RTAPI_MSG_ERR,__FUNCTION__, __LINE__,"HAL error:", \
			    "argument '%s' too long (%zu/%d)",		\
			    name, strlen(name), len);			\
	    return -EINVAL;						\
	}								\
    } while(0)

#define NOMEM(fmt, ...)						\
    do {							\
	rtapi_print_loc(RTAPI_MSG_ERR,				\
			__FUNCTION__, __LINE__,"HAL error:",	\
			" insufficient memory for: "  fmt,	\
			## __VA_ARGS__);			\
	return -ENOMEM;						\
    } while(0)

#endif // HAL_LOGGING_H
