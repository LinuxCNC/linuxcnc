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

#include "config.h"		// build configuration
#include "rtapi.h"		// these functions
#include "rtapi_common.h"	// RTAPI macros and decls
#include "rtapi/shmdrv/shmdrv.h"


#ifdef MODULE
#include "rtapi_proc.h"		/* proc filesystem decls & code */

#  if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
#    include <linux/cpumask.h>	/* NR_CPUS, cpu_online() */
#  endif
#endif

extern void init_rtapi_data(rtapi_data_t * data);

#ifdef RTAPI
MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("RTAPI module support - kernel threads");
MODULE_LICENSE("GPL");

// kernel styles do not support multiple instances
// this just numbers this particular instance to fit with the scheme
int rtapi_instance;

RTAPI_MP_INT(rtapi_instance, "RTAPI instance id");
EXPORT_SYMBOL(rtapi_instance);

global_data_t *global_data = NULL;
EXPORT_SYMBOL(global_data);

rtapi_switch_t *rtapi_switch  = NULL;
EXPORT_SYMBOL(rtapi_switch);

ringbuffer_t rtapi_message_buffer;   // error ring access strcuture

/* the following are internal functions that do the real work associated
   with deleting tasks, etc.  They do not check the mutex that protects
   the internal data structures.  When someone calls an rtapi_xxx_delete()
   function, the rtapi funct gets the mutex before calling one of these
   internal functions.  When internal code that already has the mutex
   needs to delete something, it calls these functions directly.
*/
static int module_delete(int module_id);
extern void _rtapi_module_cleanup_hook(void);

/***********************************************************************
*                   INIT AND SHUTDOWN FUNCTIONS                        *
************************************************************************/

#ifdef HAVE_RTAPI_MODULE_INIT_HOOK
void _rtapi_module_init_hook(void);
#endif

#ifdef HAVE_RTAPI_MODULE_EXIT_HOOK
void _rtapi_module_exit_hook(void);
#endif

int init_module(void) {
    int n;
    struct shm_status sm;
    int retval;

    rtapi_switch = rtapi_get_handle();

    // first thing: attach global_data
    sm.key = OS_KEY(GLOBAL_KEY, rtapi_instance);
    sm.size = 0;
    sm.flags = 0;

    if ((retval = shmdrv_attach(&sm, (void **)&global_data)) < 0) {
	// cant use the  message ringbuffer just yet
	printk("RTAPI ERROR: can attach global segment: %d",
	       retval);
	return -EINVAL;
    }

    // fail immediately if the global segment isnt in shape yet
    // this catches https://github.com/zultron/linuxcnc/issues/49 early
    if (global_data->magic != GLOBAL_READY) {

	printk("RTAPI ERROR: bad global magic: 0x%x",
	       global_data->magic);

	// TBD: remove once cause identified
	printk("halsize=%d\n", global_data->hal_size);
	printk("msgd pid=%d\n", global_data->rtapi_msgd_pid);
	printk("magic=%x\n", global_data->magic);
	printk("flavor=%d\n", global_data->rtapi_thread_flavor);
	// fail the insmod
	return -EINVAL;
    }

    // say hello - this now goes through the message ringbuffer
    RTAPIINFO("%s %s init",
	      rtapi_get_handle()->thread_flavor_name,
	      GIT_VERSION);

    sm.key = OS_KEY(RTAPI_KEY, rtapi_instance);
    sm.size = sizeof(rtapi_data_t);
    sm.flags = 0;
    if ((retval = shmdrv_create(&sm)) < 0) {
	RTAPIERR("can create rtapi segment: %d",  retval);
	return -EINVAL;
    }
    if ((retval = shmdrv_attach(&sm, (void **)&rtapi_data)) < 0) {
	RTAPIERR("cant attach rtapi segment: %d", retval);
	return -EINVAL;
    }

    // make error ringbuffer accessible within RTAPI
    ringbuffer_init(&global_data->rtapi_messages, &rtapi_message_buffer);
    global_data->rtapi_messages.refcount += 1;   // rtapi is 'attached'

    // tag messages originating from RT proper
    rtapi_set_logtag("rt");

    /* this will take care of any threads flavor hook */
    init_rtapi_data(rtapi_data);

    /* check flavor and serial codes */
    if ((rtapi_data->thread_flavor_id != THREAD_FLAVOR_ID) ||
	(rtapi_data->serial != RTAPI_SERIAL)) {

	if (rtapi_data->thread_flavor_id != THREAD_FLAVOR_ID)
	    RTAPIERR("flavor mismatch %d vs %d",
		     rtapi_data->thread_flavor_id,
		     THREAD_FLAVOR_ID);

	if (rtapi_data->serial != RTAPI_SERIAL)
	    RTAPIERR("serial mismatch '%d' vs '%d'",
		     rtapi_data->serial, RTAPI_SERIAL);

	// release rtapi and global shared memory blocks
	sm.key = OS_KEY(RTAPI_KEY, rtapi_instance);
	sm.size = sizeof(global_data_t);
	sm.flags = 0;
	if ((retval = shmdrv_detach(&sm)) < 0)
	    RTAPIERR("shmdrv_detach(rtapi=0x%x,%zu) returns %d",
		     sm.key, sm.size, retval);

	sm.key = OS_KEY(GLOBAL_KEY, rtapi_instance);
	sm.size = sizeof(global_data_t);
	sm.flags = 0;
	if ((retval = shmdrv_detach(&sm)) < 0)
	    RTAPIERR("shmdrv_detach(global=0x%x,%zu) returns %d",
		     sm.key, sm.size, retval);

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
	RTAPIWARN("Could not activate /proc entries");
	proc_clean();
    }
#endif

#ifdef HAVE_RTAPI_MODULE_INIT_HOOK
    _rtapi_module_init_hook();
#endif

    RTAPIINFO("Init complete");
    return 0;
}

