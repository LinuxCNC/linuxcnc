/*
 * This module is used to test the embshmem driver.  It's a separate driver that 
 * grabs a pointer to the shared memory, and then periodically
 * watches for (and responds to) changes in the shared memory.
 *
 * Copyright (c) Embrisk Ltd 2012.
 * This is public domain software with no warranty.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timer.h>

#include "shmdrv.h"
#include "testshmdrv.h"

static int key = 1234;
module_param(key, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(key, "segment id to attach to, or create; default 1234");

static int size = 23423;
module_param(size, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(size, "segment size");


#define TICK_TIME_MS 100

static struct timer_list testTimer;
bool exiting = false;
struct workqueue_struct *workqueue;
struct work_struct work;
struct ShmemStruct *shmem;

/*
 * Where we periodically do some work - we check the shared memory for a changed value,
 * and if so change another value.
 */
static void smemtest_work(struct work_struct *work)
{
    static int lastB = 0;

    if (lastB != shmem->countB) {
        lastB = shmem->countB;
        shmem->countC = lastB + 1;
    }
}

/*
 * Timer tick call-back which we use to schedule our work.
 */
void smemtest_timer_callback( unsigned long data )
{
    int ret;
    queue_work(workqueue, &work);
    ret = mod_timer(&testTimer, jiffies + msecs_to_jiffies(TICK_TIME_MS));
    if (ret) {
        printk(KERN_CRIT "%s: failed to modify timer (%d)\n", __func__, ret);
    }
}

static int smemtest_init(void) 
{
    int ret;
    struct shm_status sm;

    printk(KERN_ERR "%s: module initialising\n", __func__);

    sm.key = key;
    ret = shmdrv_status(&sm);
    if (ret) { // segment doesnt exist
	sm.size = size;
	ret = shmdrv_create(&sm);
	if (ret < 0) {
	    err("cant create segment key=%d/%x", sm.key, sm.key);
	    return ret;
	}
	// created ok
    }
    // shm segment exists, attach
    ret = shmdrv_attach(&sm, (void **) &shmem);
    if (ret) {
	err("cant attach to segment key=%d/%x", sm.key, sm.key);
	return ret;
    }

    setup_timer(&testTimer, smemtest_timer_callback, 0);
    ret = mod_timer(&testTimer, jiffies + msecs_to_jiffies(TICK_TIME_MS));
    if (ret) {
        printk(KERN_CRIT "%s: failed to modify timer\n", __func__);
        return(ret);
    }

    workqueue = create_workqueue("test_embshmem");
    if (!workqueue) {
        printk(KERN_CRIT "%s: failed to create workqueue\n", __func__);
        return(-EIO);
    }

    INIT_WORK(&work, smemtest_work);

    info("module init ok");

    return(ret);
}

static void smemtest_exit(void) 
{
    struct shm_status sm;
    int ret;

    exiting = true;
    flush_workqueue(workqueue);
    destroy_workqueue(workqueue);
    del_timer_sync(&testTimer);

    if (shmem) {
	sm.key = key;
	ret = shmdrv_detach(&sm);
	if (ret) {
	    err("cant detach segment key=%d/%x", sm.key, sm.key);
	}
    }
    info("module exiting");
}

MODULE_LICENSE("GPL");
module_init(smemtest_init);
module_exit(smemtest_exit);

