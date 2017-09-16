/*************************************************************************

 Copyright (C) 2007 John Kasunich (jmkasunich at fastmail dot fm)

 This library contains functions to scan the PCI bus, find a particular
 PCI card, and read or write memory and I/O locations on a PCI card.

**************************************************************************

This program is free software; you can redistribute it and/or
modify it under the terms of version 2 of the GNU General
Public License as published by the Free Software Foundation.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
harming persons must have provisions for completely removing power
from all motors, etc, before persons enter any danger area.  All
machinery must be designed to comply with local and national safety
codes, and the authors of this software can not, and do not, take
any responsibility for such compliance.

This code was written as part of the EMC HAL project.  For more
information, go to www.linuxcnc.org.

*************************************************************************/

#define _GNU_SOURCE /* getline() */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/io.h>
#include <sys/mman.h>
#include <linux/types.h>
#include "upci.h"

/************************************************************************/

/* Assorted typedefs */

/* this is an standarized struct that is present in the
   configuration space of every PCI card */
struct cfg_info {
    __u16 vendor_id;
    __u16 device_id;
    __u16 command;
    __u16 status;
    __u32 class_rev;
    __u32 misc_0;
    __u32 base[6];
    __u32 cardbus;
    __u16 ss_vendor_id;
    __u16 ss_device_id;
    __u32 rom_addr;
    __u32 reserved[2];
    __u32 misc_1;
};

/* internal structs used by upci */
struct region_info {
    enum upci_region_type type;
    __u32 base_addr;
    __u32 size;
    int region_is_mapped;
    void *mapped_ptr;
};

struct dev_info {
    struct upci_dev_info p;		/* public info */
    struct cfg_info cfg;		/* raw data from device */
    struct region_info regions[6];	/* region info */
};

/************************************************************************/

#define MAX_DEVICES 100
#define MAX_REGIONS (MAX_DEVICES*6)

/* we get our info from the proc filesystem */
#define DEVICES_FILE	"/proc/bus/pci/devices"
#define DEVICE_FILE	"/proc/bus/pci/%02x/%02x.%01x"

/* number of discovered devices */
static int num_devs = 0;
/* pointers to all discovered devices (others are null) */
static struct dev_info *devices[MAX_DEVICES] = { NULL };
/* pointers to all mapped regions (others are null) */
static struct region_info *regions[MAX_REGIONS] = { NULL };
/* file descriptor for /dev/mem */
static int memfd = -1;
/* reference counters, increment each time a region is opened */
static int memaccess = 0;	/* non-zero if /dev/mem is open */
static int ioaccess = 0;	/* non-zero if iopl > 0 */

/************************************************************************/

static void errmsg(const char *funct, const char *fmt, ...)
{
    va_list vp;

    va_start(vp, fmt);
    fprintf(stderr, "ERROR in %s(): ", funct);
    vfprintf(stderr, fmt, vp);
    fprintf(stderr, "\n");
    va_end(vp);
}

void upci_reset(void)
{
    int d, r;
    struct dev_info *dev;
    struct region_info *region;

    /* traverse list */
    for ( d = 0 ; d < num_devs ; d++ ) {
	dev = devices[d];
	if ( dev != NULL ) {
	    for ( r = 0 ; r < 6 ; r++ ) {
		region = &(dev->regions[r]);
		if ( region->region_is_mapped ) {
		    /* unmap any mapped memory regions */
		    munmap ( region->mapped_ptr, region->size );
		}
	    }
	    /* free data structures */
	    free(dev);
	    devices[d] = NULL;
	}
    }
    /* release I/O port access */
    if ( ioaccess ) {
	iopl(0);
	ioaccess = 0;
    }
    /* release raw memory access */
    if ( memaccess ) {
	close (memfd);
	memaccess = 0;
    }
    num_devs = 0;
}

