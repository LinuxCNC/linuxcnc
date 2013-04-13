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
#include <linux/sched.h> 

#include "shmdrv.h"

static bool debug = false;
module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "enable debug info (default: false)");

static int nseg = 200;
module_param(nseg, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(nseg, "number of shm segments (default: 200)");

static int mlimit = 1024*1024;
module_param(mlimit, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mlimit, "memory size below which to use kmalloc, above vmalloc; default 1M");

static int nopen;

struct shm_segment {
    bool in_use;
    int key;
    size_t size;     // requested
    size_t act_size; // aligned
    int n_kattach;
    int n_uattach;
    int creator;  // pid or 0 for kernel
    void *kmem;
};

static struct shm_segment  *shm_segments;
static spinlock_t shm_lock;

static int shm_malloc(struct shm_segment *seg)
{
    void *mem;
    unsigned long adr;
    size_t size;

    size = PAGE_ALIGN(seg->size);
    mem = vmalloc_32(size);
    if (!mem)
	return -ENOMEM;

    seg->act_size = size;
    memset(mem, 0, size); 
    adr = (unsigned long) mem;
    while (size > 0) {
	SetPageReserved(vmalloc_to_page((void *)adr));
	adr += PAGE_SIZE;
	size -= PAGE_SIZE;
    }
    seg->kmem = mem;
    return 0;
}

static void shm_free(struct shm_segment *seg)
{
    unsigned long adr;
    size_t size;

    if (!seg->kmem)
	return;

    adr = (unsigned long) seg->kmem;
    size = seg->act_size;
    while ((long) size > 0) {
	ClearPageReserved(vmalloc_to_page((void *)adr));
	adr += PAGE_SIZE;
	size -= PAGE_SIZE;
    }
    vfree(seg->kmem);
}


// usage tracking
static void shmdrv_vma_open(struct vm_area_struct *vma)
{
    int segno = (int) vma->vm_private_data;
    struct shm_segment *seg = &shm_segments[segno];

    info("VMA open, virt %lx, phys %lx length %ld segno=%d",
	vma->vm_start, 
	vma->vm_pgoff << PAGE_SHIFT,
	vma->vm_end - vma->vm_start,
	segno);

    seg->n_uattach++;
}

static void shmdrv_vma_close(struct vm_area_struct *vma)
{
    int segno = (int) vma->vm_private_data;
    struct shm_segment *seg = &shm_segments[segno];

    info("VMA close releasing %p, segno=%d", vma, segno);
    seg->n_uattach--;
}

static struct vm_operations_struct mmap_ops = {
    .open = shmdrv_vma_open,
    .close = shmdrv_vma_close,
};

static int shm_mmap(struct file *file, struct vm_area_struct *vma)
{
    int length, segno;
    int ret;
    struct shm_segment *seg;

    // check file for private_data pointing to a shmem_data entry, fail if not
    // this must have been set by the ioctl ATTACH
    segno = (int) file->private_data;

    dbg("%s(%ld) , segno=%d",
	__func__, 
	vma->vm_end - vma->vm_start,
	segno);

    if (segno == -1) {
	err("no segment associated with file");
	return -EINVAL;
    }

    if ((segno < 0) || (segno > nseg-1)) {
	err("invalid segmend index  %d, nseg=%d", segno, nseg);
	return -EINVAL;
    }

    seg = &shm_segments[segno];
    length = vma->vm_end - vma->vm_start;

    if (length > seg->act_size) {
	err("segment %d: map size %d greater than segment size %d", 
	    segno, length,seg->size);
	return -EINVAL;
    }

    if (!seg->in_use) {
	err("BUG: segment %d not in use", segno);
	return -EINVAL;
    }
    if (!seg->kmem) {
	err("BUG: segment %d kmem == NULL", segno);
	return -EINVAL;
    }

    // Map the allocated memory into the calling processes address space.
    ret = remap_pfn_range(vma, vma->vm_start, 
			  virt_to_phys((void *)seg->kmem) >> PAGE_SHIFT,
			  length, vma->vm_page_prot);
    if (ret < 0) {
        err( "%s: remap of shared memory failed, %d", __func__, ret);
        return(ret);
    }
    vma->vm_private_data = (void *) segno; // so it can be referred to in vma ops
    vma->vm_ops = &mmap_ops;
    shmdrv_vma_open(vma);

    return(0);
}

static int shm_open(struct inode* inode, struct file* filp)
{
    filp->private_data = (void *) -1; // no valid segment associated

    dbg("");
    nopen++;
    return 0;
}


static int shm_close(struct inode* inode, struct file* filp)
{
    dbg("");
    nopen--;
    return 0;
}

static int find_shm_by_key(int key)
{
    int i;
    struct shm_segment *segs = shm_segments;

    for(i = 0; i < nseg; i++) {
	if (segs[i].in_use && (key == segs[i].key))
	    return i;
    }
    return -ENOENT;
}

static int find_free_shm_lot(void)
{
    int i;
    struct shm_segment *segs = shm_segments;

    for(i = 0; i < nseg; i++) {
	if (!segs[i].in_use)
	    return i;
    }
    return -EINVAL;
}

int free_segments(void)
{
    int n;
    struct shm_segment *seg;
    int fail = 0;

    for (n = 0; n < nseg; n++) {
	seg = &shm_segments[n];
	if (seg->in_use) {
	    if (seg->n_kattach || seg->n_uattach) {
		err("segment %d still in use userattach=%d kattach=%d",
		    n, seg->n_uattach, seg->n_kattach);
		fail++;
		continue;
	    }
	    shm_free(seg);
	}
    }
    return fail;
}

void init_shmemdata(void)
{
    int n;
    struct shm_segment *seg;

    for (n = 0; n < nseg; n++) {
	seg = &shm_segments[n];
	seg->in_use = 0;
	seg->key = 0;
	seg->size = 0;
	seg->act_size = 0;
	seg->n_kattach = 0;
	seg->n_uattach = 0;
	seg->creator = -1;
	seg->kmem = 0;
    }
}

static long shm_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0, segno;
    struct shm_ioctlmsg sm; 
    struct shm_segment *seg;
    unsigned long irqState;

    spin_lock_irqsave(&shm_lock, irqState);

    switch(cmd) {

    case IOC_SHM_STATUS:
	ret = copy_from_user(&sm, (char *)arg, sizeof(struct shm_ioctlmsg));
	if (ret < 0) {
	    err("IOC_SHM_STATUS: copy_from_user %d", ret);
	    goto done;
	}
	ret = find_shm_by_key(sm.key);
	if (ret < 0) {
	    err("IOC_SHM_STATUS: segemnt %d does not exist", sm.key);
	    goto done;
	}
	seg = &shm_segments[ret];
	sm.id = ret;
	sm.n_kattach = seg->n_kattach;
	sm.n_uattach = seg->n_uattach;
	sm.size = seg->size;
	sm.act_size = seg->act_size;
	sm.creator = seg->creator;
	ret = copy_to_user((char *)arg, &sm, sizeof(struct shm_ioctlmsg));
	if (ret < 0) {
	    err("IOC_SHM_STATUS: copy_to_user %d", ret);
	}
	break;

    case IOC_SHM_CREATE:
	ret = copy_from_user(&sm, (char *)arg, sizeof(struct shm_ioctlmsg));
	if (ret < 0) {
	    err("IOC_SHM_CREATE: copy_from_user %d", ret);
 	    goto done;
	}
	segno = find_shm_by_key(sm.key);
	if (segno > -1) {
	    dbg("CREATE: shm segment exists: key=%x pos=%d", sm.key, segno);
	    goto done;
	}
	segno = find_free_shm_lot();
	if (segno < 0) {
	    err("IOC_SHM_CREATE: all slots full, use larger nseg param (%d)", nseg);
	    ret = -EINVAL;
	    goto done;
	}
	seg = &shm_segments[segno];
	seg->creator = current->pid;
	seg->in_use = 1;
	seg->key = sm.key;
	seg->n_kattach = 0;
	seg->n_uattach = 0;
	seg->size = sm.size;
	ret = shm_malloc(seg);
	if (ret) {
	    err("CREATE: shm_malloc fail size=%d", seg->size);
	    goto done;
	}

	file->private_data = (void *) segno; // record shm id for ensuing mmap
	sm.id = segno;
	sm.n_kattach = seg->n_kattach;
	sm.n_uattach = seg->n_uattach;
	sm.size = seg->size;
	sm.act_size = seg->act_size;
	sm.creator = seg->creator;
	ret = copy_to_user((char *)arg, &sm, sizeof(struct shm_ioctlmsg));
	if (ret < 0) {
	    err("IOC_SHM_CREATE: copy_to_user %d", ret);
	}
	err("created seg %d size %d key %d at %p",
	    segno, seg->size, seg->key, seg->kmem);
	break;

    case IOC_SHM_ATTACH:

	ret = copy_from_user(&sm, (char *)arg, sizeof(struct shm_ioctlmsg));
	if (ret < 0) {
	    err("IOC_SHM_ATTACH: copy_from_user %d", ret);
	    goto done;
	}
	ret = find_shm_by_key(sm.key);
	if (ret < 0) {
	    dbg("ATTACH: shm segment does not exist: key=%d ret=%d", sm.key, ret);
	    goto done;
	}
	sm.id = ret;
	seg = &shm_segments[ret];
	sm.size =seg->size;
	sm.n_kattach = seg->n_kattach;
	sm.n_uattach = seg->n_uattach;
	sm.creator = seg->creator;

	ret = copy_to_user((char *)arg, &sm, sizeof(struct shm_ioctlmsg));
	if (ret < 0) {
	    err("IOC_SHM_ATTACH: copy_to_user %d", ret);
	    goto done;
	}
	file->private_data = (void *) sm.id; // record shm id for ensuing mmap
	ret = 0;
	break;

    case IOC_SHM_DELETE:
	ret = copy_from_user(&sm, (char *)arg, sizeof(struct shm_ioctlmsg));
	if (ret < 0) {
	    err("IOC_SHM_DELETE: copy_from_user %d", ret);
	    goto done;
	}
	ret = find_shm_by_key(sm.key);
	if (ret < 0) {
	    dbg("DELETE: shm segment does not exist: key=%x pos=%d", sm.key, ret);
	    goto done;
	}
	// actually delete it
	break;

    default:
	ret = -EINVAL;
    }
 done:
    spin_unlock_irqrestore(&shm_lock, irqState);
    return ret;
}


