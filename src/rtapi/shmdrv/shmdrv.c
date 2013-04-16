/* This driver provides a single shared memory area that can be accessed from user space and
 * from kernel space.
 *
 * NB: add 'vmalloc=128MB' or more to the kernel command line!
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
#include <linux/vmalloc.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/sched.h> 
#include <linux/kallsyms.h>
#include <linux/string.h>

#include "shmdrv.h"

static bool debug = true; // false;
module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "enable debug info (default: false)");

static bool gc = true;
module_param(gc, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(gc, "delete segment on last detach (default: false)");

static int nseg = 200;
module_param(nseg, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(nseg, "number of shm segments (default: 200)");

static int mlimit = 1024*1024;
module_param(mlimit, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mlimit, "memory size below which to use kmalloc, above vmalloc; default 1M");

MODULE_AUTHOR("Michael Haberler <git@mah.priv.at>");
MODULE_DESCRIPTION("shared memory driver");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");

struct shm_segment {
    bool in_use;
    int key;
    size_t size;     // requested
    size_t act_size; // aligned
    int n_kattach;
    int n_uattach;
    int flags;
    int creator;  // pid or 0 for kernel
    void *kmem;
};

static int nopen;
static int maxopen = -1;
static size_t allocated, freed;
static struct shm_segment  *shm_segments;
static spinlock_t shm_lock, ll_lock;
static int  shm_nonstandard_detach(struct shm_segment *segs);

static int shm_malloc(struct shm_segment *seg)
{
    void *mem;
    unsigned long adr;
    size_t size;

    size = PAGE_ALIGN(seg->size);
    mem = vmalloc_user(size);
    if (!mem)
	return -ENOMEM;

    allocated += size;
    seg->act_size = size;
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
    freed += size;
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
    int ret;
    int segno = (int) vma->vm_private_data;
    struct shm_segment *seg = &shm_segments[segno];

    info("VMA close releasing %p, segno=%d", vma, segno);
    seg->n_uattach--;
    if (gc && ((seg->n_uattach + seg->n_kattach) == 0)) {
	// last reference gone away, gc true
	if (seg->flags & OUTBOARD_MASK) {
	    // a segment type requiring special massage
	    ret = shm_nonstandard_detach(seg);
	} else {
	    // vanilla shm segment
	    shm_free(seg);
	    ret = 0;
	}
	info("unmap: gc segment %d key=0x%8.8x", segno, seg->key);
	seg->in_use = 0;
    }
}

// see http://stackoverflow.com/questions/654393/examining-mmaped-addresses-using-gdb
// needed to get gdb to work on mmap'ed segments
static int shmdrv_generic_access_phys(struct vm_area_struct *vma, unsigned long addr,
            void *buf, int len, int write)
{
    void __iomem *maddr;
    struct shm_segment *seg;
    int segno,offset;

    segno = (int) vma->vm_private_data;

    if ((segno < 0) || (segno > nseg)) {
	err("segno out of range: %d",segno);
	return -EINVAL;
    }

    seg = &shm_segments[segno];
    offset = (addr) - vma->vm_start;
    maddr = phys_to_virt(__pa(seg->kmem));
    info("%d: maddr %p kmem %p len %d offset %d wr:%d",
	 segno,maddr, seg->kmem, len,offset, write);

    if (write)
        memcpy_toio(maddr + offset, buf, len);
    else
        memcpy_fromio(buf, maddr + offset, len);

    return len;
}

static inline int shmdrv_vma_access(struct vm_area_struct *vma, unsigned long addr,
          void *buf, int len, int write)
{
    return shmdrv_generic_access_phys(vma, addr, buf, len, write);
}

static struct vm_operations_struct mmap_ops = {
    .open   = shmdrv_vma_open,
    .close  = shmdrv_vma_close,
    .access = shmdrv_vma_access,
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
	err("segment %d: map size %d greater than segment size %d/%d",
	    segno, length,seg->act_size,seg->size);
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

    ret = remap_vmalloc_range(vma, seg->kmem, 0);
    if (ret < 0) {
        err( "%s: remap_pfn_range() failed, %d", __func__, ret);
        return(ret);
    }
    vma->vm_private_data = (void *) segno; // so it can be referred to in vma ops
    vma->vm_ops = &mmap_ops;
    shmdrv_vma_open(vma);  // track usage
    info("mmap seg %d size %d key %d/%x at %p length %d",
	 segno, seg->size, seg->key, seg->key, seg->kmem, length);
    return(0);
}

static int  shm_nonstandard_attach(struct shm_segment *segs)
{
    dbg("");

    // ll_lock already engaged 
    if (segs->flags &  OUTBOARD1_CREATE) {
	dbg("attach code for outboard type 1 missing");
	return -EINVAL;
    } else if (segs->flags &  OUTBOARD2_CREATE) {
	dbg("attach code for outboard type 2 missing");
	return -EINVAL;
    } else {
	// not handled
	err("attach: invalid flags 0x%x - no such outboard type",segs->flags);
	return -EINVAL;
    }
}

static int  shm_nonstandard_detach(struct shm_segment *segs)
{
    dbg("");

    if (segs->flags &  OUTBOARD1_CREATE) {
	dbg("detach code for outboard type 1 missing");
	return -EINVAL;
    } else if (segs->flags &  OUTBOARD2_CREATE) {
	dbg("detach code for outboard type 2 missing");
	return -EINVAL;
    } else {
	// not handled
	err("detach: invalid flags 0x%x - no such outboard type",segs->flags);
	return -EINVAL;
    }
}

static int shm_open(struct inode* inode, struct file* filp)
{
    // no valid segment associated yet
    // this is set in a successful 
    // IOC_SHM_CREATE or IOC_SHM_ATTACH ioctl
    filp->private_data = (void *) -1; 

    dbg("");
    nopen++;
    if (nopen > maxopen)
	maxopen = nopen;
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

int free_segments(int warn)
{
    int n;
    struct shm_segment *seg;
    int fail = 0;

    info("open=%d alloced=%dK freed=%dK balance=%dK\n", 
	 nopen, 
	 allocated >> 10, freed >> 10, (allocated-freed) >> 10);

    for (n = 0; n < nseg; n++) {
	seg = &shm_segments[n];
	if (seg->in_use) {
	    if (seg->n_uattach) {
		if (warn)
		    err("segment %d still in use by a user process uattach=%d",
			n, seg->n_uattach);
		fail++;
		continue;
	    }
	    if (seg->n_kattach) {
		// a programming error - kmods should properly release segs
		// on exit; but dont fail hard here because if we can unload
		// the using code must have already unloaded too
		if (warn)
		    err("segment %d still in use by a kernel module kattach=%d",
			n, seg->n_kattach);
		continue;
	    }
	    shm_free(seg);
	    info("segment %d deleted",n);
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
	seg->flags = 0;
	seg->creator = -1;
	seg->kmem = 0;
    }
}

// the in-kernel exported API:

int shmdrv_status(struct shm_status *shmstat) 
{
    struct shm_segment *seg;
    int ret = 0, segno;

    spin_lock(&ll_lock);
    if ((segno = find_shm_by_key(shmstat->key)) < 0) {
	err("no such segment key=0x%8.8x", shmstat->key);
	ret = -EINVAL;
	goto done;
    }
    seg = &shm_segments[segno];
    shmstat->id = segno;
    shmstat->n_kattach = seg->n_kattach;
    shmstat->n_uattach = seg->n_uattach;
    shmstat->size = seg->size;
    shmstat->act_size = seg->act_size;
    shmstat->creator = seg->creator;
    shmstat->flags = seg->flags;

 done:
    spin_unlock(&ll_lock);
    return ret;
}
EXPORT_SYMBOL(shmdrv_status);

int shmdrv_attach_pid(struct shm_status *shmstat, void **shm, int pid)
{
    struct shm_segment *seg;
    int ret;

    spin_lock(&ll_lock);
    ret = find_shm_by_key(shmstat->key);
    if (ret < 0) {
	err("shm segment does not exist: key=0x%8.8x ret=%d", 
	    shmstat->key, ret);
	goto done;
    }
    seg = &shm_segments[ret];
    shmstat->id = ret;

    // dont increment n_uattach, this will be handled during mmap()
    if (!pid) 
	seg->n_kattach++;
    if (shm)
	*shm = seg->kmem;
    ret = 0;
 done:
    spin_unlock(&ll_lock);
    return ret;
}

static int shmdrv_create_pid(struct shm_status *shmstat, int pid)
{
    struct shm_segment *seg;
    int ret = 0, segno;

    spin_lock(&ll_lock);
    if (shmstat->size <= 0) {
	err("invalid size %d", shmstat->size);
	ret = -EINVAL;
    }
    segno = find_shm_by_key(shmstat->key);
    if (segno > -1) {
	err("shm segment exists: key=0x%8.8x pos=%d", shmstat->key, segno);	
	ret = -EINVAL;
	goto done;
    }
    segno = find_free_shm_lot();
    if (segno < 0) {
	err("all slots full, use larger nseg param (%d)", nseg);
	ret = -EINVAL;
	goto done;
    }
    seg = &shm_segments[segno];
    seg->creator = pid;
    seg->in_use = 1;
    seg->key = shmstat->key;
    seg->n_kattach = 0;
    seg->n_uattach = 0;
    seg->size = shmstat->size;
    seg->flags = shmstat->flags;
    if (seg->flags & OUTBOARD_MASK) {
	// a segment type requiring special massage
	ret = shm_nonstandard_attach(seg);
    } else {
	// vanilla shm segment

	ret = shm_malloc(seg);
	if (!seg->kmem)
	    ret = -ENOMEM;
    }
    if (ret) {
	err("IOC_SHM_CREATE: shm_malloc fail size=%d key=0x%8.8x", 
	    seg->size, seg->key);
	goto done;
    }
    ret = segno;

 done:
    spin_unlock(&ll_lock);
    return ret;
}

// in-kernel API sets pid=0
int shmdrv_create(struct shm_status *shmstat)
{
    return shmdrv_create_pid(shmstat, 0);
}
EXPORT_SYMBOL(shmdrv_create);


int shmdrv_attach(struct shm_status *shmstat, void **shm)
{
    return shmdrv_attach_pid(shmstat, shm, 0);
}
EXPORT_SYMBOL(shmdrv_attach);

// kernel-only; userland refcount is taken care in munmap

int shmdrv_detach(struct shm_status *shmstat)
{
    struct shm_segment *seg;
    int ret = 0, segno;

    spin_lock(&ll_lock);
    segno = find_shm_by_key(shmstat->key);
    if (segno < 0) {
	err("shm segment does not exist: key=0x%8.8x",
	    shmstat->key);
	ret = -EINVAL;
	goto done;
    }
    seg = &shm_segments[segno];
    if (seg->n_kattach == 0) {
	err("shm segment not attached in-kernel: 0x%8.8x",
	    shmstat->key);
	ret = -EINVAL;
	goto done;
    }
    seg->n_kattach--;
    shmstat->id = segno;
    shmstat->n_kattach = seg->n_kattach;
    shmstat->n_uattach = seg->n_uattach;

    if (gc && ((seg->n_uattach + seg->n_kattach) == 0)) {
	// last reference gone away, gc true 
	if (seg->flags & OUTBOARD_MASK) {
	    // a segment type requiring special massage
	    ret = shm_nonstandard_detach(seg);
	} else {
	    // vanilla shm segment
	    shm_free(seg);
	    ret = 0;
	}
	info("gc segment %d key=0x%8.8x", segno, seg->key);
	seg->in_use = 0;
    }
 done:
    spin_unlock(&ll_lock);
    return ret;
}
EXPORT_SYMBOL(shmdrv_detach);


static long shm_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0, segno;
    struct shm_status sm; 
    struct shm_segment *seg;
    unsigned long irqState;

    spin_lock_irqsave(&shm_lock, irqState);

    switch(cmd) {

    case IOC_SHM_STATUS:
	ret = copy_from_user(&sm, (char *)arg, sizeof(struct shm_status));
	if (ret < 0) {
	    err("IOC_SHM_STATUS: copy_from_user %d", ret);
	    goto done;
	}
	ret = shmdrv_status(&sm);
	if (ret < 0) {
	    err("IOC_SHM_STATUS: segemnt 0x%8.8x does not exist", sm.key);
	    goto done;
	}

	ret = copy_to_user((char *)arg, &sm, sizeof(struct shm_status));
	if (ret < 0) {
	    err("IOC_SHM_STATUS: copy_to_user %d", ret);
	}
	break;

    case IOC_SHM_CREATE:
	ret = copy_from_user(&sm, (char *)arg, sizeof(struct shm_status));
	if (ret < 0) {
	    err("IOC_SHM_CREATE: copy_from_user %d", ret);
 	    goto done;
	}
	segno = shmdrv_create_pid(&sm, current->pid);
	if (segno < 0) {
	    err("IOC_SHM_CREATE: shmdrv_create_pid fail key=0x%8.8x size=%d", 
		sm.key,sm.size);
	    goto done;
	}
	seg = &shm_segments[segno];
	file->private_data = (void *) segno; // record shm id for ensuing mmap

	sm.id = segno;
	sm.n_kattach = seg->n_kattach;
	sm.n_uattach = seg->n_uattach;
	sm.act_size = seg->act_size;
	sm.creator = seg->creator;

	ret = copy_to_user((char *)arg, &sm, sizeof(struct shm_status));
	if (ret < 0) {
	    err("IOC_SHM_CREATE: copy_to_user %d", ret);
	}
	info("created seg %d size %d key 0x%8.8x at %p",
	    segno, seg->size, seg->key, seg->kmem);
	ret =  segno;
	break;

    case IOC_SHM_ATTACH:

	ret = copy_from_user(&sm, (char *)arg, sizeof(struct shm_status));
	if (ret < 0) {
	    err("IOC_SHM_ATTACH: copy_from_user %d", ret);
	    goto done;
	}

	ret = shmdrv_attach_pid(&sm, NULL, current->pid);
	if (ret < 0) {
	    err("IOC_SHM_ATTACH: shm segment does not exist: key=0x%8.8x ret=%d", sm.key, ret);
	    goto done;
	}
	ret = copy_to_user((char *)arg, &sm, sizeof(struct shm_status));
	if (ret < 0) {
	    err("IOC_SHM_ATTACH: copy_to_user %d", ret);
	    goto done;
	}
	file->private_data = (void *) sm.id; // record shm id for ensuing mmap
	seg = &shm_segments[sm.id];
	info("attached seg %d size %d key 0x%8.8x key at %p",
	    sm.id, seg->size, seg->key, seg->kmem);
	ret = 0;
	break;

    case IOC_SHM_GC:
	ret = free_segments(0);
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
		     "%d segment(s), open=%d u=%d k=%d total=%d aligned=%d alloced=%dK freed=%dK balance=%dK\n", 
		     nsegments, nopen, uattach, kattach, total_alloc, total_alloc_aligned, 
		     allocated >> 10, freed >> 10, (allocated-freed) >> 10);
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
			    "%d: key=0x%8.8x size=%d aligned=%d ul=%d k=%d creator=%d mem=%p\n",
			     i, seg->key, seg->size, seg->act_size, 
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

    spin_lock_init(&ll_lock);
    spin_lock_init(&shm_lock);

    return 0;
}

static void shmdrv_exit(void) 
{
    int ret;

    dbg("");

    ret = free_segments(1);
    if (ret) {
	err("%d segments still in use", ret);
    }
    if (shm_segments)
        kfree(shm_segments);

    misc_deregister(&shm_misc_dev);
}

module_init(shmdrv_init);
module_exit(shmdrv_exit);
