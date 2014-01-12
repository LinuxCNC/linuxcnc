/********************************************************************
* Description:  rtapi_pci.c
*               This file, 'rtapi_pci.c', implements the 
*               usermode PCI functions 
*
*
* Copyright (C) 2009 - 2013 Michael Büsch <m AT bues DOT CH>, 
*                           Charles Steinkuehler <charles AT steinkuehler DOT net>
*                           John Morris <john AT zultron DOT com>
*                           Michael Haberler <license AT mah DOT priv DOT at>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
********************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "config.h"
#if defined(USERMODE_PCI) && defined(BUILD_SYS_USER_DSO)

#include <libudev.h>
#include <stdio.h>		/* vprintf() */
#include <stdlib.h>		/* malloc(), sizeof() */
#include <stddef.h>		/* offsetof */
#include <unistd.h>		/* usleep() */
#include <sys/io.h>		/* shmget() */
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <time.h>

#include "rtapi.h"
#include "rtapi_pci.h"
#include "rtapi_common.h"

/***********************************************************************
*                        PCI DEVICE SUPPORT                            *
************************************************************************/

struct rtapi_pcidev_mmio {
	int bar;			/* The PCI BAR */
	int fd;				/* sysfs_pci resourceN file descriptor */
	void *mmio;			/* The MMIO address */
	size_t length;			/* Length of the mapping */
};

struct rtapi_pcidev {
	__u16 vendor;				/* Vendor ID */
	__u16 device;				/* Device ID */
	char busid[32];				/* PCI bus ID (0000:00:00.0) */
	char devnode[32];			/* UIO /dev node */
	int fd;					/* UIO /dev file descriptor */
	int mmio_refcnt;			/* MMIO resource reference count */
	struct rtapi_pcidev_mmio mmio[8];	/* Mappings */
};

/*---------------------------------------------------------------------**
** Per sysfs-rules.txt, we should be checking more than one location:  **
**   /sys/subsystem/                                                   **
**   /sys/class/                                                       **
**   /sys/bus/                                                         **
**---------------------------------------------------------------------*/
#define UIO_PCI_PATH	"/sys/bus/pci/drivers/uio_pci_generic"

//Fixme//  Nasty global variable...only one board for now.
struct pci_dev *one_dev = NULL;

static ssize_t readfile(const char *path, char *buf, size_t buflen)
{
	int fd;
	ssize_t res;

	fd = open(path, O_RDONLY);
	if (!fd) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"Failed to open \"%s\" (%s)\n",
				path, strerror(errno));
		return -1;
	}
	memset(buf, 0, buflen);
	res = read(fd, buf, buflen - 1);
	close(fd);
	if (res < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"Failed to read \"%s\" (%s)\n",
				path, strerror(errno));
		return -1;
	}

	return res;
}

static int writefile(const char *path, const char *buf, size_t len)
{
	int fd;
	ssize_t res;

	fd = open(path, O_WRONLY);
	if (!fd) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"Failed to open \"%s\" (%s)\n",
				path, strerror(errno));
		return -1;
	}
	res = write(fd, buf, len);
	close(fd);
	if (res != len) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"Failed to write \"%s\" (%s)\n",
				path, strerror(errno));
		return -1;
	}

	return 0;
}

struct rtapi_pcidev * rtapi_pci_get_device(__u16 vendor, __u16 device,
					   struct rtapi_pcidev *from)
{
	int err;
	DIR *dir;
	struct dirent dirent_buf, *dirent;
	ssize_t res;
	char buf[256], path[256];
	struct rtapi_pcidev *dev = NULL;
	int found_start = 0;

	/* Register the new device IDs */
	snprintf(buf, sizeof(buf), "%04X %04X", vendor, device);
	err = writefile(UIO_PCI_PATH "/new_id", buf, strlen(buf));
	if (err) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"Failed to register PCI device to UIO-pci-generic. "
				"Is the \"uio-pci-generic\" kernel module loaded?\n");
		goto error;
	}

rtapi_print_msg(RTAPI_MSG_ERR,
		"RTAPI_PCI: Registered device (%s)\n", buf);

	/* UIO-pci-generic should bind to the device now. Wait to avoid races. */
