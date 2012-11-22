/********************************************************************
* Description:  xenomai-user.c
*               This file, 'xenomai-user.c', implements the unique
*               functions for the Xenomai userland thread system.
********************************************************************/

#include "config.h"
#include "rtapi.h"

unsigned int rev_code = 3;

/* RTAPI task-related functions */

#include <native/task.h>        /* Xenomai task */
#include <native/timer.h>
#include <native/mutex.h>
#include <rtdk.h>
#include <nucleus/types.h>     /* for XNOBJECT_NAME_LEN */

#include <sys/io.h>             /* inb, outb */

RT_TASK ostask_array[RTAPI_MAX_TASKS + 1];

// this is needed due to the weirdness of the rt_task_self return value -
// it does _not_ match the address of the RT_TASK structure it was 
// created with
RT_TASK *ostask_self[RTAPI_MAX_TASKS + 1];

int rtapi_task_delete_hook(task_data *task) {
    int retval;

    if ((retval = rt_task_delete( &ostask_array[id] )) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,"ERROR: rt_task_delete() = %d %s\n", 
			retval, strerror(retval));
	return retval;
    }
    task->magic = 0;
    return 0;
}

void rtapi_task_start_hook(task_data *task) {
    int which_cpu = 0;
}

void rtapi_task_create_hook(task_data *task, int task_id) {
    int retval;

#if !defined(BROKEN_XENOMAU_CPU_AFFINITY)
    // seems to work for me
    // not sure T_CPU(n) is possible - see:
    // http://www.xenomai.org/pipermail/xenomai-help/2010-09/msg00081.html

    if (task->cpu > -1)  // explicitly set by threads, motmod
	which_cpu = T_CPU(task->cpu);
#endif

    // http://www.xenomai.org/documentation/trunk/html/api/group__task.html#ga03387550693c21d0223f739570ccd992
    // Passing T_FPU|T_CPU(1) in the mode parameter thus creates a
    // task with FPU support enabled and which will be affine to CPU #1
    // the task will start out dormant; execution begins with rt_task_start()

    // since this is a usermode RT task, it will be FP anyway
    if ((retval = rt_task_create (&ostask_array[task_id], task->name, 
				  task->stacksize, task->prio, 
				  (task->uses_fp ? T_FPU : 0) | which_cpu )
	 ) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"rt_task_create failed, rc = %d\n", retval );
	return -ENOMEM;
    }

    if (retval = rt_task_start( &ostask_array[task_id],
				wrapper, (void*)task_id)) {
	rtapi_print_msg(RTAPI_MSG_INFO,
			"rt_task_start failed, rc = %d\n", retval );
	return -ENOMEM;
    }
    return 0;
}

void rtapi_task_stop_hook(task_data *task) {
    int retval;

    if ((retval = rt_task_delete( &ostask_array[task_id] )) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,"rt_task_delete() = %d\n", retval);
	return retval;
    }

    return 0;
}

int rtapi_task_pause_hook(task_data *task) {
    return rt_task_suspend( task->ostask );
}

int rtapi_task_resume_hook(task_data *task) {
    return rt_task_resume( task->ostask );
}


void rtapi_outb_hook(unsigned char byte, unsigned int port) {
    outb(byte,port);
}

void rtapi_inb_hook(unsigned int port) {
    return outb(port);
}


