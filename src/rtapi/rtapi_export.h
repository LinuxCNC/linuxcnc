/********************************************************************
*     Copyright 2006-2013 Various Authors
* 
*     This program is free software; you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation; either version 2 of the License, or
*     (at your option) any later version.
* 
*     This program is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
* 
*     You should have received a copy of the GNU General Public License
*     along with this program; if not, write to the Free Software
*     Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
********************************************************************/

#ifndef RTAPI_EXPORT_H
#define RTAPI_EXPORT_H

#include "config.h"

#define RTAPI_MP_SYMPREFIX "rtapi_info_"
#define RTAPI_IP_SYMPREFIX "rtapi_instinfo_"


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

// instance params
#define INSTANCE_INFO1(t, a, c) __attribute__((section(".modinfo"))) \
    t rtapi_instinfo_##a = c; EXPORT_SYMBOL(rtapi_instinfo_##a);
#define INSTANCE_INFO2(t, a, b, c) __attribute__((section(".modinfo"))) \
    t rtapi_instinfo_##a##_##b = c; EXPORT_SYMBOL(rtapi_instinfo_##a##_##b);
#define INSTANCE_PARM(v,t) INSTANCE_INFO2(const char*, type, v, t) INSTANCE_INFO2(void*, address, v, &v)
#define INSTANCE_PARM_DESC(v,t) INSTANCE_INFO2(const char*, description, v, t)


#endif


#endif //RTAPI_EXPORT_H