static struct file_operations fileops = {
    .owner = THIS_MODULE,
    .open = shm_open,
    //  .read = shm_read,
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
			  char* buf, size_t count)
{
    int i;
    size_t size, written, left = PAGE_SIZE - 80; // leave some space for "..." line
    struct shm_segment *seg;
    unsigned long irqState;
    int nsegments = 0;
    int total_alloc = 0;
    int total_alloc_aligned = 0;
    int kattach = 0, uattach = 0;

    dbg("");
    spin_lock_irqsave(&shm_lock, irqState);

    // stats
    for (i = 0; i < nseg; i++) {
	seg = &shm_segments[i];
	if (seg->in_use) {
	    nsegments++;
	    total_alloc += seg->size;
	    total_alloc_aligned += seg->act_size;
	    uattach += seg->n_uattach;
	    kattach += seg->n_kattach;
	}
    }
    size = scnprintf(buf, left, 
		     "%d segment(s), open=%d uattach=%d kattach=%d total=%d aligned=%d\n", 
		     nsegments, nopen, uattach, kattach, total_alloc, total_alloc_aligned);
    left -= size;
    buf += size;
    written = size;
    for (i = 0; i < nseg; i++) {
	seg = &shm_segments[i];
	if (seg->in_use) {
	    if (left < 80) {
		size = scnprintf(buf, left, "...\n");
		left -= size;
		written += size;
		goto done;
	    }
	    size = scnprintf(buf, left,
			    "%d: key=%d/0x%8.8x size=%d aligned=%d ul=%d k=%d creator=%d mem=%p\n",
			     i, seg->key, seg->key, seg->size, seg->act_size, 
			     seg->n_uattach,
			     seg->n_kattach, seg->creator, seg->kmem);
	    left -= size;
	    written += size;
	    buf += size;
	}
    }

 done:
    spin_unlock_irqrestore(&shm_lock, irqState);
    return written;
}

