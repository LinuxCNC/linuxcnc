/********************************************************************
* Description:  linux_pci.c
*               This file, 'linux_pci.c', implements the 
*               usermode PCI functions 
*
* Copyright (c) 2009 Michael Buesch <mb@bu3sch.de>
*               reworjed Michael Haberler 2012
*
* License: GPL Version 2
*
********************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>		/* vprintf() */
#include <stdlib.h>		/* malloc(), sizeof() */
#include <unistd.h>		/* usleep() */
#include <sys/io.h>		/* shmget() */
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <time.h>

#include "config.h"
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

#define UIO_PCI_PATH	"/sys/bus/pci/drivers/uio_pci_generic"
#define UIO_BAR_PATH	"/sys/devices/pci/drivers/uio_pci_generic"

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
	int i;

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
