#include "config.h"	

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>		/* replaces malloc.h in recent kernels */
#include <linux/ctype.h>	/* isdigit */
#include <linux/delay.h>	/* udelay */
#include <asm/uaccess.h>	/* copy_from_user() */
#include <asm/msr.h>		/* rdtscll() */

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
#include <linux/cpumask.h>	/* NR_CPUS, cpu_online() */
#endif

#include <native/heap.h>
#include <native/task.h>
#include <native/intr.h>
#include  <rtdk.h>

#include "rtapi.h"		/* public RTAPI decls */
#include "rtapi_common.h"	/* shared realtime/nonrealtime stuff */

static RT_HEAP shmem_heap_array[RTAPI_MAX_SHMEMS + 1];        
void *shmem_addr_array[RTAPI_MAX_SHMEMS + 1];

/* module parameters */

#include "rtapi_proc.h"		/* proc filesystem decls & code */

/* the following are internal functions that do the real work associated
   with deleting tasks, etc.  They do not check the mutex that protects
   the internal data structures.  When someone calls an rtapi_xxx_delete()
   function, the rtapi funct gets the mutex before calling one of these
   internal functions.  When internal code that already has the mutex
   needs to delete something, it calls these functions directly.
*/
static int shmem_delete(int shmem_id, int module_id);

/***********************************************************************
*                  SHARED MEMORY RELATED FUNCTIONS                     *
************************************************************************/

int rtapi_shmem_new(int key, int module_id, unsigned long int size)
{
    int n;
    int shmem_id;
    shmem_data *shmem;
    char shm_name[20];

    /* key must be non-zero, and also cannot match the key that RTAPI uses */
    if ((key == 0) || (key == RTAPI_KEY)) {
	return -EINVAL;
    }
    /* get the mutex */
    rtapi_mutex_get(&(rtapi_data->mutex));
    /* validate module_id */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }
    if (module_array[module_id].state != REALTIME) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EINVAL;
    }

    /* check if a block is already open for this key */
    for (n = 1; n <= RTAPI_MAX_SHMEMS; n++) {
	if (shmem_array[n].key == key) {
	    /* found a match */
	    shmem_id = n;
	    shmem = &(shmem_array[n]);
	    /* is it big enough? */
	    if (shmem->size < size) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -EINVAL;
	    }
	    /* yes, has it been mapped into kernel space? */
	    if (shmem->rtusers == 0) {
		/* no, map it and save the address */

		rtapi_print_msg(RTAPI_MSG_ERR, 
				"RTAPI: UNSUPPORTED OPERATION - cannot map user segment %d into kernel\n",n);
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -ENOMEM;
	    }
	    /* is this module already using it? */
	    if (test_bit(module_id, shmem->bitmap)) {
		rtapi_mutex_give(&(rtapi_data->mutex));
		return -EINVAL;
	    }
	    /* update usage data */
	    set_bit(module_id, shmem->bitmap);
	    shmem->rtusers++;
	    /* announce another user for this shmem */
	    rtapi_print_msg(RTAPI_MSG_DBG,
		"RTAPI: shmem %02d opened by module %02d\n",
		shmem_id, module_id);
	    rtapi_mutex_give(&(rtapi_data->mutex));
	    return shmem_id;
	}
    }
    /* find empty spot in shmem array */
    n = 1;
    while ((n <= RTAPI_MAX_SHMEMS) && (shmem_array[n].key != 0)) {
	n++;
    }
    if (n > RTAPI_MAX_SHMEMS) {
	/* no room */
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -EMFILE;
    }
    /* we have space for the block data */
    shmem_id = n;
    shmem = &(shmem_array[n]);

    /* get shared memory block from OS and save its address */

    snprintf(shm_name, sizeof(shm_name), "shm-%d", shmem_id);
    if ((n = rt_heap_create(&shmem_heap_array[shmem_id], shm_name, 
			    size, H_SHARED)) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: rt_heap_create() returns %d\n", n);
	return -EINVAL;
    }
    if ((n = rt_heap_alloc(&shmem_heap_array[shmem_id], 0, TM_INFINITE , 
			   (void **)&shmem_addr_array[shmem_id])) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ERROR: rt_heap_alloc() returns %d\n", n);
	return -EINVAL;
    }

    if (shmem_addr_array[shmem_id] == NULL) {
	rtapi_mutex_give(&(rtapi_data->mutex));
	return -ENOMEM;
    }
    /* the block has been created, update data */
    set_bit(module_id, shmem->bitmap);
    shmem->key = key;
    shmem->rtusers = 1;
    shmem->ulusers = 0;
    shmem->size = size;
    rtapi_data->shmem_count++;
    /* zero the first word of the shmem area */
    *((long int *) (shmem_addr_array[shmem_id])) = 0;
    /* announce the birth of a brand new baby shmem */
    rtapi_print_msg(RTAPI_MSG_DBG,
	"RTAPI: shmem %02d created by module %02d, key: %d, size: %lu\n",
	shmem_id, module_id, key, size);

    /* and return the ID to the proud parent */
    rtapi_mutex_give(&(rtapi_data->mutex));
    return shmem_id;
}