usleep(1000);
/*	{
		struct timespec time = {
			.tv_sec = 0,
			.tv_nsec = 1000 * 1000 * 100,
		};
		do {
			err = nanosleep(&time, &time);
			if (err && errno != EINTR) {
				rtapi_print_msg(RTAPI_MSG_ERR,
						"Nanosleep failure (%s)\n",
						strerror(errno));
				break;
			}
		} while (time.tv_sec || time.tv_nsec);
	}
*/
rtapi_print_msg(RTAPI_MSG_ERR,
		"RTAPI_PCI: Finished waiting.\n");

	/* Find the device */
	dir = opendir(UIO_PCI_PATH);
	if (!dir) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"Failed to open UIO-pci-generic sysfs directory. (%s)\n",
				strerror(errno));
		goto error;
	}
	while (1) {
		unsigned int tmp;

		err = readdir_r(dir, &dirent_buf, &dirent);
		if (err) {
			rtapi_print_msg(RTAPI_MSG_ERR,
					"Failed to read UIO-pci-generic sysfs directory. (%s)\n",
					strerror(errno));
			closedir(dir);
			goto error;
		}
		if (from && !found_start) {
			if (strcmp(dirent->d_name, from->busid) == 0) {
				/* Found the start entry. */
				found_start = 1;
			}
			continue;
		}
		res = sscanf(dirent->d_name, "%X:%X:%X.%X", &tmp, &tmp, &tmp, &tmp);
		if (res != 4)
			continue;
		/* Check the vendor ID */
		snprintf(path, sizeof(path), UIO_PCI_PATH "/%s/vendor", dirent->d_name);
		res = readfile(path, buf, sizeof(buf));
		if (res <= 0)
			continue;
		res = sscanf(buf, "%X", &tmp);
		if (res != 1 || tmp != vendor)
			continue;
		/* Check the device ID */
		snprintf(path, sizeof(path), UIO_PCI_PATH "/%s/device", dirent->d_name);
		res = readfile(path, buf, sizeof(buf));
		if (res <= 0)
			continue;
		res = sscanf(buf, "%X", &tmp);
		if (res != 1 || tmp != device)
			continue;

		/* We got a device! */
		dev = malloc(sizeof(*dev));
		if (!dev) {
			rtapi_print_msg(RTAPI_MSG_ERR, "Out of memory\n");
			closedir(dir);
			goto error;
		}
		memset(dev, 0, sizeof(*dev));
		strncpy(dev->busid, dirent->d_name, sizeof(dev->busid) - 1);
		break;
	}
	closedir(dir);
	if (!dev) {
		rtapi_print_msg(RTAPI_MSG_INFO, "PCI device %04X:%04X not found\n",
				vendor, device);
		goto error;
	}

	dev->vendor = vendor;
	dev->device = device;

	/* Check which /dev node the device is on. */
	snprintf(path, sizeof(path), UIO_PCI_PATH "/%s/uio", dev->busid);
	dir = opendir(path);
	if (!dir) {
		rtapi_print_msg(RTAPI_MSG_ERR, "Failed to open UIO directory. (%s)\n",
				strerror(errno));
		goto error;
	}
	while (1) {
		unsigned int tmp;

		err = readdir_r(dir, &dirent_buf, &dirent);
		if (err) {
			rtapi_print_msg(RTAPI_MSG_ERR,
					"Failed to read UIO directory. (%s)\n",
					strerror(errno));
			closedir(dir);
			goto error;
		}
		res = sscanf(dirent->d_name, "uio%u", &tmp);
		if (res != 1)
			continue;
		snprintf(dev->devnode, sizeof(dev->devnode), "/dev/uio%u", tmp);
		break;
	}
	closedir(dir);
	if (strlen(dev->devnode) == 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "Could not determine UIO /dev node.\n");
		goto error;
	}

	/* Open the device node */
	dev->fd = open(dev->devnode, O_RDWR);
	if (dev->fd < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "Could not open UIO node \"%s\". (%s)\n",
				dev->devnode, strerror(errno));
		goto error;
	}

	return dev;

error:
	if (dev)
		free(dev);
	return NULL;
}

void rtapi_pci_put_device(struct rtapi_pcidev *dev)
{
	int err;
	char buf[256];

	if (!dev)
		return;

	if (dev->mmio_refcnt != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "Nonzero PCI MMIO reference count (%d). "
				"Check your ioremap/iounmap calls.\n", dev->mmio_refcnt);
	}

	/* Remove the device IDs */
	snprintf(buf, sizeof(buf), "%04X %04X", dev->vendor, dev->device);
	err = writefile(UIO_PCI_PATH "/remove_id", buf, strlen(buf));
	if (err) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"Failed to remove PCI device ID from UIO-pci-generic.\n");
	}

	/* Unbind the device from the UIO-pci-generic driver. */
	err = writefile(UIO_PCI_PATH "/unbind", dev->busid, strlen(dev->busid));
	if (err) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"Failed to unbind the PCI device %s\n", dev->busid);
	}

	close(dev->fd);

	memset(dev, 0, sizeof(*dev));
	free(dev);
}