/* This cleanup code attempts to fix any messes left by modules
that fail to load properly, or fail to clean up after themselves */

void cleanup_module(void) {
    int n;
    struct shm_status sm;
    int retval;

    if (rtapi_data == NULL) {
	/* never got inited, nothing to do */
	return;
    }

#ifdef HAVE_RTAPI_MODULE_EXIT_HOOK
    _rtapi_module_exit_hook();
#endif

    /* grab the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    RTAPIINFO("exit");

    /* clean up leftover modules (start at 1, we don't use ID 0 */
    for (n = 1; n <= RTAPI_MAX_MODULES; n++) {
	if (module_array[n].state == REALTIME) {
	    RTAPIWARN("module '%s' (ID: %02d) did not "
		      "call rtapi_exit()",
		      module_array[n].name, n);
	    module_delete(n);
	}
    }

    /* cleaning up modules should clean up everything, if not there has
       probably been an unrecoverable internal error.... */
    for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
	if (shmem_array[n].rtusers > 0) {
	    RTAPIERR("shared memory block %02d not deleted, "
		     "%d RT users left", n,
		     shmem_array[n].rtusers );
	}
    }
    for (n = 1; n <= RTAPI_MAX_TASKS; n++) {
	if (task_array[n].state != EMPTY) {
	    RTAPIERR("task %02d not deleted", n);
	    /* probably un-recoverable, but try anyway */
	    _rtapi_task_pause(n);
	    /* rtapi_task_delete should not grab mutex  */
	    task_array[n].state = DELETE_LOCKED;
	    _rtapi_task_delete(n);
	}
    }
    if (rtapi_data->timer_running != 0) {
#ifdef HAVE_RTAPI_MODULE_TIMER_STOP
	_rtapi_module_timer_stop();
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

    sm.key = OS_KEY(RTAPI_KEY, rtapi_instance);
    sm.size = sizeof(rtapi_data_t);
    sm.flags = 0;
    if ((retval = shmdrv_detach(&sm)) < 0) {
	RTAPIERR("shmdrv_detach(rtapi) returns %d",
		 retval);
    }
    rtapi_data = NULL;

    global_data->rtapi_messages.refcount -= 1;   // detach rtapi end

    sm.key = OS_KEY(GLOBAL_KEY, rtapi_instance);
    sm.size = sizeof(global_data_t);
    sm.flags = 0;
    if ((retval = shmdrv_detach(&sm)) < 0) {
	RTAPIERR("shmdrv_detach(global) returns %d",
		 retval);
    }
    global_data = NULL;

    RTAPIINFO("Exit complete");
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

    RTAPIDBG("initing module %s",  modname);

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
	RTAPIERR("'%s': reached module limit %d", modname, n);
	return -EMFILE;
    }

    /* we have space for the module */
    module_id = n;
    module = &(module_array[n]);
    /* update module data */
    module->state = REALTIME;
    if (modname != NULL) {
	/* use name supplied by caller, truncating if needed */
	rtapi_snprintf(module->name, RTAPI_NAME_LEN,
		       "%s", modname);
    } else {
	/* make up a name */
	rtapi_snprintf(module->name, RTAPI_NAME_LEN,
		       "RTMOD%03d", module_id);
    }
    rtapi_data->rt_module_count++;
    RTAPIDBG("module '%s' loaded, ID: %d",
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

    RTAPIDBG("module %d exiting", module_id);

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
	    RTAPIWARN("module '%s' failed to delete task %02d",
		      module->name, n);
	    task_array[n].state = DELETE_LOCKED;
	    _rtapi_task_delete(n);
	}
    }
    for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
	if (rtapi_test_bit(module_id, shmem_array[n].bitmap)) {

	    RTAPIWARN("module '%s' failed to delete shmem %02d rt=%d ul=%d",
		      module->name, n,
		      shmem_array[n].rtusers,
		      shmem_array[n].ulusers);

	    // mark block as ready for delete, lock already held
	    shmem_array[n].magic = SHMEM_MAGIC_DEL_LOCKED;
	    _rtapi_shmem_delete(n, module_id);
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
	    _rtapi_module_timer_stop();
#endif
	    rtapi_data->timer_period = 0;
	    timer_counts = 0;
	    max_delay = DEFAULT_MAX_DELAY;
	    rtapi_data->timer_running = 0;
	}
    }
    RTAPIDBG("module %d exited, name: '%s'",
	     module_id, name);
    return 0;
}

