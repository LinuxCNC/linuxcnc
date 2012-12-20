#ifndef RTAPI_LIMITS_H
#define RTAPI_LIMITS_H

#include "config.h"

#if defined(RTAPI) && defined(BUILD_SYS_KBUILD)
#include <linux/kernel.h>
#else
#include <limits.h>
#endif

#endif
