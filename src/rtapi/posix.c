/********************************************************************
* Description:  posix.c
*               This file, 'posix.c', implements the unique
*               functions for the POSIX userland thread system.
********************************************************************/

#include "config.h"
#include "rtapi.h"

/***********************************************************************
*                           TASK FUNCTIONS                             *
************************************************************************/
#include <pth.h>		/* pth_uctx_* */

static struct timeval schedule;
static int base_periods;
static pth_uctx_t main_ctx, this_ctx;

pth_uctx_t ostask_array[RTAPI_MAX_TASKS + 1]; /* thread's context */

int rtapi_task_new_hook(task_data *task, int n) {
    task->thread_extra.ostask = NULL;
}


int rtapi_task_delete_hook(task_data *task) {

    pth_uctx_destroy(task->thread_extra.ostask);

}

void rtapi_task_create_hook(task_data *task, int task_id) {
    int retval;

    retval = pth_uctx_create(&ostask_array[task_id]);
    if (retval == FALSE)
	return -ENOMEM;
    retval = pth_uctx_make(ostask_array[task_id], NULL, task->stacksize, NULL,
			   rtapi_task_wrapper, (void*)task_id, 0);
    if (retval == FALSE)
	return -ENOMEM;

    return 0;
}


int rtapi_task_stop_hook(task_data *task) {
    pth_uctx_destroy(ostask_array[task_id]);
}


int rtapi_wait_hook(task_data *task) {
    pth_uctx_switch(this_ctx, main_ctx);
    return 0;
}
