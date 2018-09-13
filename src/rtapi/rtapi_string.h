//    Copyright 2006-2009, Jeff Epler
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#ifndef RTAPI_STRING_H
#define RTAPI_STRING_H

#include <rtapi.h>

#ifdef __cplusplus
#include <assert.h>
#include <type_traits>
#define rtapi_static_assert(a,b) static_assert(a,b)
#define rtapi_is_array(x) (std::is_array<decltype(x)>::value)
#else
#define rtapi_static_assert(a,b) _Static_assert(a,b)
#define rtapi_is_array(x) (!__builtin_types_compatible_p(__typeof__((x)), __typeof__(&(x)[0])))
#endif

#ifdef MODULE
/* Suspect only very early kernels are missing the basic string functions.
   To be sure, see what has been implemented by looking in linux/string.h
   and {linux_src_dir}/lib/string.c */
#include <linux/string.h>
#include <linux/version.h>
#define rtapi_argv_split argv_split
#define rtapi_argv_free argv_free
#define rtapi_kstrdup(a,b) kstrdup(a,b)
#else
#include <string.h>
#include <rtapi_gfp.h>
RTAPI_BEGIN_DECLS
extern char **rtapi_argv_split(rtapi_gfp_t, const char *argstr, int *argc);
extern void rtapi_argv_free(char **argv);
#define rtapi_kstrdup(a,b) strdup(a)
RTAPI_END_DECLS
#endif
RTAPI_BEGIN_DECLS
static inline size_t rtapi_strlcpy(char *dst, const char *src, size_t size) {
    return rtapi_snprintf(dst, size, "%s", src);
}
#define rtapi_strxcpy(dst, src) ({ \
    rtapi_static_assert(rtapi_is_array(dst), "dst must be non-const array"); \
    rtapi_strlcpy(dst, src, sizeof(dst)); \
})

static inline size_t rtapi_strlcat(char *dst, const char *src, size_t size) {
    size_t l = strlen(dst);
    return rtapi_snprintf(dst+l, size-l, "%s", src);
}

#define rtapi_strxcat(dst, src) ({ \
    rtapi_static_assert(rtapi_is_array(dst), "dst must be non-const array"); \
    rtapi_strlcat(dst, src, sizeof(dst)); \
})
RTAPI_END_DECLS

#endif