int upci_scan_bus(void)
{
    FILE *f;
    char *lineptr;
    size_t linelen;
    int linenum, r, n;
    struct dev_info *dev, *ndev;
    struct cfg_info cfg;
    __u32 vendordev;
    __u16 busdevfunc;
    char dev_fname[256];
    int fd;

    /* delete any previous list */
    upci_reset();
    /* we use /proc as the source of all knowledge */
    f = fopen(DEVICES_FILE, "r");
    if (f == NULL) {
	errmsg(__func__, "opening '%s': %s", DEVICES_FILE, strerror(errno));
	goto cleanup1;
    }
    lineptr = NULL;
    linelen = 0;
    linenum = 0;
    while ( 1 ) {
	if ( num_devs == MAX_DEVICES ) {
	    errmsg(__func__, "devices list full (%d entries)", num_devs);
	    /* return the ones that we already found, ignore others */
	    free(lineptr);
	    fclose(f);
	    return num_devs;
	}
	/* get a line */
	if(getline(&lineptr, &linelen, f) < 0) {
	    /* end of file, done */
	    free(lineptr);
	    fclose(f);
	    return num_devs;
	}
	++linenum;
	/* allocate a structure */
	dev = (struct dev_info *)(malloc(sizeof(struct dev_info)));
	if ( dev == NULL ) {
	    errmsg(__func__,"malloc failure");
	    goto cleanup2;
	}
	devices[num_devs++] = dev;
	n = sscanf(lineptr,
	    "%hx %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
	    &busdevfunc, &vendordev, &(dev->p.irq),
	    &dev->p.base_addr[0],  &dev->p.base_addr[1], &dev->p.base_addr[2],
	    &dev->p.base_addr[3],  &dev->p.base_addr[4], &dev->p.base_addr[5],
	    &dev->p.rom_base_addr,
	    &dev->p.size[0], &dev->p.size[1], &dev->p.size[2],
	    &dev->p.size[3], &dev->p.size[4], &dev->p.size[5],
	    &dev->p.rom_size);
	if (n != 17) {
	    errmsg(__func__, "only %d items on %s line %d", n, DEVICES_FILE, linenum);
	    goto cleanup3;
	}
	/* separate the packed fields */
	dev->p.vendor_id = (vendordev >> 16) & 0xFFFF;
	dev->p.device_id =  vendordev & 0xFFFF;
	dev->p.bus = (busdevfunc >> 8) & 0x00FF;
	dev->p.dev = (busdevfunc >> 3) & 0x001F;
	dev->p.func = busdevfunc & 0x0007;
	/* fix up region info: remove embedded info in low bits,
	   copy to region_info structs, init various fields */
	for ( r = 0 ; r < 6 ; r++ ) {
	    if ( dev->p.base_addr[r] & 1 ) {
		/* region is I/O */
		dev->p.region_type[r] = UPCI_REG_IO;
		dev->p.base_addr[r] &= ~0x03;
	    } else {
		/* region is memory */
		dev->p.region_type[r] = UPCI_REG_MEM;
		dev->p.base_addr[r] &= ~0x0F;
	    }
	    dev->regions[r].type = dev->p.region_type[r];
	    dev->regions[r].base_addr = dev->p.base_addr[r];
	    dev->regions[r].size = dev->p.size[r];
	    dev->regions[r].region_is_mapped = 0;
	    dev->regions[r].mapped_ptr = NULL;
	}
	/* the subsystem info isn't stored in DEVICES_FILE, need to
	   look deeper to find it */
	snprintf(dev_fname, 255, DEVICE_FILE, dev->p.bus, dev->p.dev, dev->p.func);
	fd = open(dev_fname, O_RDONLY);
	if (fd < 0) {
	    errmsg(__func__, "opening '%s': %s", dev_fname, strerror(errno));
	    goto cleanup3;
	}
	n = read(fd, &cfg, sizeof(struct cfg_info));
	if ( n != sizeof(struct cfg_info) ) {
	    errmsg(__func__, "reading '%s': %s", dev_fname, strerror(errno));
	    goto cleanup4;
	}
	close(fd);
	dev->p.ss_vendor_id = cfg.ss_vendor_id;
	dev->p.ss_device_id = cfg.ss_device_id;
	dev->p.instance = 0;
	/* look for previous devices with the same vendor, device,
	   and subsystem codes.  start at end of list and work forward */
	n = num_devs - 2;
	while ( n >= 0 ) {
	    ndev = devices[n];
	    if (( dev->p.vendor_id == ndev->p.vendor_id ) &&
		( dev->p.device_id == ndev->p.device_id ) &&
		( dev->p.ss_vendor_id == ndev->p.ss_vendor_id ) &&
		( dev->p.ss_device_id == ndev->p.ss_device_id )) {
		/* found a match */
		dev->p.instance = ndev->p.instance + 1;
		/* don't need to look farther */
		break;
	    }
	    n--;
	}
    }
    /* loop never falls through, success returns from inside the loop */
    /* errors jump here */
cleanup4:
    close(fd);
cleanup3:
    free(dev);
    num_devs --;
cleanup2:
    free(lineptr);
    fclose(f);
cleanup1:
    upci_reset();
    return -1;
}

