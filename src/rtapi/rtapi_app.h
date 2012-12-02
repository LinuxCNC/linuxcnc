#ifndef RTAPI_APP_H
#define RTAPI_APP_H

#include "config.h"

#if defined(BUILD_SYS_USER_DSO)
EXPORT_SYMBOL(rtapi_app_main);
EXPORT_SYMBOL(rtapi_app_exit);
#else  /*  BUILD_SYS_KBUILD  */
#define rtapi_app_main(a) init_module(a)
#define rtapi_app_exit(a) cleanup_module(a)
#endif

#endif /* RTAPI_APP_H */
