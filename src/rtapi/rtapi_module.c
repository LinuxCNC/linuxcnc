/********************************************************************
* Description:  rtapi_module.c
*
*               This file, 'rtapi_module.c', implements the
*               init_module(), cleanup_module(), rtapi_init(),
*               rtapi_exit() and module_delete() functions for kernel
*               space thread systems.
*
*		It should not be built for userspace thread systems.
*
********************************************************************/

#include "config.h"		// build configuration
#include "rtapi.h"		// these functions
#include "rtapi_common.h"	// RTAPI macros and decls


#ifdef MODULE
#include "rtapi_proc.h"		/* proc filesystem decls & code */

#  if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
#    include <linux/cpumask.h>	/* NR_CPUS, cpu_online() */
#  endif
#endif


#ifdef RTAPI
/* the following are internal functions that do the real work associated
   with deleting tasks, etc.  They do not check the mutex that protects
   the internal data structures.  When someone calls an rtapi_xxx_delete()
   function, the rtapi funct gets the mutex before calling one of these
   internal functions.  When internal code that already has the mutex
   needs to delete something, it calls these functions directly.
*/
static int module_delete(int module_id);
extern int rtapi_module_master_shared_memory_init(rtapi_data_t **rtapi_data);
extern void rtapi_module_cleanup_hook(void);
extern void rtapi_module_master_shared_memory_free(void);

/***********************************************************************
*                   INIT AND SHUTDOWN FUNCTIONS                        *
************************************************************************/

#ifdef HAVE_RTAPI_MODULE_INIT_HOOK
void rtapi_module_init_hook(void);
#endif

int init_module(void) {
  int n, res;

    /* say hello */
    rtapi_print_msg(RTAPI_MSG_INFO, "RTAPI: Init\n");
    /* get master shared memory block from OS and save its address */
    res = rtapi_module_master_shared_memory_init(&rtapi_data);
    if (res) return res;
    /* perform a global init if needed */
    init_rtapi_data(rtapi_data);
    /* check revision code */
    if (rtapi_data->rev_code != REV_CODE) {
	/* mismatch - release master shared memory block */
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: ERROR: version mismatch %d vs %d\n",
			rtapi_data->rev_code, REV_CODE);
	rtapi_module_master_shared_memory_free();
	return -EINVAL;
    }
    /* set up local pointers to global data */
    module_array = rtapi_data->module_array;
    task_array = rtapi_data->task_array;
    shmem_array = rtapi_data->shmem_array;

    /* perform local init */
    for (n = 0; n <= RTAPI_MAX_TASKS; n++) {
	ostask_array[n] = NULL;
    }
    for (n = 0; n <= RTAPI_MAX_SHMEMS; n++) {
	shmem_addr_array[n] = NULL;
    }
    rtapi_data->timer_running = 0;
    rtapi_data->timer_period = 0;
    max_delay = DEFAULT_MAX_DELAY;

#ifdef RT_LINUX_USE_FPU
    rt_linux_use_fpu(1);
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
    /* on SMP machines, we want to put RT code on the last CPU */
    n = NR_CPUS-1;
    while ( ! cpu_online(n) ) {
	n--;
    }
    rtapi_data->rt_cpu = n;
#else
    /* old kernel, the SMP hooks aren't available, so use CPU 0 */
    rtapi_data->rt_cpu = 0;
#endif


#ifdef CONFIG_PROC_FS
    /* set up /proc/rtapi */
    if (proc_init() != 0) {
	rtapi_print_msg(RTAPI_MSG_WARN,
	    "RTAPI: WARNING: Could not activate /proc entries\n");
	proc_clean();
    }
#endif

#ifdef HAVE_RTAPI_MODULE_INIT_HOOK
    rtapi_module_init_hook();
#endif

    /* done */
    rtapi_print_msg(RTAPI_MSG_INFO, "RTAPI: Init complete\n");
    return 0;
}

/* This cleanup code attempts to fix any messes left by modules
that fail to load properly, or fail to clean up after themselves */