int upci_print_device_info(int devnum)
{
    struct upci_dev_info *dev;
    int n;

    if (( devnum < 0 ) || ( devnum >= num_devs )) {
	errmsg(__func__, "device %d not found", devnum);
	return -1;
    }
    dev = &(devices[devnum]->p);
    printf("PCI device %2d at bus/device/function %02x/%02x/%01x\n",
	devnum, dev->bus, dev->dev, dev->func );
    printf("Vendor/Device/SSVendor/SSDevice: %04x/%04x/%04x/%04x\n",
	dev->vendor_id, dev->device_id, dev->ss_vendor_id, dev->ss_device_id );
    for ( n = 0 ; n < 6 ; n++ ) {
	if ( dev->size[n] > 0 ) {
	    if ( dev->region_type[n] == UPCI_REG_IO ) {
		/* I/O region */
		printf("Region %d: I/O %u bytes at %04x\n",
		    n, dev->size[n], dev->base_addr[n] );
	    } else {
		/* memory region */
		printf("Region %d: MEM %u bytes at %08x\n",
		    n, dev->size[n], dev->base_addr[n] );
	    }
	}
    }
    return 0;
}

int upci_get_device_info(struct upci_dev_info *p, int devnum)
{
    if (( devnum < 0 ) || ( devnum >= num_devs )) {
	return -1;
    }
    if ( p == NULL ) {
	return -1;
    }
    /* copy from our private structure into the user's one */
    *p = devices[devnum]->p;
    return 0;
}

int upci_find_device(struct upci_dev_info *p)
{
    int n;
    struct dev_info *dev;

    n = 0;
    while ( n < num_devs ) {
	dev = devices[n];
	if (( dev->p.vendor_id == p->vendor_id ) &&
	    ( dev->p.device_id == p->device_id ) &&
	    ( dev->p.ss_vendor_id == p->ss_vendor_id ) &&
	    ( dev->p.ss_device_id == p->ss_device_id ) &&
	    ( dev->p.instance == p->instance )) {
	    /* found a match */
	    return n;
	}
	n++;
    }
    return -1;
}

static int incr_io_usage ( void )
{
    int retval, eno;

    /* make sure we can do I/O */
    if ( ioaccess == 0 ) {
	/* enable access */
	/* this needs priviliges */
	if (seteuid(0) != 0) {
	    errmsg(__func__, "need root privilges (or setuid root)");
	    return -1;
	}
	/* do it */
	retval = iopl(3);
	eno = errno;
	/* drop priviliges */
	if(seteuid(getuid()) != 0)
	{
	    errmsg(__func__, "unable to drop root privileges");
	    /* Don't continue past this point, because following code may
	     * execute with unexpected privileges
	     */
	    _exit(99);
	}
	/* check result */
	if(retval < 0 || iopl(3) < 0) {
	    errmsg(__func__,"opening I/O ports: %s", strerror(eno));
	    return -1;
	}
    }
    /* increment reference count */
    ioaccess++;
    return 0;
}

