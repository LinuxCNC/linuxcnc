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
* Copyright (C) 2014-2026 Jeff Epler <jepler@unpythonic.net>

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

#include <rtapi.h>
#include <rtapi_pci.h>
#include <rtapi_firmware.h>

#include <stdatomic.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif
#include <sys/mman.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/fsuid.h>
#endif

#define MAX_DEVICES 64
#define MAX_IOMAPS 64

static uid_t euid, ruid;
static _Atomic int with_root_level = 0;

static void with_root_enter(void) {
    if(atomic_fetch_add(&with_root_level, 1) == 0) {
#ifdef __linux__
        setfsuid(euid);
#endif
    }
}

static void with_root_exit(void) {
    if(atomic_fetch_sub(&with_root_level, 1) == 1) {
#ifdef __linux__
        setfsuid(ruid);
#endif
    }
}

#ifdef NO_LIBUDEV
int rtapi_pci_register_driver(struct rtapi_pci_driver *driver) {
    return -ENODEV;
}
void rtapi_pci_unregister_driver(struct rtapi_pci_driver *driver)
{
}

void rtapi__iomem *rtapi_pci_ioremap_bar(struct rtapi_pci_dev *dev, int bar)
{
    return NULL;
}

void rtapi_iounmap(volatile void rtapi__iomem *addr)
{
}

int rtapi_pci_enable_device(struct rtapi_pci_dev *dev)
{
    return -ENODEV;
}

int rtapi_pci_disable_device(struct rtapi_pci_dev *dev)
{
    return -ENODEV;
}

#else
#include <libudev.h>

struct device_vector {
    struct rtapi_pci_dev *devices[MAX_DEVICES];
    int count;
};

static struct device_vector *get_devices(struct rtapi_pci_driver *driver) {
    return (struct device_vector *)(driver->private_data);
}

struct iomap_entry {
    void *addr;
    size_t size;
    int in_use;
};

static struct iomap_entry iomaps[MAX_IOMAPS];

static int check_device_id(struct udev_device *udev_dev, struct rtapi_pci_device_id *dev_id, struct rtapi_pci_driver *driver)
{
    ssize_t res;
    const char *pval;
    unsigned int i;

    /* Read the ID values into the dev_id structure */

    pval = udev_device_get_property_value (udev_dev, "PCI_ID");
    res = sscanf(pval, "%X:%X", &dev_id->vendor, &dev_id->device);
    if (res != 2) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "Unable to parse vendor:device from '%s'\n", pval);
        return -1;
    }

    pval = udev_device_get_property_value (udev_dev, "PCI_SUBSYS_ID");
    res = sscanf(pval, "%X:%X", &dev_id->subvendor, &dev_id->subdevice);
    if (res != 2) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "Unable to parse subvendor:subdevice from '%s'\n", pval);
        return -1;
    }

    rtapi_print_msg(RTAPI_MSG_DBG,"PCI_ID: %04X:%04X %04X:%04X\n",
        dev_id->vendor, dev_id->device,
        dev_id->subvendor, dev_id->subdevice);

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

int rtapi_pci_register_driver(struct rtapi_pci_driver *driver)
{
    struct device_vector *devices;
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *device_list, *dev_list_entry;
    struct udev_device *udev_dev;
    char buf[256];
    int i, err;
    ssize_t res;
    struct rtapi_pci_dev *dev = NULL;
    struct rtapi_pci_device_id dev_id;

    devices = malloc(sizeof(struct device_vector));
    if (!devices) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Out of memory\n");
        return -1;
    }
    devices->count = 0;
    driver->private_data = (void *)devices;

    with_root_enter();

    /* Create the udev object */
    udev = udev_new();
    if (!udev) {
        rtapi_print_msg(RTAPI_MSG_ERR,"Can't create udev\n");
        with_root_exit();
        free(devices);
        return -1;
    }

    /* Create a filter that matches the PCI_IDs we're looking for */
    enumerate = udev_enumerate_new(udev);
    err = udev_enumerate_add_match_subsystem(enumerate, "pci");
    if (err != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "Failed to register subsystem match: pci\n");
        goto error;
    }

    for (i=0; driver->id_table[i].vendor; i++) {
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
    device_list = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(dev_list_entry, device_list) {
        const char *name;

        /* Get the filename of the /sys entry for the device
           and create a udev_device object (dev) representing it */
        name = udev_list_entry_get_name(dev_list_entry);
        udev_dev = udev_device_new_from_syspath(udev, name);

        /* We have a device...see if it matches the ID list in the driver */
        int r = check_device_id(udev_dev, &dev_id, driver);
        if (r < 0) {
            udev_device_unref(udev_dev);
            continue;
        }

        /* We got a device! */

        /* Allocate and clear a device structure */
        dev = malloc(sizeof(struct rtapi_pci_dev));
        if (!dev) {
            rtapi_print_msg(RTAPI_MSG_ERR, "Out of memory\n");
            udev_device_unref(udev_dev);
            goto error;
        }
        memset(dev, 0, sizeof(struct rtapi_pci_dev));

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

        rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_PCI: DeviceID: %04X %04X %04X %04X\n",
            dev->vendor,
            dev->device,
            dev->subsystem_vendor,
            dev->subsystem_device );

        /* Call module init code */
        rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_PCI: Calling driver probe function\n");
        res = driver->probe(dev, &dev_id);
        if (res != 0) {
            free(dev);
            rtapi_print_msg(RTAPI_MSG_ERR, "Driver probe function failed!\n");
            rtapi_pci_unregister_driver(driver);
            goto error;
        }
        
        if (devices->count < MAX_DEVICES) {
            devices->devices[devices->count++] = dev;
        } else {
            rtapi_print_msg(RTAPI_MSG_ERR, "Too many PCI devices\n");
            free(dev);
            goto error;
        }
    }
    /* Free the enumerator object */
    udev_enumerate_unref(enumerate);

    udev_unref(udev);
    with_root_exit();
    return 0;

    error:
    udev_enumerate_unref(enumerate);
    udev_unref(udev);
    with_root_exit();
    return -1;
}

