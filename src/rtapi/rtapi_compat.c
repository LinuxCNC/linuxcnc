// miscellaneos functions, mostly used during startup in
// a user process; RTAPI not necessarily available

#include "config.h"
#include "rtapi.h"
#include "rtapi_compat.h"
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

// Exists on RTAI and Xenomai
#define PROC_IPIPE "/proc/ipipe"

// Exists on Xenomai but not on RTAI
#define PROC_IPIPE_XENOMAI "/proc/ipipe/Xenomai"

// static storage of kernel module directory
static char kmodule_dir[PATH_MAX];

FILE *rtapi_inifile = NULL;

int kernel_is_xenomai()
{
    struct stat sb;

    return ((stat(XNHEAP_DEV_NAME, &sb) == 0) &&
	    (stat(PROC_IPIPE_XENOMAI, &sb) == 0));
}

int kernel_is_rtai()
{
    struct stat sb;

    return ((stat(PROC_IPIPE, &sb) == 0) && 
	    (stat(PROC_IPIPE_XENOMAI, &sb) != 0));
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
    { .name = RTAPI_POSIX_NAME,
      .mod_ext = ".so",
      .so_ext = ".so",
      .build_sys = "user-dso",
      .id = RTAPI_POSIX_ID,
      .flags = POSIX_FLAVOR_FLAGS // FLAVOR_USABLE
    },
    { .name = "sim", // alias for above- old habíts die hard
      .mod_ext = ".so",
      .so_ext = ".so",
      .build_sys = "user-dso",
      .id = RTAPI_POSIX_ID,
      .flags = POSIX_FLAVOR_FLAGS
    },
    { .name = RTAPI_RT_PREEMPT_NAME,
      .mod_ext = ".so",
      .so_ext = ".so",
      .build_sys = "user-dso",
      .id = RTAPI_RT_PREEMPT_ID,
      .flags = RTPREEMPT_FLAVOR_FLAGS
    },
     { .name = RTAPI_XENOMAI_NAME,
      .mod_ext = ".so",
      .so_ext = ".so",
      .build_sys = "user-dso",
      .id = RTAPI_XENOMAI_ID,
      .flags = XENOMAI_FLAVOR_FLAGS
    },
    { .name = RTAPI_RTAI_KERNEL_NAME,
      .mod_ext = ".ko",
      .so_ext = ".so",
      .build_sys = "kbuild",
      .id = RTAPI_RTAI_KERNEL_ID,
      .flags = RTAI_KERNEL_FLAVOR_FLAGS
    },

    { .name = RTAPI_XENOMAI_KERNEL_NAME,
      .mod_ext = ".ko",
      .so_ext = ".so",
      .build_sys = "kbuild",
      .id = RTAPI_XENOMAI_KERNEL_ID,
      .flags =  XENOMAI_KERNEL_FLAVOR_FLAGS
    },

    { .name = RTAPI_NOTLOADED_NAME,
      .mod_ext = "",
      .so_ext = "",
      .build_sys = "n/a",
      .id = RTAPI_NOTLOADED_ID,
      .flags = 0
    },

    { .name = NULL, // list sentinel
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
    char rtlib_result[PATH_MAX];
    int has_rtdir;
    struct utsname uts;
	
    // Initialize kmodule_dir, only once
    if (kmodule_dir[0] == 0) {
	uname(&uts);

	get_rtapi_config(buf,"RUN_IN_PLACE",4);
	if (strncmp(buf,"yes",3) == 0) {
	    // Complete RTLIB_DIR should be <RTLIB_DIR>/<flavor>/<uname -r>
	    if (get_rtapi_config(buf,"RTLIB_DIR",PATH_MAX) != 0)
		return -ENOENT;

	    if (strcmp(default_flavor()->build_sys,"user-dso") == 0) {
		// point user threads to a common directory
		snprintf(kmodule_dir,PATH_MAX,"%s/userland/%s",
			 buf, uts.release);
	    } else {
		// kthreads each have their own directory
		snprintf(kmodule_dir,PATH_MAX,"%s/%s/%s",
			 buf, default_flavor()->name, uts.release);
	    }
	} else {
	    // Complete RTLIB_DIR should be /lib/modules/<uname -r>/linuxcnc
	    snprintf(kmodule_dir, PATH_MAX,
		     "/lib/modules/%s/linuxcnc", uts.release);
	}
    }

    // Look for module in kmodule_dir/RTLIB_DIR
    snprintf(result, PATH_MAX, "%s/%s.ko", kmodule_dir, basename);
    if ((stat(result, &sb) == 0)  && (S_ISREG(sb.st_mode)))
	return 0;

    // Not found; save result for possible later diagnostic msg
    strcpy(rtlib_result,result);

    // Check RTDIR as well (RTAI)
    has_rtdir = (get_rtapi_config(buf, "RTDIR", PATH_MAX) == 0 && buf[0] != 0);
    if (has_rtdir) {
	snprintf(result, PATH_MAX, "%s/%s.ko", buf, basename);
	if ((stat(result, &sb) == 0)  && (S_ISREG(sb.st_mode)))
	    return 0;
    }

    // Module not found
    fprintf(stderr, "module '%s.ko' not found in directory\n\t%s\n",
	    basename, kmodule_dir);
    if (has_rtdir)
	fprintf(stderr, "\tor directory %s\n", buf);

    return -ENOENT;
}

int is_module_loaded(const char *module)
{
    FILE *fd;
    char line[100];
    int len = strlen(module);

    fd = fopen("/proc/modules", "r");
    if (fd == NULL) {
	fprintf(stderr, "module_loaded: ERROR: cannot read /proc/modules\n");
        return -1;
    }
    while (fgets(line, sizeof(line), fd)) {
        if (!strncmp(line, module, len)) {
            fclose(fd);
            return 1;
        }
    }
    fclose(fd);
    return 0;
}

int load_module(const char *module, const char *modargs)
{
    char mod_helper[PATH_MAX];
    char line[PATH_MAX + 100];
    int retval;

    if (get_rtapi_config(mod_helper, "linuxcnc_module_helper", PATH_MAX) != 0) {
        fprintf(stderr, "load_module: ERROR: failed to read "
		"linuxcnc_module_helper path from rtapi config\n");
	return -1;
    }
	
    sprintf(line, "%s insert %s %s", mod_helper,
	    module, modargs ? modargs : "");
    if ((retval = system(line))) {
        fprintf(stderr, "load_module: ERROR: executing '%s'  %d - %s\n",
                        line, errno, strerror(errno));
        return retval;
    }
    return 0;
}