static void decr_io_usage ( void )
{
    if ( ioaccess <= 0 ) {
	/* should not be calling decr if already zero */
	return;
    }
    /* decrement reference count */
    ioaccess--;
    if ( ioaccess == 0 ) {
	/* release I/O priveleges */
	iopl(0);
    }
}

static int incr_mem_usage ( void )
{
    int eno;

    /* make sure /dev/mem is open */
    if ( memaccess == 0 ) {
	/* open it */
	/* this needs priviliges */
	if (seteuid(0) != 0) {
	    errmsg(__func__, "need root privilges (or setuid root)");
	    return -1;
	}
	/* do it */
	memfd = open("/dev/mem", O_RDWR);
	eno = errno;
	/* drop priviliges */
	if(seteuid(getuid()) != 0)
	{
	    errmsg(__func__, "unable to drop root privileges");
	    /* Don't continue past this point, because following code may
	     * execute with unexpected privileges
	     */
	    _exit(99);
	}
	/* check result */
	if ( memfd < 0 ) {
	    errmsg(__func__,"can't open /dev/mem: %s", strerror(eno));
	    return -1;
	}
    }
    /* increment reference count */
    memaccess++;
    return 0;
}

static void decr_mem_usage ( void )
{
    if ( memaccess <= 0 ) {
	/* should not be calling decr if already zero */
	return;
    }
    /* decrement reference count */
    memaccess--;
    if ( memaccess == 0 ) {
	/* close /dev/mem */
	close(memfd);
    }
}

int upci_open_region(int devnum, int region_num)
{
    struct dev_info *dev;
    struct region_info *reg;
    int rd;

    if (( devnum < 0 ) || ( devnum >= num_devs )) {
	errmsg(__func__, "device %d not found", devnum);
	return -1;
    }
    dev = devices[devnum];
    if (( region_num < 0 ) || ( region_num >= 6 )) {
	errmsg(__func__, "bad region number %d", region_num);
	return -1;
    }
    reg = &(dev->regions[region_num]);
    if ( reg->size == 0 ) {
	errmsg(__func__, "region %d not found in device %d",
	    region_num, devnum);
	return -1;
    }
    /* calculate region descriptor */
    rd = devnum * 6 + region_num;
    if ( reg->region_is_mapped ) {
	/* already open, nothing to do */
	return rd;
    }
    if ( reg->type == UPCI_REG_IO ) {
	/* region is IO, no mapping needed */
	/* update reference counter */
	if ( incr_io_usage() != 0 ) {
	    errmsg(__func__, "no I/O access");
	    return -1;
	}
    } else {
	/* region is memory */
	/* update reference counter */
	if ( incr_mem_usage() != 0 ) {
	    return -1;
	}
	/* map the memory region */
	reg->mapped_ptr = mmap(0, reg->size, PROT_READ | PROT_WRITE,
	    MAP_SHARED, memfd, reg->base_addr);
	if ( reg->mapped_ptr == MAP_FAILED ) {
	    reg->mapped_ptr = NULL;
	    errmsg(__func__, "can't map /dev/mem for device %d, region %d: %s",
		devnum, region_num, strerror(errno));
	    decr_mem_usage();
	    return -1;
	}
    }
    /* mark region as mapped */
    reg->region_is_mapped = 1;
    regions[rd] = reg;
    return rd;
}

void upci_close_region(int rd)
{
    struct region_info *reg;

    if ((rd < 0 ) || ( rd >= MAX_REGIONS )) return;
    reg = regions[rd];
    if ( reg == NULL ) return;
    if ( reg->type == UPCI_REG_IO ) {
	/* region is IO, nothing to unmap */
	decr_io_usage();
    } else {
	/* region is memory, unmap it */
	munmap ( reg->mapped_ptr, reg->size );
	decr_mem_usage();
    }
    /* mark region as unmapped */
    reg->region_is_mapped = 0;
    regions[rd] = NULL;
}

