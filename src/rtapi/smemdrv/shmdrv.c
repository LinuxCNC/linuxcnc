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
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/ioctl.h>

#include "shmdrv.h"

static bool debug = false;
module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "enable debug info (default: false)");


/*
 * Device related data.
 */

struct shm_dev {
    /* struct cdev cdev; */
    /* struct device *shm_device; */
    // shmem_data shmem[RTAPI_MAX_SHMEMS+1];
};

static struct shm_dev *devices;

/*
 * allocArea is the memory block that we allocate, of which memArea points to the
 * bit we actually use.
 */
static void *allocArea;
static void *memArea;
static int memLength;
static spinlock_t shm_lock;

//int _rtapi_shmem_new_inst(int userkey, int instance, int module_id, unsigned long int size) {
// int _rtapi_shmem_getptr(int handle, void **ptr) {
//int _rtapi_shmem_delete(int handle, int module_id) {


/*
 * Allocate the shared memory.  This is called directly by other kernel routines that access
 * the shared memory, and indirectly by user space via shm_mmap.
 */
extern int shm_get_memory(int length, void **pointer)
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
            err("Couldn't allocate memory to share");
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
        spin_lock_irqsave(&shm_lock, irqState);
        if (memArea) {
            /*
             * Free the memory we just allocated.
             */
            spin_unlock_irqrestore(&shm_lock, irqState);
            kfree(newAllocArea);
        } else {
            /*
             * We still need some memory, so save the stuff we have.
             */
            memArea = newMemArea;
            allocArea = newAllocArea;
            memLength = length;
            spin_unlock_irqrestore(&shm_lock, irqState);
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
EXPORT_SYMBOL(shm_get_memory);

static void simple_vma_open(struct vm_area_struct *vma)
{
    dbg("VMA open, virt %lx, phys %lx length %ld private=%lx\n",
	vma->vm_start, 
	vma->vm_pgoff << PAGE_SHIFT,
	vma->vm_end - vma->vm_start,
	(unsigned long) vma->vm_private_data);
    // increase refcount here
}
static void simple_vma_close(struct vm_area_struct *vma)
{
    printk("VMA close releasing %p, private_data = %p\n", vma, vma->vm_private_data);
    // decrease refcount here
}

static struct vm_operations_struct mmap_ops = {
    .open = simple_vma_open,
    .close = simple_vma_close,
};

static int shm_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long length;
    int ret;

    // check file for private_data pointing to a shmem_data entry, fail if not
    // this must have been set by the ioctl ATTACH

    dbg("%s(%ld) , private_data=0x%lx",
	__func__, 
	vma->vm_end - vma->vm_start,
	(unsigned long)file->private_data);

    length = vma->vm_end - vma->vm_start;

    ret = shm_get_memory(length, NULL);
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
    // make vma->vm_private_data point to the shmem entry
    vma->vm_ops = &mmap_ops;
    simple_vma_open(vma);

    return(0);
}

/* 
 * This function is called whenever a process which has already opened the
 * device file attempts to read from it.
 */
static ssize_t shm_read(struct file *file, char __user * buffer, size_t length,
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

static int shm_close(struct inode* inode, struct file* filp)
{
    dbg("");
    return 0;
}

static long shm_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    int val;
    struct shm_ioctlmsg sm; 

    //    mutex_lock(&shm_sysfs_mutex);

    switch(cmd) {
    case IOC_SHM_EXISTS:
	ret = copy_from_user(&val, (char *)arg, sizeof(int));

	dbg("EXISTS: %d (ret=%d)", val, ret);
	break;

    case IOC_SHM_CREATE:
	ret = copy_from_user(&sm, (char *)arg, sizeof(struct shm_ioctlmsg));
	dbg("CREATE: id=%d size=%d (ret=%d)", sm.id, sm.size, ret);

	//ret = put_user(dev->rcache, (int __user *)arg);
	break;

    case IOC_SHM_ATTACH:
	dbg("ATTACH");

	sm.id = 4711;
	sm.size = 4096;
	ret = copy_to_user((char *)arg, &sm, sizeof(struct shm_ioctlmsg));
	file->private_data = (void *) 0x01020304;
	break;

    case IOC_SHM_DELETE:
	break;

    default:
	ret = -EINVAL;
    }
    //    mutex_unlock(&shm_sysfs_mutex);
    return ret;
}


static struct file_operations fileops = {
    .owner = THIS_MODULE,
    .read = shm_read,
    .release = shm_close,
    .unlocked_ioctl = shm_unlocked_ioctl,
    .mmap = shm_mmap,
};

static struct miscdevice shm_misc_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= DEVICE_NAME,
	.fops	= &fileops,
};


static ssize_t sys_status(struct device* dev, struct device_attribute* attr, 
			  const char* buf, size_t count)
{
    int ret;
    dbg("");
    spin_lock_irq(&shm_lock);
    ret = snprintf(buf, PAGE_SIZE, "%u\n", 0x4711);
    spin_unlock_irq(&shm_lock);

    return ret;
}

static DEVICE_ATTR(status, S_IRUGO, sys_status, NULL);

static struct attribute *shmdrv_sysfs_attrs[] = {
	&dev_attr_status.attr,
	NULL,
};

static const struct attribute_group shmdrv_sysfs_attr_group = {
	.attrs = shmdrv_sysfs_attrs,
};


void shm_cleanup_module(void)
{
    dbg("");

    if (devices) {

	kfree(devices);
	dbg("devices freed");
    }
}

/* void init_shmemdata(shmem_data *shmem_array) */
/* { */
/*     int n,m; */
/*     for (n = 0; n <= RTAPI_MAX_SHMEMS; n++) { */
/* 	shmem_array[n].key = 0; */
/* 	shmem_array[n].rtusers = 0; */
/* 	shmem_array[n].ulusers = 0; */
/* 	shmem_array[n].size = 0; */
/* 	for (m = 0; m < _BITS_TO_LONGS(RTAPI_MAX_SHMEMS +1); m++) { */
/* 	    shmem_array[n].bitmap[m] = 0; */
/* 	} */
/*     } */
/* } */


static int shmdrv_init(void) {
    int retval;
    struct device *this_device;

    dbg("");

    retval =  misc_register(&shm_misc_dev);
    if (retval) {
	err("misc_register failed");
	return retval;
    }

    /* Create the sysfs files */
    this_device = shm_misc_dev.this_device;

    //    dev_set_drvdata(this_device, priv); // ??
    retval = sysfs_create_group(&this_device->kobj, &shmdrv_sysfs_attr_group);
    if (retval) {
	err("Unable to create sysfs files: %d", retval);
	misc_deregister(&shm_misc_dev);
	return retval;
    }

    spin_lock_init(&shm_lock);

    return 0;
}

static void shmdrv_exit(void) 
{
    dbg("");

    if (allocArea) {
        kfree(allocArea);
    }
    // shm_cleanup_module();
    misc_deregister(&shm_misc_dev);
}

MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
module_init(shmdrv_init);
module_exit(shmdrv_exit);