void * rtapi_pci_ioremap(struct rtapi_pcidev *dev, int bar, size_t size)
{
	//size_t pagesize;
	void *mmio;
	char path[256];

	if (bar < 0 || bar >= sizeof(dev->mmio)) {
		rtapi_print_msg(RTAPI_MSG_ERR, "Invalid PCI BAR %d\n", bar);
		return NULL;
	}

	snprintf(path, sizeof(path), UIO_PCI_PATH "/%s/resource%i", dev->busid, bar);

	/* Open the resource node */
	dev->mmio[bar].fd = open(path, O_RDWR | O_SYNC);
	if (dev->mmio[bar].fd < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "Could not open UIO resource \"%s\". (%s)\n",
				path, strerror(errno));
		return NULL;
	}

	//pagesize = sysconf(_SC_PAGESIZE);

	mmio = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
		    dev->mmio[bar].fd, 0);

	if (mmio == NULL || mmio == MAP_FAILED) {
		if (mmio == NULL)
			munmap(mmio, size);
		rtapi_print_msg(RTAPI_MSG_ERR, "Failed to remap MMIO %d of PCI device %s\n",
				bar, dev->busid);
		return NULL;
	}

//	rtapi_print_msg(RTAPI_MSG_ERR, "%i: %lx.%lx %lx.%lx\n", i, 
//		*((unsigned long *) (mmio + 0xff0)),
//		*((unsigned long *) (mmio + 0xff4)),
//		*((unsigned long *) (mmio + 0xff8)),
//		*((unsigned long *) (mmio + 0xffc)) );

	dev->mmio[bar].bar = bar;
	dev->mmio[bar].mmio = mmio;
	dev->mmio[bar].length = size;
	dev->mmio_refcnt++;

	return mmio;
}

void rtapi_pci_iounmap(struct rtapi_pcidev *dev, void *mmio)
{
	unsigned int i;

	if (!dev || !mmio)
		return;

	for (i = 0; i < sizeof(dev->mmio); i++) {
		if (dev->mmio[i].mmio == mmio) {
			munmap(mmio, dev->mmio[i].length);
			memset(&dev->mmio[i], 0, sizeof(dev->mmio[i]));
			dev->mmio_refcnt--;
			return;
		}
	}
	rtapi_print_msg(RTAPI_MSG_ERR, "IO-unmap: Did not find PCI mapping %p", mmio);
}

/*----------------------------------------------------------**
** Register IDs with uio_pci_generic driver                 **
** Look for any boards                                      **
**   Find devcie entries under driver/uio_pci_generic       **
**   Read device details into local struct                  **
**   Verify details match device_id list from driver        **
**                                                          **
** Allocate ram and create pci_dev struct for each board    **
** Call driver probe function serially for each board found **
**----------------------------------------------------------*/
//Fixme// Global for testing only!
struct rtapi_pci_dev *dev_g = NULL;

static int check_device_id(struct udev_device *udev_dev, struct pci_device_id *dev_id, struct pci_driver *driver)
{
    ssize_t res;
    const char *pval;
    unsigned int tmp1, tmp2, i;

    /* Read the ID values into the dev_id structure */

    pval = udev_device_get_property_value (udev_dev, "PCI_ID");
    res = sscanf(pval, "%X:%X", &tmp1, &tmp2);
    if (res != 2)
        return -1;
    dev_id->vendor = tmp1;
    dev_id->device = tmp2;

    pval = udev_device_get_property_value (udev_dev, "PCI_SUBSYS_ID");
    res = sscanf(pval, "%X:%X", &tmp1, &tmp2);
    if (res != 2)
        return -1;
    dev_id->subvendor = tmp1;
    dev_id->subdevice = tmp2;

    rtapi_print_msg(RTAPI_MSG_DBG,"PCI_ID: %04X:%04X %04X:%04X\n",
    dev_id->vendor,
    dev_id->device,
    dev_id->subvendor,
    dev_id->subdevice);

    /* Compare values with list of valid IDs from driver struct */
    for (i=0; driver->id_table[i].vendor != 0; i++) {
        if (dev_id->vendor != driver->id_table[i].vendor)
            continue;
        if (dev_id->device != driver->id_table[i].device)
            continue;
        if (dev_id->subvendor != driver->id_table[i].subvendor)
            continue;
        if (dev_id->subdevice != driver->id_table[i].subdevice)
            continue;

        /* Everything matched up! */
        return 0;
    }

    /* ID was not present in driver ID table */
    return -1;
}

