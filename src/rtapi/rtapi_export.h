#ifndef RTAPI_EXPORT_H
#define RTAPI_EXPORT_H

#include "config.h"

#if defined(BUILD_SYS_USER_DSO)
#define MODULE_INFO1(t, a, c) __attribute__((section(".modinfo"))) \
    t rtapi_info_##a = c; EXPORT_SYMBOL(rtapi_info_##a);
#define MODULE_INFO2(t, a, b, c) __attribute__((section(".modinfo"))) \
    t rtapi_info_##a##_##b = c; EXPORT_SYMBOL(rtapi_info_##a##_##b);
#define MODULE_PARM(v,t) MODULE_INFO2(const char*, type, v, t) MODULE_INFO2(void*, address, v, &v)
#define MODULE_PARM_DESC(v,t) MODULE_INFO2(const char*, description, v, t)
#define MODULE_LICENSE(s) MODULE_INFO1(const char*, license, s)
#define MODULE_AUTHOR(s) MODULE_INFO1(const char*, author, s)
#define MODULE_DESCRIPTION(s) MODULE_INFO1(const char*, description, s)
#define MODULE_SUPPORTED_DEVICE(s) MODULE_INFO1(const char*, supported_device, s)
#define EXPORT_SYMBOL(x) __attribute__((section(".rtapi_export"))) \
    char rtapi_exported_##x[] = #x;
#define EXPORT_SYMBOL_GPL(sym) EXPORT_SYMBOL(sym)
#define MODULE_DEVICE_TABLE(type, name)
#endif


#endif //RTAPI_EXPORT_H
