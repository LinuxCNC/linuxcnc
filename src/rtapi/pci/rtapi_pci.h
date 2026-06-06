/* rtapi_pci.h - Standalone PCI access library for LinuxCNC cmod drivers
 *
 * Copyright (C) 2009-2013 Sebastian Kuzminsky, Michael Büsch,
 *                         Charles Steinkuehler, John Morris, Michael Haberler
 * Copyright (C) 2014-2026 Jeff Epler <jepler@unpythonic.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */
#ifndef RTAPI_PCI_H
#define RTAPI_PCI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rtapi_pci_device_id {
    uint32_t vendor, device;
    uint32_t subvendor, subdevice;
    uint32_t rtapi_class, class_mask;
};

struct rtapi_pci_resource {
    intptr_t        start;
    intptr_t        end;
    unsigned long   flags;
    void            *mmio;
};

struct rtapi_pci_dev {
    char            dev_name[32];
    char            sys_path[256];
    unsigned short  vendor;
    unsigned short  device;
    unsigned short  subsystem_vendor;
    unsigned short  subsystem_device;
    unsigned int    rtapi_class;
    struct rtapi_pci_resource resource[6];
    void *driver_data;
};

struct rtapi_pci_driver {
    char *name;
    const struct rtapi_pci_device_id *id_table;
    int  (*probe)  (struct rtapi_pci_dev *dev, const struct rtapi_pci_device_id *id);
    void (*remove) (struct rtapi_pci_dev *dev);
    void *private_data;
};

static inline const char *rtapi_pci_name(const struct rtapi_pci_dev *pdev) {
    return pdev->dev_name;
}

static inline void *rtapi_pci_get_drvdata(struct rtapi_pci_dev *pdev) {
    return pdev->driver_data;
}

static inline void rtapi_pci_set_drvdata(struct rtapi_pci_dev *pdev, void *data) {
    pdev->driver_data = data;
}

int rtapi_pci_register_driver(struct rtapi_pci_driver *driver);
void rtapi_pci_unregister_driver(struct rtapi_pci_driver *driver);
int rtapi_pci_enable_device(struct rtapi_pci_dev *dev);
int rtapi_pci_disable_device(struct rtapi_pci_dev *dev);
void *rtapi_pci_ioremap_bar(struct rtapi_pci_dev *pdev, int bar);
void rtapi_iounmap(volatile void *addr);

#define rtapi_pci_resource_start(dev, bar)  ((dev)->resource[(bar)].start)
#define rtapi_pci_resource_end(dev, bar)    ((dev)->resource[(bar)].end)
#define rtapi_pci_resource_flags(dev, bar)  ((dev)->resource[(bar)].flags)
#define rtapi_pci_resource_len(dev, bar) \
    ((rtapi_pci_resource_start((dev), (bar)) == 0 && \
      rtapi_pci_resource_end((dev), (bar)) == rtapi_pci_resource_start((dev), (bar))) ? 0 : \
     (rtapi_pci_resource_end((dev), (bar)) - rtapi_pci_resource_start((dev), (bar)) + 1))

#ifdef __cplusplus
}
#endif

#endif /* RTAPI_PCI_H */