static DEVICE_ATTR(status, S_IRUGO, sys_status, NULL);

static struct attribute *shmdrv_sysfs_attrs[] = {
    &dev_attr_status.attr,
    NULL,
};

static const struct attribute_group shmdrv_sysfs_attr_group = {
    .attrs = shmdrv_sysfs_attrs,
};

static int shmdrv_init(void) {
    int retval;
    struct device *this_device;

    dbg("");

    shm_segments = kzalloc(nseg * sizeof(struct shm_segment), GFP_KERNEL);
    if (!shm_segments) {
	err("cant kzalloc %d shm segment descriptors", nseg);
	return -ENOMEM;
    }

    init_shmemdata();

    retval =  misc_register(&shm_misc_dev);
    if (retval) {
	err("misc_register failed");
	return retval;
    }

    this_device = shm_misc_dev.this_device;
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
    int ret;

    dbg("");

    ret = free_segments();
    if (ret) {
	err("%d segments still in use", ret);
	return -EBUSY;
    }
    if (shm_segments)
        kfree(shm_segments);

    misc_deregister(&shm_misc_dev);
}

MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
module_init(shmdrv_init);
module_exit(shmdrv_exit);


//int _rtapi_shmem_new_inst(int userkey, int instance, int module_id, unsigned long int size) {
// int _rtapi_shmem_getptr(int handle, void **ptr) {
//int _rtapi_shmem_delete(int handle, int module_id) {

