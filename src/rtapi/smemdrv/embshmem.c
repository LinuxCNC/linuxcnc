/*
 * This driver provides a single shared memory area that can be accessed from user space and
 * from kernel space.
 *
 * Copyright (c) Embrisk Ltd 2012.
 * This is public domain software with no warranty.
 */
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/mman.h>

#include "embshmem.h"

/*
 * Device related data.
 */
static dev_t myDevice;
static struct cdev *cdev_p;

/*
 * allocArea is the memory block that we allocate, of which memArea points to the
 * bit we actually use.
 */
static void *allocArea;
static void *memArea;
static int memLength;
static spinlock_t smemdrv_lock;

/*
 * Allocate the shared memory.  This is called directly by other kernel routines that access
 * the shared memory, and indirectly by user space via smemdrv_mmap.
 */
extern int smemdrv_get_memory(int length, void **pointer)
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
            printk(KERN_CRIT "%s: Couldn't allocate memory to share\n", __func__);
            return(-ENOMEM);
        }
    
        /*
         * memArea is the page aligned area we actually use.
         */
        newMemArea = (void *)(((unsigned long)newAllocArea + PAGE_SIZE - 1) & PAGE_MASK);
        memset(newMemArea, 0, length);
        printk(KERN_INFO "%s: allocArea %p, memArea %p, size %x\n", __func__, newAllocArea, 
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
        spin_lock_irqsave(&smemdrv_lock, irqState);
        if (memArea) {
            /*
             * Free the memory we just allocated.
             */
            spin_unlock_irqrestore(&smemdrv_lock, irqState);
            kfree(newAllocArea);
        } else {
            /*
             * We still need some memory, so save the stuff we have.
             */
            memArea = newMemArea;
            allocArea = newAllocArea;
            memLength = length;
            spin_unlock_irqrestore(&smemdrv_lock, irqState);
        }
    }

    /*
     * We now have an allocated area - check that it's the right length.
     */
    if (memLength < length) {
        printk(KERN_CRIT "%s: requested %d bytes, but already allocated %d\n", __func__,
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
EXPORT_SYMBOL(smemdrv_get_memory);

static int smemdrv_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long length;
    int ret;

    printk(KERN_ERR "%s(%ld):\n", __func__, vma->vm_end - vma->vm_start);

    length = vma->vm_end - vma->vm_start;

    ret = smemdrv_get_memory(length, NULL);
    if (ret != 0) {
        printk(KERN_CRIT "%s: Couldn't allocate shared memory for user space\n", __func__);
        return(ret);
    }

    /*
     * Map the allocated memory into the calling processes address space.
     */
    ret = remap_pfn_range(vma, vma->vm_start, virt_to_phys((void *)memArea) >> PAGE_SHIFT,
            length, vma->vm_page_prot);
    if (ret < 0) {
        printk(KERN_CRIT "%s: remap of shared memory failed, %d\n", __func__, ret);
        return(ret);
    }

    return(0);
}

/* 
 * This function is called whenever a process which has already opened the
 * device file attempts to read from it.
 */
static ssize_t smemdrv_read(struct file *file, char __user * buffer, size_t length,
        loff_t * offset)
{
    printk(KERN_INFO "%s: len %d, offset %d\n", __func__, length, (int)(*offset));

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

static struct file_operations fileOps = {
    .owner = THIS_MODULE,
    .read = smemdrv_read,
    .mmap = smemdrv_mmap,
};


static int smemdrv_init(void) {
    int ret;

    printk(KERN_ERR "%s: initialising embshmem driver \n", __func__);

    ret = alloc_chrdev_region(&myDevice, 0, 1, "embshmem");
    if (ret < 0) {
        printk(KERN_CRIT "%s: Couldn't allocate device number (%d).\n", __func__, ret);
        return(ret);
    }

    
    cdev_p = cdev_alloc();
    cdev_p->ops = &fileOps;
    ret = cdev_add(cdev_p, myDevice, 1);
    if (ret) {
        printk(KERN_CRIT "%s: Couldn't add character device (%d)\n", __func__, ret);
        return(ret);
    }

    spin_lock_init(&smemdrv_lock);
    return 0;
}
    
static void smemdrv_exit(void) {
    printk(KERN_ERR "%s: embshmem driver exiting\n", __func__);
    cdev_del(cdev_p);
    unregister_chrdev_region(myDevice, 1);
    if (allocArea) {
        kfree(allocArea);
    }
}

MODULE_LICENSE("GPL");
module_init(smemdrv_init);
module_exit(smemdrv_exit);