void rtapi_pci_unregister_driver(struct rtapi_pci_driver *driver)
{
    struct device_vector *devices;
    int i;
    
    if (!driver) return;

    devices = get_devices(driver);
    if (!devices) return;

    with_root_enter();
    
    for(i = 0; i < devices->count; i++)
    {
        driver->remove(devices->devices[i]);
        free(devices->devices[i]);
    }
    
    free(devices);
    driver->private_data = NULL;
    
    with_root_exit();
}

void rtapi__iomem *rtapi_pci_ioremap_bar(struct rtapi_pci_dev *dev, int bar)
{
    void *mmio;
    char path[256];
    int fd;
    size_t resource_len;
    size_t ret;
    int i;

    with_root_enter();

    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_PCI: Map BAR %i\n", bar);

    if (bar < 0 || bar >= 6) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Invalid PCI BAR %d\n", bar);
        with_root_exit();
        return NULL;
    }

    ret = snprintf(path, sizeof(path), "%s/resource%i", dev->sys_path, bar);
    if (ret >= sizeof(path)) {
        with_root_exit();
        return NULL;
    }
    
    /* Open the resource node */
    fd = open(path, O_RDWR | O_SYNC);
    if (fd < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Could not open resource \"%s\". (%s)\n",
            path, strerror(errno));
        with_root_exit();
        return NULL;
    }

    resource_len = rtapi_pci_resource_len(dev, bar);
    mmio = mmap(NULL, resource_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (mmio == NULL || mmio == MAP_FAILED) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Failed to remap MMIO %d of PCI device %s: %s\n",
            bar, dev->dev_name, strerror(errno));
        with_root_exit();
        return NULL;
    }

    /* Store in iomap table */
    for(i = 0; i < MAX_IOMAPS; i++) {
        if(!iomaps[i].in_use) {
            iomaps[i].addr = mmio;
            iomaps[i].size = resource_len;
            iomaps[i].in_use = 1;
            break;
        }
    }

    with_root_exit();
    return mmio;
}

void rtapi_iounmap(volatile void rtapi__iomem *addr)
{
    int i;
    
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_PCI: Unmap BAR\n");

    if (!addr) return;

    for(i = 0; i < MAX_IOMAPS; i++) {
        if(iomaps[i].in_use && iomaps[i].addr == (void*)addr) {
            munmap(iomaps[i].addr, iomaps[i].size);
            rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_PCI: Unmapped %zd bytes at %p\n", 
                iomaps[i].size, addr);
            iomaps[i].in_use = 0;
            return;
        }
    }
    
    rtapi_print_msg(RTAPI_MSG_ERR, "IO-unmap: Did not find PCI mapping %p", addr);
}

static char *get_sys_enable_path(const struct rtapi_pci_dev *dev, char *path, size_t npath) {
    snprintf(path, npath, "%s/enable", dev->sys_path);
    /* kernel 3.12 renamed this file to "enabled"... doh */
    if(access(path, F_OK) < 0)
        snprintf(path, npath, "%s/enabled", dev->sys_path);
    return path;
}

