
#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_global.h"       /* global_data_t */
//#include "rtapi_common.h"

//#define XENO 0
//#include <sched.h>		// sched_get_priority_*()
//#include <pthread.h>		/* pthread_* */

#ifdef XENO
#include <native/heap.h>		// RT_HEAP, H_SHARED, rt_heap_*
#include <native/task.h>		// RT_TASK, rt_task_*()
static RT_HEAP ul_global_heap_desc;

#else

//#include <rtai.h>
//#include <rtai_sched.h>
#include <rtai_shm.h>
#endif



int main(int argc,char **argv)
{

    return 0;
}
