#include "config.h"
#include "rtapi.h"

#include <stdio.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <limits.h>

// really in nucleus/heap.h but we rather get away with minimum include files
#ifndef XNHEAP_DEV_NAME
#define XNHEAP_DEV_NAME  "/dev/rtheap"
#endif

// if this exists, and contents is '1', it's RT_PREEMPT
#define PREEMPT_RT_SYSFS "/sys/kernel/realtime"

// dev/rtai_shm visible only after 'realtime start'
#define DEV_RTAI_SHM "/dev/rtai_shm"

static struct utsname uts;

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
      .id = RTAPI_POSIX_ID,
      .flags = 0
    },
    { .name = "sim", // alias - old habíts die hard
      .mod_ext = ".so",
      .so_ext = ".so",
      .id = RTAPI_POSIX_ID,
      .flags = 0
    },
    // #endif

    // #if BUILD_RT_PREEMPT
    { .name = "rt-preempt-user",
      .mod_ext = ".so",
      .so_ext = ".so",
      .id = RTAPI_RT_PREEMPT_USER_ID,
      .flags = FLAVOR_DOES_IO,
    },
    // #endif
    // #if BUILD_XEMNOMAI_USER

    { .name = "xenomai-user",
      .mod_ext = ".so",
      .so_ext = ".so",
      .id = RTAPI_XENOMAI_USER_ID,
      .flags = FLAVOR_DOES_IO,
    },
    // #endif

    // #if BUILD_RTAI_KERNEL

    { .name = "rtai",
      .mod_ext = ".ko",
      .so_ext = ".so",
      .id = RTAPI_RTAI_KERNEL_ID,
      .flags = FLAVOR_DOES_IO|FLAVOR_KERNEL_BUILD,
    },
    // #endif

    // #if BUILD_XENOMAI_KERNEL

    { .name = "xenomai-kernel",
      .mod_ext = ".ko",
      .so_ext = ".so",
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

    // hack around single target builds for now
    // force default to waht we built for
#ifdef RTAPI_POSIX
    return flavor_byid(RTAPI_POSIX_ID);
#endif
#ifdef RTAPI_XENOMAI_USER
    return flavor_byid(RTAPI_XENOMAI_USER_ID);
#endif
#ifdef RTAPI_XENOMAI_KERNEL
    return flavor_byid(RTAPI_XENOMAI_KERNEL_ID);
#endif
#ifdef  RTAPI_RTPREEMPT_USER
    return flavor_byid(RTAPI_RT_PREEMPT_USER_ID);
#endif
#ifdef RTAPI_RTAI
    return flavor_byid(RTAPI_RTAI_KERNEL_ID);
#endif
    return NULL;
#if 0
    // this can be enabled once multiple targets per build are possible
    // to clever ;)

    if (kernel_is_rtai())
	return flavor_byid(RTAPI_RTAI_KERNEL_ID);
    if (kernel_is_xenomai())
	return flavor_byid(RTAPI_XENOMAI_USER_ID);
    if (kernel_is_rtpreempt())
	return flavor_byid(RTAPI_RT_PREEMPT_USER_ID);
    return flavor_byid(RTAPI_POSIX_ID);
#endif
}

int module_path(flavor_ptr f, 
		char *result,
		const char *libpath, 
		const char *basename, 
		const char *ext)
{
    struct stat sb;

    if (!uts.release[0])
	uname(&uts);
	
    snprintf(result, PATH_MAX, "%s/%s/%s/%s%s",
	     libpath, f->name, uts.release, 
	     basename, ext);
    if ((stat(result, &sb) == 0)  && (S_ISREG(sb.st_mode)))
	return 0;

    snprintf(result, PATH_MAX, "%s/%s/%s%s",
	     libpath, f->name,  
	     basename, ext);
    if ((stat(result, &sb) == 0)  && (S_ISREG(sb.st_mode)))
	return 0;
   
    snprintf(result, PATH_MAX, "%s/%s%s",
	     libpath, 
	     basename, ext);
    if ((stat(result, &sb) == 0)  && (S_ISREG(sb.st_mode)))
	return 0;

    return -ENOENT;
}
