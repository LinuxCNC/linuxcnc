/*
 * This driver provides a single shared memory area that can be accessed from user space and
 * from kernel space.
 *
 * Copyright (c) Embrisk Ltd 2012.
 * This is public domain software with no warranty.
 *
 * adapted for LinuxCNC Michael Haberler 4/2013
 */

#define DEBUG

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>

#include "rtapishm.h"

static bool debug = false;
module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "enable debug info (default: false)");

static int count = 10;
module_param(count, int, S_IRUGO);
MODULE_PARM_DESC(count, "number of rtapi shm devices to create");

/*
 * Device related data.
 */
static int major;
static struct class *class_rtapishm;

struct rtapishm_dev {
    struct cdev cdev;
    struct device *rtapishm_device;
};
//static struct device *dev_rtapishm;

static struct rtapishm_dev *rtapishm_devices;

/*
 * allocArea is the memory block that we allocate, of which memArea points to the
 * bit we actually use.
 */
static void *allocArea;
static void *memArea;
static int memLength;
static spinlock_t rtapishm_lock;

/*
 * Allocate the shared memory.  This is called directly by other kernel routines that access
 * the shared memory, and indirectly by user space via rtapishm_mmap.
 */
extern int rtapishm_get_memory(int length, void **pointer)
{
    int i;

    /*
     * If the shared memory hasn't already been allocated, then allocate it.
     */
    if (!memArea) {
        void *newAllocArea;
        void *newMemArea;
        unsigned long irqState;
        int memSize;

        /*
         * For the memory mapping, we'll need to allocate a whole number of pages, so we'll need to
         * round the size up.  Then we need it to start on a page boundary, and as we don't know
         * where kmalloc will put things, we need to add almost a whole extra page so that we can
         * be sure we can find an area of the correct length somewhere in the page.
         */
        length = (length + PAGE_SIZE + 1) & PAGE_MASK;
        memSize = length + PAGE_SIZE - 1;
        newAllocArea = kmalloc(memSize, GFP_KERNEL);
        if (!newAllocArea) {
            err( "%s: Couldn't allocate memory to share", __func__);
            return(-ENOMEM);
        }

        /*
         * memArea is the page aligned area we actually use.
         */
        newMemArea = (void *)(((unsigned long)newAllocArea + PAGE_SIZE - 1) & PAGE_MASK);
        memset(newMemArea, 0, length);
        info("%s: allocArea %p, memArea %p, size %x", __func__, newAllocArea, 
                newMemArea, memSize);

        /*
         * Mark the pages as reserved.
         */
        for (i = 0; i < memSize; i+= PAGE_SIZE) {
                SetPageReserved(virt_to_page(newMemArea + i));
        }

        /*
         * While we were doing that, it's possible that someone else has got in and already
         * allocated the memory.  So check for that, and if so discard the memory we allocated.
         */
        spin_lock_irqsave(&rtapishm_lock, irqState);
        if (memArea) {
            /*
             * Free the memory we just allocated.
             */
            spin_unlock_irqrestore(&rtapishm_lock, irqState);
            kfree(newAllocArea);
        } else {
            /*
             * We still need some memory, so save the stuff we have.
             */
            memArea = newMemArea;
            allocArea = newAllocArea;
            memLength = length;
            spin_unlock_irqrestore(&rtapishm_lock, irqState);
        }
    }

    /*
     * We now have an allocated area - check that it's the right length.
     */
    if (memLength < length) {
        err( "%s: requested %d bytes, but already allocated %d", __func__,
                length, memLength);
        return(-EFBIG);
    }

    /*
     * If the user has requested a pointer to the memory area, supply it for them.
     */
    if (pointer) {
        *pointer = memArea;
    }

    return(0);
}
EXPORT_SYMBOL(rtapishm_get_memory);

static int rtapishm_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long length;
    int ret;

    printk(KERN_ERR "%s(%ld):", __func__, vma->vm_end - vma->vm_start);

    length = vma->vm_end - vma->vm_start;

    ret = rtapishm_get_memory(length, NULL);
    if (ret != 0) {
        err( "%s: Couldn't allocate shared memory for user space", __func__);
        return(ret);
    }

    /*
     * Map the allocated memory into the calling processes address space.
     */
    ret = remap_pfn_range(vma, vma->vm_start, virt_to_phys((void *)memArea) >> PAGE_SHIFT,
            length, vma->vm_page_prot);
    if (ret < 0) {
        err( "%s: remap of shared memory failed, %d", __func__, ret);
        return(ret);
    }

    return(0);
}

/* 
 * This function is called whenever a process which has already opened the
 * device file attempts to read from it.
 */
static ssize_t rtapishm_read(struct file *file, char __user * buffer, size_t length,
        loff_t * offset)
{
    info("%s: len %d, offset %d", __func__, length, (int)(*offset));

    /*
     * Check for a read that takes us past the end of the memory area.
     */
    if ((*offset + length) > memLength) {
        if (*offset < memLength) {
            length = memLength - *offset;
        } else {
            *offset = -1;
            return(0);
        }
    }

    copy_to_user(buffer, memArea + *offset, length);
    *offset += length;

    return(length);
}

static int rtapishm_close(struct inode* inode, struct file* filp)
{
	dbg("");
	return 0;
}