void cleanup_module(void) {
    int n;

    if (rtapi_data == NULL) {
	/* never got inited, nothing to do */
	return;
    }
    /* grab the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    rtapi_print_msg(RTAPI_MSG_INFO, "RTAPI: Exiting\n");

    /* clean up leftover modules (start at 1, we don't use ID 0 */
    for (n = 1; n <= RTAPI_MAX_MODULES; n++) {
	if (module_array[n].state == REALTIME) {
	    rtapi_print_msg(RTAPI_MSG_WARN,
			    "RTAPI: WARNING: module '%s' (ID: %02d) did not "
			    "call rtapi_exit()\n",
			    module_array[n].name, n);
	    module_delete(n);
	}
    }

    /* cleaning up modules should clean up everything, if not there has
       probably been an unrecoverable internal error.... */
    for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
	if (shmem_array[n].rtusers > 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"RTAPI: ERROR: shared memory block %02d not deleted\n", n);
	}
    }
    for (n = 1; n <= RTAPI_MAX_TASKS; n++) {
	if (task_array[n].state != EMPTY) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"RTAPI: ERROR: task %02d not deleted\n", n);
	    /* probably un-recoverable, but try anyway */
	    rtapi_task_pause(n);
	    /* rtapi_task_delete should not grab mutex  */
	    task_array[n].state = DELETE_LOCKED;
	    rtapi_task_delete(n);
	}
    }
    if (rtapi_data->timer_running != 0) {
#ifdef HAVE_RTAPI_MODULE_TIMER_STOP
	rtapi_module_timer_stop();
#endif
	rtapi_data->timer_period = 0;
	timer_counts = 0;
	rtapi_data->timer_running = 0;
	max_delay = DEFAULT_MAX_DELAY;
    }
    rtapi_mutex_give(&(rtapi_data->mutex));
#ifdef CONFIG_PROC_FS
    proc_clean();
#endif

    /* perform thread system-specific module cleanups */
    rtapi_module_cleanup_hook();

    rtapi_print_msg(RTAPI_MSG_INFO, "RTAPI: Exit complete\n");
    return;
}

/***********************************************************************
*                   GENERAL PURPOSE FUNCTIONS                          *
************************************************************************/

/* all RTAPI init is done when the rtapi kernel module
is insmoded.  The rtapi_init() and rtapi_exit() functions
simply register that another module is using the RTAPI.
For other RTOSes, things might be different, especially
if the RTOS does not use modules. */

int _rtapi_init(const char *modname) {
    int n, module_id;
    module_data *module;

    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: initing module %s\n", modname);
    /* get the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    /* find empty spot in module array */
    n = 1;
    while ((n <= RTAPI_MAX_MODULES) && (module_array[n].state != NO_MODULE)) {
	n++;
    }
    if (n > RTAPI_MAX_MODULES) {
	/* no room */
	rtapi_mutex_give(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: reached module limit %d\n",
	    n);
	return -EMFILE;
    }
    /* we have space for the module */
    module_id = n;
    module = &(module_array[n]);
    /* update module data */
    module->state = REALTIME;
    if (modname != NULL) {
	/* use name supplied by caller, truncating if needed */
	rtapi_snprintf(module->name, RTAPI_NAME_LEN, "%s", modname);
    } else {
	/* make up a name */
	rtapi_snprintf(module->name, RTAPI_NAME_LEN, "RTMOD%03d", module_id);
    }
    rtapi_data->rt_module_count++;
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: module '%s' loaded, ID: %d\n",
	module->name, module_id);
    rtapi_mutex_give(&(rtapi_data->mutex));
    return module_id;
}

int _rtapi_exit(int module_id) {
    int retval;

    rtapi_mutex_get(&(rtapi_data->mutex));
    retval = module_delete(module_id);
    rtapi_mutex_give(&(rtapi_data->mutex));
    return retval;
}

static int module_delete(int module_id) {
    module_data *module;
    char name[RTAPI_NAME_LEN + 1];
    int n;

    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: module %d exiting\n", module_id);
    /* validate module ID */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	return -EINVAL;
    }
    /* point to the module's data */
    module = &(module_array[module_id]);
    /* check module status */
    if (module->state != REALTIME) {
	/* not an active realtime module */
	return -EINVAL;
    }
    /* clean up any mess left behind by the module */
    for (n = 1; n <= RTAPI_MAX_TASKS; n++) {
	if ((task_array[n].state != EMPTY)
	    && (task_array[n].owner == module_id)) {
	    rtapi_print_msg(RTAPI_MSG_WARN,
		"RTAPI: WARNING: module '%s' failed to delete task %02d\n",
		module->name, n);
	    task_array[n].state = DELETE_LOCKED;
	    rtapi_task_delete(n);
	}
    }
    for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
	if (test_bit(module_id, shmem_array[n].bitmap)) {
	    rtapi_print_msg(RTAPI_MSG_WARN,
		"RTAPI: WARNING: module '%s' failed to delete shmem %02d\n",
		module->name, n);
	    // mark block as ready for delete, lock already held
	    shmem_array[n].magic = SHMEM_MAGIC_DEL_LOCKED;
	    rtapi_shmem_delete(n, module_id);
	}
    }
 
    rtapi_snprintf(name, RTAPI_NAME_LEN, "%s", module->name);
    /* update module data */
    module->state = NO_MODULE;
    module->name[0] = '\0';
    rtapi_data->rt_module_count--;
    if (rtapi_data->rt_module_count == 0) {
	if (rtapi_data->timer_running != 0) {
#ifdef HAVE_RTAPI_MODULE_TIMER_STOP
	    rtapi_module_timer_stop();
#endif
	    rtapi_data->timer_period = 0;
	    timer_counts = 0;
	    max_delay = DEFAULT_MAX_DELAY;
	    rtapi_data->timer_running = 0;
	}
    }
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: module %d exited, name: '%s'\n",
	module_id, name);
    return 0;
}