__u8 upci_read_u8(int rd, __u32 offset)
{
    struct region_info *reg;
    int port;
    volatile __u8 *ptr, data;

    /* test for out of range, not mapped, or offset beyond end of region */
    if ((rd < 0 ) || ( rd >= MAX_REGIONS ) || 
	( (reg = regions[rd]) == NULL ) || ( offset > reg->size )) return 0;
    /* get the data */
    if ( reg->type == UPCI_REG_IO ) {
	/* region is IO */
	port = reg->base_addr + offset;
	data = inb(port);
    } else {
	/* region is memory */
	ptr = (__u8 *)(reg->mapped_ptr + offset);
	data = *ptr;
    }
    return data;
}

__s8 upci_read_s8(int rd, __u32 offset)
{
    struct region_info *reg;
    int port;
    volatile __s8 *ptr, data;

    /* test for out of range, not mapped, or offset beyond end of region */
    if ((rd < 0 ) || ( rd >= MAX_REGIONS ) ||
	( (reg = regions[rd]) == NULL ) || ( offset > reg->size )) return 0;
    /* get the data */
    if ( reg->type == UPCI_REG_IO ) {
	port = reg->base_addr + offset;
	data = inb(port);
    } else {
	ptr = (__s8 *)(reg->mapped_ptr + offset);
	data = *ptr;
    }
    return data;
}

__u16 upci_read_u16(int rd, __u32 offset)
{
    struct region_info *reg;
    int port;
    volatile __u16 *ptr, data;

    /* test for out of range, not mapped, or offset beyond end of region */
    if ((rd < 0 ) || ( rd >= MAX_REGIONS ) ||
	( (reg = regions[rd]) == NULL ) || ( offset > reg->size )) return 0;
    /* get the data */
    if ( reg->type == UPCI_REG_IO ) {
	port = reg->base_addr + offset;
	data = inw(port);
    } else {
	ptr = (__u16 *)(reg->mapped_ptr + offset);
	data = *ptr;
    }
    return data;
}

__s16 upci_read_s16(int rd, __u32 offset)
{
    struct region_info *reg;
    int port;
    volatile __s16 *ptr, data;

    /* test for out of range, not mapped, or offset beyond end of region */
    if ((rd < 0 ) || ( rd >= MAX_REGIONS ) ||
	( (reg = regions[rd]) == NULL ) || ( offset > reg->size )) return 0;
    /* get the data */
    if ( reg->type == UPCI_REG_IO ) {
	port = reg->base_addr + offset;
	data = inw(port);
    } else {
	ptr = (__s16 *)(reg->mapped_ptr + offset);
	data = *ptr;
    }
    return data;
}

__u32 upci_read_u32(int rd, __u32 offset)
{
    struct region_info *reg;
    int port;
    volatile __u32 *ptr, data;

    /* test for out of range, not mapped, or offset beyond end of region */
    if ((rd < 0 ) || ( rd >= MAX_REGIONS ) ||
	( (reg = regions[rd]) == NULL ) || ( offset > reg->size )) return 0;
    /* get the data */
    if ( reg->type == UPCI_REG_IO ) {
	port = reg->base_addr + offset;
	data = inl(port);
    } else {
	ptr = (__u32 *)(reg->mapped_ptr + offset);
	data = *ptr;
    }
    return data;
}

__s32 upci_read_s32(int rd, __u32 offset)
{
    struct region_info *reg;
    int port;
    volatile __s32 *ptr, data;

    /* test for out of range, not mapped, or offset beyond end of region */
    if ((rd < 0 ) || ( rd >= MAX_REGIONS ) ||
	( (reg = regions[rd]) == NULL ) || ( offset > reg->size )) return 0;
    /* get the data */
    if ( reg->type == UPCI_REG_IO ) {
	port = reg->base_addr + offset;
	data = inl(port);
    } else {
	ptr = (__s32 *)(reg->mapped_ptr + offset);
	data = *ptr;
    }
    return data;
}