int rtapi_pci_enable_device(struct rtapi_pci_dev *dev)
{
    FILE *stream;
    char path[280];
    int i,r;
    unsigned long long L1, L2, L3;
    size_t ret;

    with_root_enter();

    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_PCI: Enabling Device %s\n", dev->dev_name);

    /* Enable the device */
    stream = fopen(get_sys_enable_path(dev, path, sizeof(path)), "w");
    if (stream == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "Failed to open \"%s\" (%s)\n",
                        path, strerror(errno));
        with_root_exit();
        return -1;
    }
    fprintf(stream, "1");
    fclose(stream);

    /* Open the resource file... */
    ret = snprintf(path, sizeof(path), "%s/resource", dev->sys_path);
    if (ret >= sizeof(path)) {
        with_root_exit();
        return -1;
    }
    stream = fopen(path, "r");
    if (stream == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "Failed to open \"%s\" (%s)\n",
                        path, strerror(errno));
        with_root_exit();
        return -1;
    }

    /* ...and read in the data */
    for (i=0; i < 6; i++) {
        r=fscanf(stream, "%Lx %Lx %Lx", &L1, &L2, &L3);
        if (r != 3) {
            rtapi_print_msg(RTAPI_MSG_ERR,"Failed to parse \"%s\"\n", path);
            fclose(stream);
            with_root_exit();
            return -1;
        }
        dev->resource[i].start = L1;
        dev->resource[i].end   = L2;
        dev->resource[i].flags = (unsigned long) L3;

        rtapi_print_msg(RTAPI_MSG_DBG,"Resource %d: %p %p %08lx\n", i,
            (void*)dev->resource[i].start,
            (void*)dev->resource[i].end,
            dev->resource[i].flags);
    }

    fclose(stream);
    with_root_exit();
    return 0;
}

int rtapi_pci_disable_device(struct rtapi_pci_dev *dev)
{
    FILE *stream;
    char path[280];
    int r;

    with_root_enter();

    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI_PCI: Disable Device\n");

    /* Enable the device */
    stream = fopen(get_sys_enable_path(dev, path, sizeof(path)), "w");
    if (stream == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "Failed to open \"%s\" (%s)\n",
                        path, strerror(errno));
        with_root_exit();
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

    with_root_exit();
    return r;
}
#endif

int rtapi_request_firmware(const struct rtapi_firmware **fw, const char *name, struct rtapi_device *device) {
    struct rtapi_firmware *lfw;
    struct utsname sysinfo;
    const char *basepath = "/lib/firmware";
    char path[256];
    struct stat st;
    int r;
    int fd = -1;

    /* Allocate and initialize a firmware struct */
    lfw = malloc(sizeof(struct rtapi_firmware));
    if (lfw == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Out of memory\n");
        return -ENOMEM;
    }

    /* Try to open the kernel-specific file */
    r = uname(&sysinfo);
    if (r >= 0) {
        snprintf(path, sizeof(path), "/%s/%s/%s", basepath, sysinfo.release, name);
        fd = open(path, O_RDONLY);
    }

    /* If we don't have a valid file descriptor yet, try an alternate location */
    if (fd < 0) {
        snprintf(path, sizeof(path), "/%s/%s", basepath, name);
        fd = open(path, O_RDONLY);
    }


    /* If we don't have a valid file descriptor by here, it's an error */
    if (fd < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Could not locate firmware \"%s\". (%s)\n",
                        path, strerror(errno));
        free(lfw);
        return -ENOENT;
    }

    /* We've found and opened the file, now let's get the size */
    if (stat(path, &st) < 0)
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "Could not determine size of file \"%s\". (%s)\n",
                        path, strerror(errno));
        free(lfw);
        return -1;
    }

    lfw->size = st.st_size;
    lfw->data = (const rtapi_u8 *)mmap(NULL, lfw->size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (lfw->data == NULL || lfw->data == MAP_FAILED) {
        if (lfw->data == NULL)
            munmap((void*)lfw->data, lfw->size);
        rtapi_print_msg(RTAPI_MSG_ERR, "Failed to mmap file %s\n", path);
        free(lfw);
        return -1;
    }

    *fw = lfw;
    return 0;
}

void rtapi_release_firmware(const struct rtapi_firmware *fw) {
    munmap((void*)fw->data, fw->size);
    free((void*)fw);
}

EXPORT_SYMBOL(rtapi_iounmap);
EXPORT_SYMBOL(rtapi_pci_enable_device);
EXPORT_SYMBOL(rtapi_pci_disable_device);
EXPORT_SYMBOL(rtapi_pci_register_driver);
EXPORT_SYMBOL(rtapi_pci_unregister_driver);
EXPORT_SYMBOL(rtapi_pci_ioremap_bar);

/* Initialize euid/ruid for with_root functions */
void __attribute__((constructor)) rtapi_pci_init(void) {
    euid = geteuid();
    ruid = getuid();
}