#else /* ULAPI */

extern rtapi_data_t *rtapi_init_hook();

int _rtapi_init(const char *modname) {
    int n, module_id;
    module_data *module;

    /* say hello */
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: initing module %s\n", modname);
    /* get shared memory block from OS and save its address */
    errno = 0;

    if ((rtapi_data = rtapi_init_hook()) == NULL)
	return -ENOMEM;

    /* perform a global init if needed */
    init_rtapi_data(rtapi_data);
    /* check revision code */
    if (rtapi_data->rev_code != REV_CODE) {
	/* mismatch - release master shared memory block */
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: ERROR: version mismatch %d vs %d\n",
			rtapi_data->rev_code, REV_CODE);
	return -EINVAL;
    }
    /* set up local pointers to global data */
    module_array = rtapi_data->module_array;
    task_array = rtapi_data->task_array;
    shmem_array = rtapi_data->shmem_array;
    /* perform local init */
    for (n = 0; n <= RTAPI_MAX_SHMEMS; n++) {
	shmem_addr_array[n] = NULL;
    }

    /* get the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    /* find empty spot in module array */
    n = 1;
    while ((n <= RTAPI_MAX_MODULES) && (module_array[n].state != NO_MODULE)) {
	n++;
    }
    if (n > RTAPI_MAX_MODULES) {
	/* no room */
	rtapi_mutex_give(&(rtapi_data->mutex));
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: ERROR: reached module limit %d\n", n);
	return -EMFILE;
    }
    /* we have space for the module */
    module_id = n;
    module = &(module_array[n]);
    /* update module data */
    module->state = USERSPACE;
    if (modname != NULL) {
	/* use name supplied by caller, truncating if needed */
	rtapi_snprintf(module->name, RTAPI_NAME_LEN, "%s", modname);
    } else {
	/* make up a name */
	rtapi_snprintf(module->name, RTAPI_NAME_LEN, "ULMOD%03d", module_id);
    }
    rtapi_data->ul_module_count++;
    rtapi_mutex_give(&(rtapi_data->mutex));
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: module '%s' inited, ID = %02d\n",
		    module->name, module_id);
    return module_id;
}

int _rtapi_exit(int module_id) {
    module_data *module;
    int n;

    if (rtapi_data == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: ERROR: exit called before init\n");
	return -EINVAL;
    }
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: module %02d exiting\n", module_id);
    /* validate module ID */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: bad module id\n");
	return -EINVAL;
    }
    /* get mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    /* point to the module's data */
    module = &(module_array[module_id]);
    /* check module status */
    if (module->state != USERSPACE) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI: ERROR: not a userspace module\n");
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    /* clean up any mess left behind by the module */
    for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
	if (test_bit(module_id, shmem_array[n].bitmap)) {
	    rtapi_print_msg(RTAPI_MSG_WARN,
			    "ULAPI: WARNING: module '%s' failed to delete "
			    "shmem %02d\n", module->name, n);
	    // mark block as ready for delete, lock already held
	    shmem_array[n].magic = SHMEM_MAGIC_DEL_LOCKED;
	    rtapi_shmem_delete(n, module_id);
	}
    }
    /* update module data */
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: module %02d exited, name = '%s'\n",
		    module_id, module->name);
    module->state = NO_MODULE;
    module->name[0] = '\0';
    rtapi_data->ul_module_count--;
    rtapi_mutex_give(&(rtapi_data->mutex));
    return 0;
}


#endif  /* ULAPI */


#ifdef MODULE
EXPORT_SYMBOL(_rtapi_init);
EXPORT_SYMBOL(_rtapi_exit);
#endif