#if 0
> I am trying to map a vmalloc kernel buffer to user
> space using remap_page_range(). In my module, this
    > function returns success if we call mmap() from user
	> space, but i can not access content of vmalloc buffer
	> from user space. Pointer returned by mmap() syscall
	> seems pointing to other memory page which contains
	> zeros. I am using linux 2.6.10 kernel on Pentium 4
	> system.

	Look for "rvmalloc" in various drivers in the kernel source tree:
    you must SetPageReserved before remap_pfn_range (or remap_page_range)
	agrees to map the page, and ClearPageReserved before freeing after.

    // allocate and prepare memory for our buffer
	my_context->buf = kmalloc(BUFFER_SIZE, GFP_KERNEL);
/* mark pages reserved so that remap_pfn_range works */
for (vaddr = (unsigned long)my_context->buf;
     vaddr < (unsigned long)my_context->buf + BUFFER_SIZE;
     vaddr += PAGE_SIZE)
    SetPageReserved(virt_to_page(vaddr));

/* clear pages reserved */
for (vaddr = (unsigned long)my_context->buf;
     vaddr < (unsigned long)my_context->buf + BUFFER_SIZE;
     vaddr += PAGE_SIZE)
    ClearPageReserved(virt_to_page(vaddr));

kfree(my_context->buf);

/*
 * Guarantee that the kmalloc'd memory is cacheline aligned.
 */
void *
xpc_kmalloc_cacheline_aligned(size_t size, gfp_t flags, void **base)
{
    /* see if kmalloc will give us cachline aligned memory by default */
    *base = kmalloc(size, flags);
    if (*base == NULL)
	return NULL;

    if ((u64)*base == L1_CACHE_ALIGN((u64)*base))
	return *base;

    kfree(*base);

    /* nope, we'll have to do it ourselves */
    *base = kmalloc(size + L1_CACHE_BYTES, flags);
    if (*base == NULL)
	return NULL;

    return (void *)L1_CACHE_ALIGN((u64)*base);
}

#endif
#if 0
/*
 * Allocate the shared memory. 
 */
extern int shm_get_memory(int length, void **pointer)
{
    int i;

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
// EXPORT_SYMBOL(shm_get_memory);
#endif
#if 0
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
#endif
