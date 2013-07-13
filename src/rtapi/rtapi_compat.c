#include "config.h"
#include "rtapi.h"
#include "inifile.h"           /* iniFind() */

#include <stdio.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <limits.h>		/* PATH_MAX */
#include <stdlib.h>		/* exit() */

// really in nucleus/heap.h but we rather get away with minimum include files
#ifndef XNHEAP_DEV_NAME
#define XNHEAP_DEV_NAME  "/dev/rtheap"
#endif

// if this exists, and contents is '1', it's RT_PREEMPT
#define PREEMPT_RT_SYSFS "/sys/kernel/realtime"

// dev/rtai_shm visible only after 'realtime start'
#define DEV_RTAI_SHM "/dev/rtai_shm"

// static storage of kernel module directory
static char kmodule_dir[PATH_MAX];

FILE *rtapi_inifile = NULL;

int kernel_is_xenomai()
{
    struct stat sb;
    return ((stat(XNHEAP_DEV_NAME, &sb) == 0)  &&
	    ((sb.st_mode & S_IFMT) == S_IFCHR));
}

int kernel_is_rtai()
{
    struct stat sb;
    // this works only after 'realtime start'
    return ((stat(DEV_RTAI_SHM, &sb) == 0)  && ((sb.st_mode & S_IFMT) == S_IFCHR));
}

int kernel_is_rtpreempt()
{
    FILE *fd;
    int retval = 0;

    if ((fd = fopen(PREEMPT_RT_SYSFS,"r")) != NULL) {
	int flag;
	retval = ((fscanf(fd, "%d", &flag) == 1) && (flag));
	fclose(fd);
    }
    return retval;
}


flavor_t flavors[] = {
    // #if BUILD_POSIX

    { .name = "posix",
      .mod_ext = ".so",
      .so_ext = ".so",
      .build_sys = "user-dso",
      .id = RTAPI_POSIX_ID,
      .flags = 0
    },
    { .name = "sim", // alias - old habíts die hard
      .mod_ext = ".so",
      .so_ext = ".so",
      .build_sys = "user-dso",
      .id = RTAPI_POSIX_ID,
      .flags = 0
    },
    // #endif

    // #if BUILD_RT_PREEMPT
    { .name = "rt-preempt",
      .mod_ext = ".so",
      .so_ext = ".so",
      .build_sys = "user-dso",
      .id = RTAPI_RT_PREEMPT_ID,
      .flags = FLAVOR_DOES_IO,
    },
    // #endif
    // #if BUILD_XEMNOMAI

    { .name = "xenomai",
      .mod_ext = ".so",
      .so_ext = ".so",
      .build_sys = "user-dso",
      .id = RTAPI_XENOMAI_ID,
      .flags = FLAVOR_DOES_IO,
    },
    // #endif

    // #if BUILD_RTAI_KERNEL

    { .name = "rtai-kernel",
      .mod_ext = ".ko",
      .so_ext = ".so",
      .build_sys = "kbuild",
      .id = RTAPI_RTAI_KERNEL_ID,
      .flags = FLAVOR_DOES_IO|FLAVOR_KERNEL_BUILD,
    },
    // #endif

    // #if BUILD_XENOMAI_KERNEL

    { .name = "xenomai-kernel",
      .mod_ext = ".ko",
      .so_ext = ".so",
      .build_sys = "kbuild",
      .id = RTAPI_XENOMAI_KERNEL_ID,
      .flags = FLAVOR_DOES_IO|FLAVOR_KERNEL_BUILD,
    },
    // #endif

    { .name = NULL, // sentinel
      .id = -1,
      .flags = 0
    }
};

flavor_ptr flavor_byname(const char *flavorname)
{
    flavor_ptr f = flavors;
    while (f->name) {
	if (!strcasecmp(flavorname, f->name))
	    return f;
	f++;
    }
    return NULL;
}

flavor_ptr flavor_byid(int flavor_id)
{
    flavor_ptr f = flavors;
    while (f->name) {
	if (flavor_id == f->id)
	    return f;
	f++;
    }
    return NULL;
}

flavor_ptr default_flavor(void)
{
    char *fname = getenv("FLAVOR");
    flavor_ptr f, flavor;

    if (fname) {
	if ((flavor = flavor_byname(fname)) == NULL) {
	    fprintf(stderr, 
		    "FLAVOR=%s: no such flavor -- valid flavors are:\n",
		    fname);
	    f = flavors;
	    while (f->name) {
		fprintf(stderr, "\t%s\n", f->name);
		f++;
	    }
	    exit(1);
	}
	return flavor;
    }

    /* FIXME

       Need to check whether the flavor is available; e.g.  xenomai
       kthreads has been built but xenomai userland has not.
    */

    if (kernel_is_rtai())
	return flavor_byid(RTAPI_RTAI_KERNEL_ID);
    if (kernel_is_xenomai())
	return flavor_byid(RTAPI_XENOMAI_ID);
    if (kernel_is_rtpreempt())
	return flavor_byid(RTAPI_RT_PREEMPT_ID);
    return flavor_byid(RTAPI_POSIX_ID);
}


