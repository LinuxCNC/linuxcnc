// HAL procfs API - kernel threads only

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */


#if defined(BUILD_SYS_USER_DSO)
#undef CONFIG_PROC_FS
#endif

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
extern struct proc_dir_entry *rtapi_dir;
static struct proc_dir_entry *hal_dir = 0;
static struct proc_dir_entry *hal_newinst_file = 0;
static struct proc_dir_entry *hal_threadcmd = 0;

static int proc_write_newinst(struct file *file,
        const char *buffer, unsigned long count, void *data)
{
    if(hal_data->pending_constructor) {
        hal_print_msg(RTAPI_MSG_DBG,
                "HAL: running constructor for %s %s\n",
                hal_data->constructor_prefix,
                hal_data->constructor_arg);
        hal_data->pending_constructor(hal_data->constructor_prefix,
                hal_data->constructor_arg);
        hal_data->pending_constructor = 0;
    }
    return count;
}

// simple interface to hal_create_thread()/hal_thread_delete()
// through /proc/rtapi/hal/threadcmd (kernel threadstyles only)
//
// to start a thread, write 'newthread' <threadname> <period> <fp> <cpu>'
// example:
//    echo newthread servo-thread 1000000 1 -1 >/proc/rtapi/hal/threadcmd
//
// to delete a thread, write 'delthread <threadname>'
//    echo delthread servo-thread >/proc/rtapi/hal/threadcmd
//
// HAL return values are reflected in the return value to write()
//
static int proc_write_threadcmd(struct file *file,
        const char *buffer, unsigned long count, void *data)
{
    char cmd[20], name[HAL_NAME_LEN + 1];
    unsigned long period;
    int fp, cpu, retval;

    if (!strncmp(buffer,"newthread", 9)) {
	if ((retval = sscanf(buffer, "%s %s %lu %d %d",
			     cmd, name, &period, &fp, &cpu)) != 5) {
	    hal_print_msg(RTAPI_MSG_ERR,
			    "HAL:newthread: expecting 5 items (s:cmd s:name d:period d:fp d:cpu), got %d\n",
			    retval);
	    return -EINVAL;
	}
	if ((period > 0) &&
	    (strlen(name) > 0)) {
	    retval = hal_create_thread(name, period, fp, cpu);
	    if (retval < 0) {
		hal_print_msg(RTAPI_MSG_ERR,
				"HAL:newthread: could not create thread '%s' - error %d\n",
				name, retval);
		return retval;
	    } else {
		hal_print_msg(RTAPI_MSG_INFO,
				"HAL:newthread: created %ld uS thread '%s' fp=%d cpu=%d\n",
				period / 1000, name, fp, cpu);
	    }
	}
    } else if (!strncmp(buffer, "delthread", 9)) {
	if ((retval = sscanf(buffer, "%s %s", cmd, name)) != 2) {
	    hal_print_msg(RTAPI_MSG_ERR,
			    "HAL:delthread: expecting 2 items: 'delthread <threadname>'\n");
	    return -EINVAL;
	}
	if ((retval = hal_thread_delete(name)))  {
	    hal_print_msg(RTAPI_MSG_ERR,
			    "HAL:delthread '%s' error %d\n", name, retval);
	    return retval;
	}
	hal_print_msg(RTAPI_MSG_INFO,
			"HAL:delthread - thread '%s' deleted\n", name);
    } else {
	hal_print_msg(RTAPI_MSG_ERR,
			"HAL: unrecognized threadcmd: '%s'\n", cmd);
	return -EINVAL;
    }
    return count;
}

void hal_proc_clean(void) {
    if(hal_newinst_file)
        remove_proc_entry("newinst", hal_dir);
    if(hal_threadcmd)
        remove_proc_entry("threadcmd", hal_dir);
    if(hal_dir)
        remove_proc_entry("hal", rtapi_dir);
    hal_newinst_file = hal_dir = hal_threadcmd = 0;
}

int hal_proc_init(void) {
    if(!rtapi_dir) return 0;
    hal_dir = create_proc_entry("hal", S_IFDIR, rtapi_dir);
    if(!hal_dir) { hal_proc_clean(); return -1; }
    hal_newinst_file = create_proc_entry("newinst", 0666, hal_dir);
    if(!hal_newinst_file) { hal_proc_clean(); return -1; }
    hal_newinst_file->data = NULL;
    hal_newinst_file->read_proc = NULL;
    hal_newinst_file->write_proc = proc_write_newinst;
    hal_threadcmd = create_proc_entry("threadcmd", 0666, hal_dir);
    if(!hal_threadcmd) { hal_proc_clean(); return -1; }
    hal_threadcmd->data = NULL;
    hal_threadcmd->read_proc = NULL;
    hal_threadcmd->write_proc = proc_write_threadcmd;
    return 0;
}
#else
void hal_proc_clean(void) {}
int hal_proc_init(void) { return 0; }
#endif
