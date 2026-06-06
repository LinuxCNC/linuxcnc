/* rtapi_pci.c - Standalone PCI + firmware library for LinuxCNC cmod drivers
 *
 * Copyright (C) 2009-2013 Michael Büsch <m AT bues DOT CH>,
 *                         Charles Steinkuehler <charles AT steinkuehler DOT net>
 *                         John Morris <john AT zultron DOT com>
 *                         Michael Haberler <license AT mah DOT priv DOT at>
 * Copyright (C) 2014-2026 Jeff Epler <jepler@unpythonic.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "rtapi_pci.h"
#include "rtapi_firmware.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#ifndef NO_LIBUDEV
#include <libudev.h>
#endif

#define MAX_DEVICES 64
#define MAX_IOMAPS 64

/* -------------------------------------------------------------------------- */
/* Logging (stderr, captured by loader)                                       */
/* -------------------------------------------------------------------------- */

#define LOG_ERR(fmt, ...) \
    fprintf(stderr, "RTAPI_PCI ERROR: " fmt "\n", ##__VA_ARGS__)
#define LOG_DBG(fmt, ...) \
    fprintf(stderr, "RTAPI_PCI: " fmt "\n", ##__VA_ARGS__)

/* -------------------------------------------------------------------------- */
/* PCI subsystem                                                              */
/* -------------------------------------------------------------------------- */

#ifdef NO_LIBUDEV

int rtapi_pci_register_driver(struct rtapi_pci_driver *driver) {
    (void)driver;
    return -ENODEV;
}

void rtapi_pci_unregister_driver(struct rtapi_pci_driver *driver) {
    (void)driver;
}

void *rtapi_pci_ioremap_bar(struct rtapi_pci_dev *dev, int bar) {
    (void)dev; (void)bar;
    return NULL;
}

void rtapi_iounmap(volatile void *addr) {
    (void)addr;
}

int rtapi_pci_enable_device(struct rtapi_pci_dev *dev) {
    (void)dev;
    return -ENODEV;
}

int rtapi_pci_disable_device(struct rtapi_pci_dev *dev) {
    (void)dev;
    return -ENODEV;
}

#else /* have libudev */

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

static int check_device_id(struct udev_device *udev_dev,
    struct rtapi_pci_device_id *dev_id, struct rtapi_pci_driver *driver)
{
    ssize_t res;
    const char *pval;
    unsigned int i;

    pval = udev_device_get_property_value(udev_dev, "PCI_ID");
    res = sscanf(pval, "%X:%X", &dev_id->vendor, &dev_id->device);
    if (res != 2) {
        LOG_ERR("Unable to parse vendor:device from '%s'", pval);
        return -1;
    }

    pval = udev_device_get_property_value(udev_dev, "PCI_SUBSYS_ID");
    res = sscanf(pval, "%X:%X", &dev_id->subvendor, &dev_id->subdevice);
    if (res != 2) {
        LOG_ERR("Unable to parse subvendor:subdevice from '%s'", pval);
        return -1;
    }

    LOG_DBG("PCI_ID: %04X:%04X %04X:%04X",
        dev_id->vendor, dev_id->device,
        dev_id->subvendor, dev_id->subdevice);

    for (i = 0; driver->id_table[i].vendor != 0; i++) {
        if (dev_id->vendor != driver->id_table[i].vendor) continue;
        if (dev_id->device != driver->id_table[i].device) continue;
        if (dev_id->subvendor != driver->id_table[i].subvendor) continue;
        if (dev_id->subdevice != driver->id_table[i].subdevice) continue;
        return 0;
    }

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
        LOG_ERR("Out of memory");
        return -1;
    }
    devices->count = 0;
    driver->private_data = (void *)devices;

    udev = udev_new();
    if (!udev) {
        LOG_ERR("Can't create udev");
        free(devices);
        return -1;
    }

    enumerate = udev_enumerate_new(udev);
    err = udev_enumerate_add_match_subsystem(enumerate, "pci");
    if (err != 0) {
        LOG_ERR("Failed to register subsystem match: pci");
        goto error;
    }

    for (i = 0; driver->id_table[i].vendor; i++) {
        snprintf(buf, sizeof(buf), "%04X:%04X",
            driver->id_table[i].vendor,
            driver->id_table[i].device);

        err = udev_enumerate_add_match_property(enumerate, "PCI_ID", buf);
        if (err) {
            LOG_ERR("Failed to register property match: PCI_ID=%s", buf);
            goto error;
        }
    }

    udev_enumerate_scan_devices(enumerate);
    device_list = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(dev_list_entry, device_list) {
        const char *name;

        name = udev_list_entry_get_name(dev_list_entry);
        udev_dev = udev_device_new_from_syspath(udev, name);

        int r = check_device_id(udev_dev, &dev_id, driver);
        if (r < 0) {
            udev_device_unref(udev_dev);
            continue;
        }

        dev = malloc(sizeof(struct rtapi_pci_dev));
        if (!dev) {
            LOG_ERR("Out of memory");
            udev_device_unref(udev_dev);
            goto error;
        }
        memset(dev, 0, sizeof(struct rtapi_pci_dev));

        strncpy(dev->dev_name,
            udev_device_get_property_value(udev_dev, "PCI_SLOT_NAME"),
            sizeof(dev->dev_name) - 1);

        strncpy(dev->sys_path, name, sizeof(dev->sys_path) - 1);

        dev->vendor           = dev_id.vendor;
        dev->device           = dev_id.device;
        dev->subsystem_vendor = dev_id.subvendor;
        dev->subsystem_device = dev_id.subdevice;
        dev->driver_data      = NULL;

        udev_device_unref(udev_dev);

        LOG_DBG("DeviceID: %04X %04X %04X %04X",
            dev->vendor, dev->device,
            dev->subsystem_vendor, dev->subsystem_device);

        LOG_DBG("Calling driver probe function");
        res = driver->probe(dev, &dev_id);
        if (res != 0) {
            free(dev);
            LOG_ERR("Driver probe function failed!");
            rtapi_pci_unregister_driver(driver);
            goto error;
        }

        if (devices->count < MAX_DEVICES) {
            devices->devices[devices->count++] = dev;
        } else {
            LOG_ERR("Too many PCI devices");
            free(dev);
            goto error;
        }
    }

    udev_enumerate_unref(enumerate);
    udev_unref(udev);
    return 0;

error:
    udev_enumerate_unref(enumerate);
    udev_unref(udev);
    return -1;
}

void rtapi_pci_unregister_driver(struct rtapi_pci_driver *driver)
{
    struct device_vector *devices;
    int i;

    if (!driver) return;

    devices = get_devices(driver);
    if (!devices) return;

    for (i = 0; i < devices->count; i++) {
        driver->remove(devices->devices[i]);
        free(devices->devices[i]);
    }

    free(devices);
    driver->private_data = NULL;
}

void *rtapi_pci_ioremap_bar(struct rtapi_pci_dev *dev, int bar)
{
    void *mmio;
    char path[280];
    int fd;
    size_t resource_len;
    int i;

    LOG_DBG("Map BAR %d", bar);

    if (bar < 0 || bar >= 6) {
        LOG_ERR("Invalid PCI BAR %d", bar);
        return NULL;
    }

    snprintf(path, sizeof(path), "%s/resource%d", dev->sys_path, bar);

    fd = open(path, O_RDWR | O_SYNC);
    if (fd < 0) {
        LOG_ERR("Could not open resource \"%s\". (%s)", path, strerror(errno));
        return NULL;
    }

    resource_len = rtapi_pci_resource_len(dev, bar);
    mmio = mmap(NULL, resource_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (mmio == NULL || mmio == MAP_FAILED) {
        LOG_ERR("Failed to remap MMIO %d of PCI device %s: %s",
            bar, dev->dev_name, strerror(errno));
        return NULL;
    }

    for (i = 0; i < MAX_IOMAPS; i++) {
        if (!iomaps[i].in_use) {
            iomaps[i].addr = mmio;
            iomaps[i].size = resource_len;
            iomaps[i].in_use = 1;
            break;
        }
    }

    return mmio;
}

void rtapi_iounmap(volatile void *addr)
{
    int i;

    if (!addr) return;

    for (i = 0; i < MAX_IOMAPS; i++) {
        if (iomaps[i].in_use && iomaps[i].addr == (void *)addr) {
            munmap(iomaps[i].addr, iomaps[i].size);
            LOG_DBG("Unmapped %zd bytes at %p", iomaps[i].size, addr);
            iomaps[i].in_use = 0;
            return;
        }
    }

    LOG_ERR("IO-unmap: Did not find PCI mapping %p", addr);
}

static char *get_sys_enable_path(const struct rtapi_pci_dev *dev,
    char *path, size_t npath)
{
    snprintf(path, npath, "%s/enable", dev->sys_path);
    if (access(path, F_OK) < 0)
        snprintf(path, npath, "%s/enabled", dev->sys_path);
    return path;
}

int rtapi_pci_enable_device(struct rtapi_pci_dev *dev)
{
    FILE *stream;
    char path[280];
    int i, r;
    unsigned long long L1, L2, L3;

    LOG_DBG("Enabling Device %s", dev->dev_name);

    stream = fopen(get_sys_enable_path(dev, path, sizeof(path)), "w");
    if (stream == NULL) {
        LOG_ERR("Failed to open \"%s\" (%s)", path, strerror(errno));
        return -1;
    }
    fprintf(stream, "1");
    fclose(stream);

    snprintf(path, sizeof(path), "%s/resource", dev->sys_path);
    stream = fopen(path, "r");
    if (stream == NULL) {
        LOG_ERR("Failed to open \"%s\" (%s)", path, strerror(errno));
        return -1;
    }

    for (i = 0; i < 6; i++) {
        r = fscanf(stream, "%Lx %Lx %Lx", &L1, &L2, &L3);
        if (r != 3) {
            LOG_ERR("Failed to parse \"%s\"", path);
            fclose(stream);
            return -1;
        }
        dev->resource[i].start = L1;
        dev->resource[i].end   = L2;
        dev->resource[i].flags = (unsigned long)L3;

        LOG_DBG("Resource %d: %p %p %08lx", i,
            (void *)dev->resource[i].start,
            (void *)dev->resource[i].end,
            dev->resource[i].flags);
    }

    fclose(stream);
    return 0;
}

int rtapi_pci_disable_device(struct rtapi_pci_dev *dev)
{
    FILE *stream;
    char path[280];
    int r;

    LOG_DBG("Disable Device %s", dev->dev_name);

    stream = fopen(get_sys_enable_path(dev, path, sizeof(path)), "w");
    if (stream == NULL) {
        LOG_ERR("Failed to open \"%s\" (%s)", path, strerror(errno));
        return -1;
    }
    r = fprintf(stream, "0");
    if (r != 1) {
        LOG_ERR("Failed to disable device \"%s\" (%s)",
            dev->dev_name, strerror(errno));
        fclose(stream);
        return -1;
    }

    fclose(stream);
    return 0;
}

#endif /* !NO_LIBUDEV */

/* -------------------------------------------------------------------------- */
/* Firmware loading                                                           */
/* -------------------------------------------------------------------------- */

int rtapi_request_firmware(const struct rtapi_firmware **fw,
    const char *name, struct rtapi_device *device)
{
    (void)device;
    struct rtapi_firmware *lfw;
    struct utsname sysinfo;
    const char *basepath = "/lib/firmware";
    char path[256];
    struct stat st;
    int fd = -1;

    lfw = malloc(sizeof(struct rtapi_firmware));
    if (lfw == NULL) {
        LOG_ERR("Out of memory");
        return -ENOMEM;
    }

    /* Try kernel-specific path first */
    if (uname(&sysinfo) >= 0) {
        snprintf(path, sizeof(path), "%s/%s/%s", basepath, sysinfo.release, name);
        fd = open(path, O_RDONLY);
    }

    /* Fallback to generic path */
    if (fd < 0) {
        snprintf(path, sizeof(path), "%s/%s", basepath, name);
        fd = open(path, O_RDONLY);
    }

    if (fd < 0) {
        LOG_ERR("Could not locate firmware \"%s\". (%s)", name, strerror(errno));
        free(lfw);
        return -ENOENT;
    }

    if (fstat(fd, &st) < 0) {
        LOG_ERR("Could not determine size of file \"%s\". (%s)",
            path, strerror(errno));
        close(fd);
        free(lfw);
        return -1;
    }

    lfw->size = st.st_size;
    lfw->data = (const uint8_t *)mmap(NULL, lfw->size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (lfw->data == NULL || lfw->data == MAP_FAILED) {
        LOG_ERR("Failed to mmap file %s", path);
        if (lfw->data == MAP_FAILED)
            munmap((void *)lfw->data, lfw->size);
        free(lfw);
        return -1;
    }

    *fw = lfw;
    return 0;
}

void rtapi_release_firmware(const struct rtapi_firmware *fw)
{
    if (!fw) return;
    munmap((void *)fw->data, fw->size);
    free((void *)fw);
}