#else /* ULAPI */

int _rtapi_init(const char *modname) {
    int n, module_id;
    module_data *module;
    struct shm_status sm;
    int retval;

    ULAPIDBG("initing module '%s'", modname);

    errno = 0;

    // if not done yet, attach global and rtapi_data segments now
    if (global_data == NULL) {
	sm.key = OS_KEY(GLOBAL_KEY, rtapi_instance);
	sm.size = sizeof(global_data_t);
	sm.flags = 0;
	if ((retval = shmdrv_attach(&sm, (void **)&global_data)) < 0) {
	    ULAPIERR(" '%s' - cant attach global segment: %d",
		     modname, retval);
	    return -EINVAL;
	}
	sm.key = OS_KEY(RTAPI_KEY, rtapi_instance);
	sm.size = sizeof(rtapi_data_t);
	sm.flags = 0;
	if ((retval = shmdrv_attach(&sm, (void **)&rtapi_data)) < 0) {
	    ULAPIERR(" '%s' - cant attach rtapi segment: %d",
		     modname, retval);
	    return -EINVAL;
	}
    }
    // I consider this very dubious - there is no reason for ULAPI to start without
    // rtapi_data already being inited: -mah
    // init_rtapi_data(rtapi_data);

    /* check flavor and serial codes */
    if (rtapi_data->thread_flavor_id != THREAD_FLAVOR_ID) {
	/* mismatch - release master shared memory block */
	ULAPIERR(" '%s' -  flavor mismatch %d vs %d",
		 modname, rtapi_data->thread_flavor_id,
		 THREAD_FLAVOR_ID);
	return -EINVAL;
    }
    if (rtapi_data->serial != RTAPI_SERIAL) {
	/* mismatch - release master shared memory block */

	ULAPIERR("serial mismatch %d vs %d",
		 rtapi_data->serial, RTAPI_SERIAL);
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
	ULAPIERR("reached module limit %d",n);
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
    ULAPIDBG("module '%s' inited, ID = %02d",
	     module->name, module_id);
    return module_id;
}

int _rtapi_exit(int module_id) {
    module_data *module;
    int n;

    if (rtapi_data == NULL) {
	ULAPIERR("exit called before init");
	return -EINVAL;
    }

    ULAPIDBG("module %02d exiting", module_id);

    /* validate module ID */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	ULAPIERR("bad module id %d", module_id);
	return -EINVAL;
    }

    /* get mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));

    /* point to the module's data */
    module = &(module_array[module_id]);
    /* check module status */
    if (module->state != USERSPACE) {
	ULAPIERR("not a userspace module: %d", module_id);
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    /* clean up any mess left behind by the module */
    for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
	if (rtapi_test_bit(module_id, shmem_array[n].bitmap)) {
	    ULAPIWARN("module '%s' failed to delete "
		      "shmem %02d", module->name, n);
	    // mark block as ready for delete, lock already held
	    shmem_array[n].magic = SHMEM_MAGIC_DEL_LOCKED;
	    _rtapi_shmem_delete(n, module_id);
	}
    }
    /* update module data */
    ULAPIDBG("module %02d exited, name = '%s'",
	     module_id, module->name);
    module->state = NO_MODULE;
    module->name[0] = '\0';
    rtapi_data->ul_module_count--;
    rtapi_mutex_give(&(rtapi_data->mutex));
    return 0;
}


#endif  /* ULAPI */

