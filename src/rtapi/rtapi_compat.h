/********************************************************************

* Copyright (C) 2012 - 2013 John Morris <john AT zultron DOT com>
*                           Michael Haberler <license AT mah DOT priv DOT at>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
********************************************************************/

#ifndef RTAPI_COMPAT_H
#define RTAPI_COMPAT_H


/***********************************************************************
*      basic features of thread flavors. Needed to init the flavor     *
*      descriptor in rtapi_compat.h and rtapi_switch->flavor_flags     *
*      technically those are flavor configuration descriptiors         *
************************************************************************/

#define  FLAVOR_DOES_IO        RTAPI_BIT(0) // userland: whether iopl() needs to be called
#define  FLAVOR_KERNEL_BUILD   RTAPI_BIT(1) // set when defined(BUILD_SYS_KBUILD)
#define  FLAVOR_RTAPI_DATA_IN_SHM   RTAPI_BIT(2) // this flavor keeps rtapi_data in a shm segment

#define POSIX_FLAVOR_FLAGS                 0

#define RTPREEMPT_FLAVOR_FLAGS             (FLAVOR_DOES_IO)

#define RTAI_KERNEL_FLAVOR_FLAGS           (FLAVOR_DOES_IO| \
					    FLAVOR_KERNEL_BUILD|\
					    FLAVOR_RTAPI_DATA_IN_SHM)

#define XENOMAI_KERNEL_FLAVOR_FLAGS        (FLAVOR_DOES_IO|\
					    FLAVOR_KERNEL_BUILD|\
					    FLAVOR_RTAPI_DATA_IN_SHM)

#define XENOMAI_FLAVOR_FLAGS               (FLAVOR_DOES_IO)


/***********************************************************************
*      Support functions for autodetection and flavor handling         *
*      these are available to user processes regardless of defining    *
*      RTAPI or ULAPI, for instance rtapi_msgd or rtapistat.           *
*      exported by rtapi_compat.c .                                    *
************************************************************************/
#ifndef MODULE

#include <limits.h> // provides PATH_MAX

typedef struct {
    const char *name;
    const char *mod_ext;	// RTAPI module extensions, .ko/.so
    const char *so_ext;		// ulapi.so module extension
    const char *build_sys;
    int id;
    unsigned long flags;
} flavor_t, *flavor_ptr;

// these functions must work with or without rtapi.h included
#if !defined(SUPPORT_BEGIN_DECLS)
#if defined(__cplusplus)
#define SUPPORT_BEGIN_DECLS extern "C" {
#define SUPPORT_END_DECLS }
#else
#define SUPPORT_BEGIN_DECLS
#define SUPPORT_END_DECLS
#endif
#endif

SUPPORT_BEGIN_DECLS

extern int is_module_loaded(const char *module);
extern int load_module(const char *module, const char *modargs);
extern int run_module_helper(const char *format, ...);
extern long int simple_strtol(const char *nptr, char **endptr, int base);


// simple interface to hal_create_thread()/hal_thread_delete()
// through /proc/rtapi/hal/rtapicmd (kernel threadstyles only)
//
// to start a thread, write 'newthread' <threadname> <period> <fp> <cpu>'
// example:
//    echo newthread servo-thread 1000000 1 -1 >/proc/rtapi/hal/rtapicmd
//
// to delete a thread, write 'delthread <threadname>'
//    echo delthread servo-thread >/proc/rtapi/hal/rtapicmd
//
// HAL return values are reflected in the return value to write()
//
#define PROCFS_RTAPICMD "/proc/rtapi/hal/rtapicmd"
extern int procfs_cmd(const char *path, const char *format, ...);

// kernel tests in rtapi_compat.c
extern int kernel_is_xenomai();
extern int kernel_is_rtai();
extern int kernel_is_rtpreempt();

// return the Xenomai group id from
// /sys/module/xeno_nucleus/parameters/xenomai_gid or -1
extern int xenomai_gid();

// if the linuxCNC user is'nt member of the xenomai group,
// most xenomai system calls will fail
extern int user_in_xenomai_group();

// return the instance ID of a kernel threads instance
// by consulting /proc/rtapi/instance
extern int kernel_instance_id();

extern flavor_t flavors[];
extern flavor_ptr flavor_byname(const char *flavorname);
extern flavor_ptr flavor_byid(int flavor_id);
extern flavor_ptr default_flavor(void);

// determine if this is a userland or kthreads flavor
static inline int kernel_threads(flavor_ptr f) {
    return (f->flags & FLAVOR_KERNEL_BUILD) != 0;
}

/*
 * Given a result buffer of PATH_MAX size and a module or shared
 * library's basename (e.g. 'rtapi' with no directory or '.ko'), find
 * the full path to a module/shared library as follows:
 *
 * Complete the module file name by appending mod_ext from flavor data
 * to the basename (e.g. 'rtapi.ko').
 *
 * For RIP builds, prepend RTLIB_DIR from rtapi.ini, flavor name and
 * kernel release to the module file name
 * (e.g. '/home/me/linuxcnc-dev/rtlib/rtai-kernel/2.6.38-rtai/rtapi.ko');
 * if the file exists, copy into *result and return 0.
 *
 * For non-RIP builds, prepend
 * '/lib/modules/<kernel-release>/linuxcnc' to the module file name
 * (e.g. '/lib/modules/2.6.38-rtai/linuxcnc/rtapi.ko'); if the file
 * exists, copy into *result and return 0.
 *
 * Otherwise, prepend any (flavor-specific, currently RTAI only) RTDIR
 * from rtapi.ini and prepend to module file name
 * (e.g. '/usr/realtime/modules/rtai_hal.ko'); if the file exists,
 * copy into *result and return 0.
 *
 * Otherwise return non-0.
 */

extern int module_path(char *result, const char *basename);

/*
 * Look up a parameter value in rtapi.ini, checking first the
 * [flavor_<flavor>] section, then the [global] section.  Returns 0 if
 * successful, 1 otherwise.  Maximum n-1 bytes of the value and a
 * trailing \0 is copied into *result.
 *
 * Beware:  this function calls exit(-1) if rtapi.ini cannot be
 * successfully opened!
 */

extern int get_rtapi_config(char *result, const char *param, int n);

// diagnostics: retrieve the rpath this binary was linked with
//
// returns malloc'd memory - caller MUST free returned string if non-null
// example:  cc -g -Wall -Wl,-rpath,/usr/local/lib -Wl,-rpath,/usr/lib foo.c -o foo
// rtapi_get_rpath() will return "/usr/local/lib:/usr/lib"

extern const char *rtapi_get_rpath(void);

// inspection of Elf objects (.so, .ko):
// retrieve raw data of Elf section section_name.
// returned in *dest on success.
// caller must free().
// returns size, or < 0 on failure.
int get_elf_section(const char *const fname, const char *section_name, void **dest);

// split the null-delimited strings in an .rtapi_caps Elf section into an argv.
// caller must free.
const char **get_caps(const char *const fname);

// given a path to an elf binary, and a capability name, return its value
// or NULL if not present.
// caller must free().
const char *get_cap(const char *const fname, const char *cap);


// given a module name and the flavor set, return the integer
// capability mask of tags.
int rtapi_get_tags(const char *mod_name);


SUPPORT_END_DECLS

#endif // MODULE
#endif // RTAPI_COMPAT_H