int rtapi_shmem_delete(int shmem_id, int module_id)
{
    int retval;

    rtapi_mutex_get(&(rtapi_data->mutex));
    retval = shmem_delete(shmem_id, module_id);
    rtapi_mutex_give(&(rtapi_data->mutex));
    return retval;
}

static int shmem_delete(int shmem_id, int module_id)
{
    shmem_data *shmem;

    /* validate shmem ID */
    if ((shmem_id < 1) || (shmem_id > RTAPI_MAX_SHMEMS)) {
	return -EINVAL;
    }
    /* point to the shmem's data */
    shmem = &(shmem_array[shmem_id]);
    /* is the block valid? */
    if (shmem->key == 0) {
	return -EINVAL;
    }
    /* validate module_id */
    if ((module_id < 1) || (module_id > RTAPI_MAX_MODULES)) {
	return -EINVAL;
    }
    if (module_array[module_id].state != REALTIME) {
	return -EINVAL;
    }
    /* is this module using the block? */
    if (test_bit(module_id, shmem->bitmap) == 0) {
	return -EINVAL;
    }
    /* OK, we're no longer using it */
    clear_bit(module_id, shmem->bitmap);
    shmem->rtusers--;
    /* is somebody else still using the block? */
    if (shmem->rtusers > 0) {
	/* yes, we're done for now */
	rtapi_print_msg(RTAPI_MSG_DBG,
	    "RTAPI: shmem %02d closed by module %02d\n", shmem_id, module_id);
	return 0;
    }
    /* no other realtime users, free the shared memory from kernel space */

    rt_heap_delete(&shmem_heap_array[shmem_id]);

    shmem_addr_array[shmem_id] = NULL;
    shmem->rtusers = 0;
    /* are any user processes using the block? */
    if (shmem->ulusers > 0) {
	/* yes, we're done for now */
	rtapi_print_msg(RTAPI_MSG_DBG,
	    "RTAPI: shmem %02d unmapped by module %02d\n", shmem_id,
	    module_id);
	return 0;
    }
    /* no other users at all, this ID is now free */
    /* update the data array and usage count */
    shmem->key = 0;
    shmem->size = 0;
    rtapi_data->shmem_count--;
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: shmem %02d freed by module %02d\n",
	shmem_id, module_id);
    return 0;
}

int rtapi_shmem_getptr(int shmem_id, void **ptr)
{
    /* validate shmem ID */
    if ((shmem_id < 1) || (shmem_id > RTAPI_MAX_SHMEMS)) {
	return -EINVAL;
    }
    /* is the block mapped? */
    if (shmem_addr_array[shmem_id] == NULL) {
	return -EINVAL;
    }
    /* pass memory address back to caller */
    *ptr = shmem_addr_array[shmem_id];
    return 0;
}


/* starting with kernel 2.6, symbols that are used by other modules
   _must_ be explicitly exported.  2.4 and earlier kernels exported
   all non-static global symbols by default, so these explicit exports
   were not needed.  For 2.4 and older, you should define EXPORT_SYMTAB
   (before including module.h) to make these explicit exports work and 
   minimize pollution of the kernel namespace.  But EXPORT_SYMTAB
   must not be defined for 2.6, so the best place to do it is 
   probably in the makefiles somewhere (as a -D option to gcc).
*/

EXPORT_SYMBOL(rtapi_shmem_new);
EXPORT_SYMBOL(rtapi_shmem_delete);
EXPORT_SYMBOL(rtapi_shmem_getptr);