void upci_write_u8(int rd, __u32 offset, __u8 data)
{
    struct region_info *reg;
    int port;
    volatile __u8 *ptr;

    /* test for out of range, not mapped, or offset beyond end of region */
    if ((rd < 0 ) || ( rd >= MAX_REGIONS ) || 
	( (reg = regions[rd]) == NULL ) || ( offset > reg->size )) return;
    /* write the data */
    if ( reg->type == UPCI_REG_IO ) {
	/* region is IO */
	port = reg->base_addr + offset;
	outb(data, port);
    } else {
	/* region is memory */
	ptr = (__u8 *)(reg->mapped_ptr + offset);
	*ptr = data;
    }
    return;
}

void upci_write_s8(int rd, __u32 offset, __s8 data)
{
    struct region_info *reg;
    int port;
    volatile __s8 *ptr;

    /* test for out of range, not mapped, or offset beyond end of region */
    if ((rd < 0 ) || ( rd >= MAX_REGIONS ) ||
	( (reg = regions[rd]) == NULL ) || ( offset > reg->size )) return;
    /* write the data */
    if ( reg->type == UPCI_REG_IO ) {
	port = reg->base_addr + offset;
	outb(data, port);
    } else {
	ptr = (__s8 *)(reg->mapped_ptr + offset);
	*ptr = data;
    }
    return;
}

void upci_write_u16(int rd, __u32 offset, __u16 data)
{
    struct region_info *reg;
    int port;
    volatile __u16 *ptr;

    /* test for out of range, not mapped, or offset beyond end of region */
    if ((rd < 0 ) || ( rd >= MAX_REGIONS ) ||
	( (reg = regions[rd]) == NULL ) || ( offset > reg->size )) return;
    /* write the data */
    if ( reg->type == UPCI_REG_IO ) {
	port = reg->base_addr + offset;
	outw(data, port);
    } else {
	ptr = (__u16 *)(reg->mapped_ptr + offset);
	*ptr = data;
    }
    return;
}

void upci_write_s16(int rd, __u32 offset, __s16 data)
{
    struct region_info *reg;
    int port;
    volatile __s16 *ptr;

    /* test for out of range, not mapped, or offset beyond end of region */
    if ((rd < 0 ) || ( rd >= MAX_REGIONS ) ||
	( (reg = regions[rd]) == NULL ) || ( offset > reg->size )) return;
    /* write the data */
    if ( reg->type == UPCI_REG_IO ) {
	port = reg->base_addr + offset;
	outw(data, port);
    } else {
	ptr = (__s16 *)(reg->mapped_ptr + offset);
	*ptr = data;
    }
    return;
}

void upci_write_u32(int rd, __u32 offset, __u32 data)
{
    struct region_info *reg;
    int port;
    volatile __u32 *ptr;

    /* test for out of range, not mapped, or offset beyond end of region */
    if ((rd < 0 ) || ( rd >= MAX_REGIONS ) ||
	( (reg = regions[rd]) == NULL ) || ( offset > reg->size )) return;
    /* write the data */
    if ( reg->type == UPCI_REG_IO ) {
	port = reg->base_addr + offset;
	outl(data, port);
    } else {
	ptr = (__u32 *)(reg->mapped_ptr + offset);
	*ptr = data;
    }
    return;
}

void upci_write_s32(int rd, __u32 offset, __s32 data)
{
    struct region_info *reg;
    int port;
    volatile __s32 *ptr;

    /* test for out of range, not mapped, or offset beyond end of region */
    if ((rd < 0 ) || ( rd >= MAX_REGIONS ) ||
	( (reg = regions[rd]) == NULL ) || ( offset > reg->size )) return;
    /* write the data */
    if ( reg->type == UPCI_REG_IO ) {
	port = reg->base_addr + offset;
	outl(data, port);
    } else {
	ptr = (__s32 *)(reg->mapped_ptr + offset);
	*ptr = data;
    }
    return;
}