int pci_register_driver(struct pci_driver *driver)
{
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    struct udev_device *udev_dev;
    char buf[256];
    int i, r;
    int err;
//    DIR *dir;
//    struct dirent *entry = NULL, *dirent;
    ssize_t res;
	struct pci_dev *dev = NULL;
    struct pci_device_id dev_id;

    /* Create the udev object */
    udev = udev_new();
    if (!udev) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Can't create udev\n");
        return -1;
    }

    /* Create a filter that matchs the PCI_IDs we're looking for */
    enumerate = udev_enumerate_new(udev);
    err = udev_enumerate_add_match_subsystem(enumerate, "pci");
    if (err != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "Failed to register subsystem match: pci\n");
        goto error;
    }

    for (i=0; driver->id_table[i].vendor != 0; i++) {
        snprintf(buf, sizeof(buf), "%04X:%04X", 
            driver->id_table[i].vendor, 
            driver->id_table[i].device);

        err = udev_enumerate_add_match_property(enumerate, "PCI_ID", buf);
        if (err) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                    "Failed to register property match: PCI_ID=%s\n",
                    buf);
            goto error;
        }
    }

    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *name;

        /* Get the filename of the /sys entry for the device
           and create a udev_device object (dev) representing it */
        name = udev_list_entry_get_name(dev_list_entry);
        udev_dev = udev_device_new_from_syspath(udev, name);

        /* We have a device...see if it matches the ID list in the driver */
        r = check_device_id(udev_dev, &dev_id, driver);
        if (r < 0) {
            udev_device_unref(udev_dev);
            continue;
        }

        /* We got a device! */

        /* Allocate and clear a device structure */
        dev = malloc(sizeof(*dev));
        if (!dev) {
            rtapi_print_msg(RTAPI_MSG_ERR, "Out of memory\n");
            udev_device_unref(udev_dev);
            goto error;
        }
        memset(dev, 0, sizeof(*dev));

        /* Initialize device structure */
        strncpy(dev->dev_name,
                udev_device_get_property_value (udev_dev, "PCI_SLOT_NAME"),
                sizeof(dev->dev_name) - 1);

        strncpy(dev->sys_path, name,
                sizeof(dev->sys_path) - 1);

        dev->vendor             = dev_id.vendor;
        dev->device             = dev_id.device;
        dev->subsystem_vendor   = dev_id.subvendor;
        dev->subsystem_device   = dev_id.subdevice;
        dev->driver_data        = NULL;

        udev_device_unref(udev_dev);

//FIXME: Hook into the (yet to be created) linked list of devices!
one_dev = dev;

        rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_PCI: DeviceID: %04X %04X %04X %04X\n",
            dev->vendor,
            dev->device,
            dev->subsystem_vendor,
            dev->subsystem_device );

        /* Call module init code */
        rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_PCI: Calling driver probe function\n");
        res = driver->probe(dev, &dev_id);
        if (res != 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "Driver probe function failed!\n");
            goto error;
            break;
        }
    }
    /* Free the enumerator object */
    udev_enumerate_unref(enumerate);

    udev_unref(udev);
    return 0;

    error:
    udev_enumerate_unref(enumerate);
    udev_unref(udev);
    return -1;
}

void pci_unregister_driver(struct pci_driver *driver)
{
	if (driver == NULL)
		return;

    //FIXME: Support more than one card!
    driver->remove(one_dev);
    if (one_dev) {
	memset(one_dev, 0, sizeof(*one_dev));
	free(one_dev);
    }
}