static int major;
static struct class *class_rtapishm;
static struct device *dev_rtapishm;

static struct file_operations rtapishm_fileops = {
    .owner = THIS_MODULE,
    .read = rtapishm_read,
    .release = rtapishm_close,
    .mmap = rtapishm_mmap,
};

static ssize_t sys_add_to_fifo(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	dbg("");
	return count;
}

static ssize_t sys_reset(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	dbg("");
	return count;
}

/* Declare the sysfs entries. The macros create instances of dev_attr_fifo and dev_attr_reset */
static DEVICE_ATTR(fifo, S_IWUSR, NULL, sys_add_to_fifo);
static DEVICE_ATTR(reset, S_IWUSR, NULL, sys_reset);


/*
 * Set up the char_dev structure for this device.
 */
static void rtapishm_setup_cdev(struct rtapishm_dev *dev, int index)
{
    int err, devno = MKDEV(major, index);

    cdev_init(&dev->cdev, &rtapishm_fileops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &rtapishm_fileops;
    err = cdev_add (&dev->cdev, devno, 1);
    /* Fail gracefully if need be */
    if (err)
	warn("Error %d adding %s%d", err,  DEVICE_NAME, index);
}

void rtapishm_cleanup_module(void)
{
    int i ;
    struct device *dev;
    dev_t devno = MKDEV(major, 0);
    dbg("");

    /* Get rid of our char dev entries */
    if (rtapishm_devices) {
	for (i = 0; i < count; i++) {
	    cdev_del(&rtapishm_devices[i].cdev);
	    dev = rtapishm_devices[i].rtapishm_device;
	    device_destroy(class_rtapishm, MKDEV(major, i));
	    dbg("device_destroy %s%d done", DEVICE_NAME, i);
	}
	kfree(rtapishm_devices);
	dbg("rtapishm_devices freed");
    }

    unregister_chrdev_region(devno, count);

    if (class_rtapishm) {
	class_unregister(class_rtapishm);
	class_destroy(class_rtapishm);
	dbg("class_destroy done");
    }
}

static int rtapishm_init(void) {
    void *ptr_err = NULL;
    int retval, i;

    dbg("");

    if ((major = register_chrdev(0, DEVICE_NAME, &rtapishm_fileops)) < 0) {
	err("failed to register device: error %d\n", major);
	goto failed_chrdevreg;
    }

    class_rtapishm = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(ptr_err = class_rtapishm)) {
	err("failed to register device class '%s'\n", CLASS_NAME);
	retval = PTR_ERR(class_rtapishm);
	goto failed_classreg;
    }

    rtapishm_devices = kzalloc(count * sizeof(struct rtapishm_dev), GFP_KERNEL);
    if (!rtapishm_devices) {
	err("cant kzalloc rtapishm_dev\n");
	retval = -ENOMEM;
	goto failed_chrdevreg;
    }

    for (i = 0; i < count; i++) {
	rtapishm_setup_cdev(&rtapishm_devices[i], i);
	rtapishm_devices[i].rtapishm_device = 

	    device_create(class_rtapishm, NULL, MKDEV(major, i), 
			  NULL, DEVICE_NAME "%u", i );
	if (IS_ERR(rtapishm_devices[i].rtapishm_device)) {
	    err("failed to create device '%s%d'\n", CLASS_NAME, i);
	    retval = PTR_ERR(rtapishm_devices[i].rtapishm_device);
	    goto failed_devreg;
	}
    }

#if 0
    dev_rtapishm = device_create(class_rtapishm, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(ptr_err = dev_rtapishm)) {
	err("failed to create device '%s_%s'", CLASS_NAME, DEVICE_NAME);
	retval = PTR_ERR(dev_rtapishm);
	goto failed_devreg;
    }
    /* Now we can create the sysfs endpoints (don't care about errors).
     * dev_attr_fifo and dev_attr_reset come from the DEVICE_ATTR(...) earlier */
    retval = device_create_file(dev_rtapishm, &dev_attr_fifo);
    if (retval < 0) {
	warn("failed to create write /sys endpoint - continuing without");
    }
    retval = device_create_file(dev_rtapishm, &dev_attr_reset);
    if (retval < 0) {
	warn("failed to create reset /sys endpoint - continuing without");
    }
#endif

    spin_lock_init(&rtapishm_lock);

    /* struct kobject *play_with_this = &dev_rtapishm->kobj; */

    return 0;
 failed_devreg:
    /* class_destroy(class_rtapishm); */
 failed_classreg:
    /* unregister_chrdev(major, DEVICE_NAME); */
    rtapishm_cleanup_module();

failed_chrdevreg:
    return PTR_ERR(ptr_err);
}

static void rtapishm_exit(void) 
{
    // printk(KERN_ERR "%s: rtapishm driver exiting\n", __func__);
    dbg("");

    if (allocArea) {
        kfree(allocArea);
    }
    rtapishm_cleanup_module();

#if 0
    device_remove_file(dev_rtapishm, &dev_attr_fifo);
    device_remove_file(dev_rtapishm, &dev_attr_reset);
    device_destroy(class_rtapishm, MKDEV(major, 0));
    class_destroy(class_rtapishm);
    unregister_chrdev(major, DEVICE_NAME);
#endif

}

MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
module_init(rtapishm_init);
module_exit(rtapishm_exit);
