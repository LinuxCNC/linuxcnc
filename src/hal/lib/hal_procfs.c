// HAL procfs API - kernel threads only

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */

// maximum argc passed to hal_call_userfunct()
#define MAX_ARGV 50

#if defined(BUILD_SYS_USER_DSO)
#undef CONFIG_PROC_FS
#endif

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <linux/string.h>
extern struct proc_dir_entry *rtapi_dir;
static struct proc_dir_entry *hal_dir = 0;
static struct proc_dir_entry *hal_rtapicmd = 0;

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
// NB: this should be move to an iocontrol, procfs doesnt cut it
static int proc_write_rtapicmd(struct file *file,
        const char *buffer, unsigned long count, void *data)
{
    char cmd[20], name[HAL_NAME_LEN + 1];
    unsigned long period;
    int fp, cpu, retval;

    if (!strncmp(buffer,"newthread", 9)) {
	if ((retval = sscanf(buffer, "%s %s %lu %d %d",
			     cmd, name, &period, &fp, &cpu)) != 5) {
	    HALERR("newthread: expecting 5 items (s:cmd s:name d:period d:fp d:cpu), got %d",
		   retval);
	    return -EINVAL;
	}
	if ((period > 0) &&
	    (strlen(name) > 0)) {
	    retval = hal_create_thread(name, period, fp, cpu);
	    if (retval < 0) {
		HALERR("newthread: could not create thread '%s' - error %d",
		       name, retval);
		return retval;
	    } else {
		HALINFO("newthread: created %ld uS thread '%s' fp=%d cpu=%d",
			period / 1000, name, fp, cpu);
	    }
	}
    } else if (!strncmp(buffer, "delthread", 9)) {
	if ((retval = sscanf(buffer, "%s %s", cmd, name)) != 2) {
	    HALERR("delthread: expecting 2 items: 'delthread <threadname>'");
	    return -EINVAL;
	}
	if ((retval = hal_thread_delete(name)))  {
	    HALERR("delthread '%s' error %d", name, retval);
	    return retval;
	}
	HALINFO("delthread - thread '%s' deleted", name);
    } else {
	// rtapi_argvize modifies its third argument in-place
	char rwbuf[1024];
	strncpy(rwbuf, buffer, sizeof(rwbuf));

	char *argv[MAX_ARGV];
	int argc = rtapi_argvize(MAX_ARGV, argv, rwbuf);
	if (argc > 1) {
	    if (!strncmp(argv[0],"call", 4)) {
		int uret = 0, retval;
		retval = hal_call_usrfunct(argv[1],
					   argc-2,
					   (const char**)&argv[2],
					   &uret);
		if (retval)
		    return retval; // library return code
		// retvall == 0: uret signifies userfunct return value
		return uret;
	    }
	}

	HALERR("unrecognized rtapicmd: '%s'", cmd);
	return -EINVAL;
    }
    return count;
}

void hal_proc_clean(void) {
    if(hal_rtapicmd)
        remove_proc_entry("rtapicmd", hal_dir);
    if(hal_dir)
        remove_proc_entry("hal", rtapi_dir);
    hal_dir = hal_rtapicmd = 0;
}

int hal_proc_init(void) {
    if(!rtapi_dir) return 0;
    hal_dir = create_proc_entry("hal", S_IFDIR, rtapi_dir);
    if(!hal_dir) { hal_proc_clean(); return -1; }
    hal_rtapicmd = create_proc_entry("rtapicmd", 0666, hal_dir);
    if(!hal_rtapicmd) { hal_proc_clean(); return -1; }
    hal_rtapicmd->data = NULL;
    hal_rtapicmd->read_proc = NULL;
    hal_rtapicmd->write_proc = proc_write_rtapicmd;
    return 0;
}
#else
void hal_proc_clean(void) {}
int hal_proc_init(void) { return 0; }
#endif