void __iomem *pci_ioremap_bar(struct pci_dev *dev, int bar)
{
	void *mmio;
	char path[256];

    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_PCI: Map BAR %i\n", bar);

	if (bar < 0 || bar >= 6) {
		rtapi_print_msg(RTAPI_MSG_ERR, "Invalid PCI BAR %d\n", bar);
		return NULL;
	}

	snprintf(path, sizeof(path), "%s/resource%i", dev->sys_path, bar);

	/* Open the resource node */
	dev->resource[bar].fd = open(path, O_RDWR | O_SYNC);
	if (dev->resource[bar].fd < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "Could not open UIO resource \"%s\". (%s)\n",
				path, strerror(errno));
		return NULL;
	}

	mmio = mmap(NULL, pci_resource_len(dev,bar), PROT_READ | PROT_WRITE, MAP_SHARED,
		    dev->resource[bar].fd, 0);

	if (mmio == NULL || mmio == MAP_FAILED) {
		if (mmio == NULL)
			munmap(mmio, pci_resource_len(dev,bar));
		rtapi_print_msg(RTAPI_MSG_ERR, "Failed to remap MMIO %d of PCI device %s\n",
				bar, dev->dev_name);
		return NULL;
	}

	dev->resource[bar].mmio = mmio;

	return mmio;
}

inline void iounmap(volatile void __iomem *addr)
{
	unsigned int i;

    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_PCI: Unmap BAR\n");

	if (!addr)
		return;

	for (i = 0; i < 6; i++) {
		if (one_dev->resource[i].mmio == addr) {
			munmap((void*)addr, pci_resource_len(one_dev,i));
        	close(one_dev->resource[i].fd);
			memset(&one_dev->resource[i], 0, sizeof(one_dev->resource[i]));
            one_dev->resource[i].mmio = NULL;
    rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI_PCI: BAR %i unmapped\n",i);
			return;
		}
	}
	rtapi_print_msg(RTAPI_MSG_ERR, "IO-unmap: Did not find PCI mapping %p", addr);

    return;
}

int pci_enable_device(struct pci_dev *dev)
{
    FILE *stream;
    char path[256];
    int i,r;
    unsigned long long L1, L2, L3;

    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_PCI: Enabling Device %s\n", dev->dev_name);

    /* Enable the device */
    snprintf(path, sizeof(path), "%s/enable", dev->sys_path);
    stream = fopen(path, "w");
    if (stream == NULL) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"Failed to open \"%s\" (%s)\n",
				path, strerror(errno));
		return -1;
	}
    fprintf(stream, "1");
    fclose(stream);
        
    /* Open the resource file... */
    snprintf(path, sizeof(path), "%s/resource", dev->sys_path);
    stream = fopen(path, "r");
    if (stream == NULL) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"Failed to open \"%s\" (%s)\n",
				path, strerror(errno));
		return -1;
	}
        
    /* ...and read in the data */
    for (i=0; i < 6; i++) {
        r=fscanf(stream, "%Lx %Lx %Lx", &L1, &L2, &L3);
        if (r != 3) {
		    rtapi_print_msg(RTAPI_MSG_ERR,"Failed to parse \"%s\"\n", path);
            fclose(stream);
		    return -1;
        }
        dev->resource[i].start = (void*) L1;
        dev->resource[i].end   = (void*) L2;
        dev->resource[i].flags = (unsigned long) L3;

        rtapi_print_msg(RTAPI_MSG_DBG,"Resource %d: %p %p %08lx\n", i,
            dev->resource[i].start,
            dev->resource[i].end,
            dev->resource[i].flags);
    }

    fclose(stream);
    return 0;
}

int pci_disable_device(struct pci_dev *dev)
{
    FILE *stream;
    char path[256];
    int r;

    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_PCI: Disable Device\n");

    /* Enable the device */
    snprintf(path, sizeof(path), "%s/enable", dev->sys_path);
    stream = fopen(path, "w");
    if (stream == NULL) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"Failed to open \"%s\" (%s)\n",
				path, strerror(errno));
		return -1;
	}
    r = fprintf(stream, "0");
    if (r != 1)
		rtapi_print_msg(RTAPI_MSG_ERR,
				"Failed to disable device \"%s\" (%s)\n",
				dev->dev_name, strerror(errno));
    else
        r = 0;

    fclose(stream);

    return r;
}

EXPORT_SYMBOL(rtapi_pci_get_device);
EXPORT_SYMBOL(rtapi_pci_put_device);
EXPORT_SYMBOL(rtapi_pci_ioremap);
EXPORT_SYMBOL(rtapi_pci_iounmap);

EXPORT_SYMBOL(iounmap);
EXPORT_SYMBOL(pci_enable_device);
EXPORT_SYMBOL(pci_disable_device);
EXPORT_SYMBOL(pci_register_driver);
EXPORT_SYMBOL(pci_unregister_driver);
EXPORT_SYMBOL(pci_ioremap_bar);


#endif // USERMODE_PCI