void check_rtapi_config_open()
{
    /* Open rtapi.ini if needed.  Private function used by
       get_rtapi_config(). */
    char config_file[PATH_MAX];

    if (rtapi_inifile == NULL) {
	/* it's the first -i (ignore repeats) */
	/* there is a following arg, and it's not an option */
	snprintf(config_file, PATH_MAX,
		 "%s/rtapi.ini", EMC2_SYSTEM_CONFIG_DIR);
	rtapi_inifile = fopen(config_file, "r");
	if (rtapi_inifile == NULL) {
	    fprintf(stderr,
		    "Could not open ini file '%s'\n",
		    config_file);
	    exit(-1);
	}
	/* make sure file is closed on exec() */
	fcntl(fileno(rtapi_inifile), F_SETFD, FD_CLOEXEC);
    }
}

int get_rtapi_config(char *result, const char *param, int n)
{
    /* Read a parameter value from rtapi.ini.  First try the flavor
       section, then the global section.  Copy max n-1 bytes into
       result buffer.  */
    char *val;
    char buf[RTAPI_NAME_LEN+8];    // strlen("flavor_") + RTAPI_NAME_LEN + 1

    // Open rtapi_inifile if it hasn't been already
    check_rtapi_config_open();

    sprintf(buf, "flavor_%s", default_flavor()->name);
    val = (char *) iniFind(rtapi_inifile, param, buf);

    if (val==NULL)
	val = (char *) iniFind(rtapi_inifile, param,"global");

    // Return if nothing found
    if (val==NULL) {
	result[0] = 0;
	return -1;
    }

    // Otherwise copy result into buffer (see 'WTF' comment in inifile.cc)
    strncpy(result, val, n-1);
    return 0;
}


int module_path(char *result, const char *basename)
{
    /* Find a kernel module's path */
    struct stat sb;
    char buf[PATH_MAX];
    struct utsname uts;
	
    // Initialize kmodule_dir, only once
    if (kmodule_dir[0] == 0) {
	uname(&uts);

	get_rtapi_config(buf,"RUN_IN_PLACE",4);
	if (strncmp(buf,"yes",3) == 0) {
	    // Complete RTLIB_DIR should be <RTLIB_DIR>/<flavor>/<uname -r>
	    if (get_rtapi_config(buf,"RTLIB_DIR",PATH_MAX) != 0)
		return -ENOENT;

	    snprintf(kmodule_dir,PATH_MAX,"%s/%s/%s",
		     buf, default_flavor()->name, uts.release);
	} else {
	    // Complete RTLIB_DIR should be /lib/modules/<uname -r>/linuxcnc
	    snprintf(kmodule_dir, PATH_MAX,
		     "/lib/modules/%s/linuxcnc", uts.release);
	}
    }

    // Look for module in kmodule_dir/RTLIB_DIR
    snprintf(result, PATH_MAX, "%s/%s%s",
	     kmodule_dir,
	     basename,
	     default_flavor()->mod_ext);
    if ((stat(result, &sb) == 0)  && (S_ISREG(sb.st_mode)))
	return 0;

    // Check RTDIR as well (RTAI)
    if (get_rtapi_config(buf, "RTDIR", PATH_MAX) != 0 || buf[0] == 0)
	return -ENOENT;  // not defined or empty
    snprintf(result, PATH_MAX, "%s/%s%s",
	     buf, basename, default_flavor()->mod_ext);
    if ((stat(result, &sb) == 0)  && (S_ISREG(sb.st_mode)))
	return 0;

    // Module not found
    return -ENOENT;
}


void ulapi_kernel_compat_check(rtapi_switch_t *rtapi_switch, char *ulapi_lib)
{
    // verify the ulapi-foo.so we just loaded is compatible with
    // the running kernel if it has special prerequisites

    switch (rtapi_switch->thread_flavor_id) {
    case  RTAPI_RT_PREEMPT_ID:
	if (!kernel_is_rtpreempt()) {
	    fprintf(stderr,
		    "HAL_LIB: ERROR - RT_PREEMPT ULAPI loaded but kernel is "
		    "not RT_PREEMPT (%s, %s)\n",
		    ulapi_lib, rtapi_switch->git_version);
	    exit(1);
	}
	break;
    case RTAPI_XENOMAI_KERNEL_ID:
    case RTAPI_XENOMAI_ID:
	if (!kernel_is_xenomai()) {
	    fprintf(stderr,
		    "HAL_LIB: ERROR - Xenomai ULAPI loaded but kernel is "
		    "not Xenomai (%s, %s)\n",
		    ulapi_lib, rtapi_switch->git_version);
	    exit(1);
	}
	break;
    case RTAPI_RTAI_KERNEL_ID:
	if (!kernel_is_rtai()) {
	    fprintf(stderr,
		    "HAL_LIB: ERROR - RTAI ULAPI loaded but kernel is "
		    "not RTAI (%s, %s)\n",
		    ulapi_lib, rtapi_switch->git_version);
	    exit(1);
	}
	break;
    default:
	// no prerequisites for vanilla
	break;
    }
}
