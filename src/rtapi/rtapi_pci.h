//    Copyright 2014 Jeff Epler
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#ifndef RTAPI_PCI_H
#define RTAPI_PCI_H

#ifdef __KERNEL__
#include <linux/pci.h>
#define rtapi__iomem __iomem
#define rtapi_pci_device_id pci_device_id
#define rtapi_pci_name pci_name
#define rtapi_pci_dev pci_dev
#define rtapi_pci_driver pci_driver
#define rtapi_pci_name pci_name
#define rtapi_pci_enable_device pci_enable_device
#define rtapi_pci_ioremap_bar pci_ioremap_bar
#define rtapi_pci_register_driver pci_register_driver
#define rtapi_pci_unregister_driver pci_unregister_driver
#define rtapi_pci_disable_device pci_disable_device
#define rtapi_pci_enable_device pci_enable_device
#define rtapi_pci_resource_start(dev, bar) pci_resource_start(dev, bar)
#define rtapi_pci_resource_end(dev, bar) pci_resource_end(dev, bar)
#define rtapi_pci_resource_flags(dev, bar) pci_resource_flags(dev, bar)
#define rtapi_pci_resource_len(dev, bar) pci_resource_len(dev, bar)
#define rtapi_pci_set_drvdata pci_set_drvdata
#define rtapi_iounmap iounmap

#else
#include <rtapi.h>
#include <rtapi_stdint.h>
#define rtapi__iomem /* nothing */

RTAPI_BEGIN_DECLS

struct rtapi_pci_device_id {
    rtapi_u32 vendor, device;           /* Vendor and device ID or PCI_ANY_ID*/
    rtapi_u32 subvendor, subdevice;     /* Subsystem ID's or PCI_ANY_ID */
    rtapi_u32 rtapi_class, class_mask;        /* (class,subclass,prog-if) triplet */
};

struct rtapi_pci_resource {
    intptr_t        start;         /* Details read from device/resource file */
    intptr_t        end;
    unsigned long   flags;
    void            *mmio;          /* The mmap address (if mapped) */
};

struct rtapi_pci_dev {
    char            dev_name[32];   /* Device name (0000:03:00.0) */
    char            sys_path[256];  /* Path to device (/sys/device/... */
    unsigned short  vendor;
    unsigned short  device;
    unsigned short  subsystem_vendor;
    unsigned short  subsystem_device;
    unsigned int    rtapi_class;          /* 3 bytes: (base,sub,prog-if) */
    struct rtapi_pci_resource
                    resource[6];    /* Device BARs */
    void *driver_data;              /* Data private to the driver */
};

struct rtapi_pci_driver {
    char *name;
    const struct rtapi_pci_device_id *id_table;   /* must be non-NULL for probe to be called */
    int  (*probe)  (struct rtapi_pci_dev *dev, const struct rtapi_pci_device_id *id);   /* New device inserted */
    void (*remove) (struct rtapi_pci_dev *dev);   /* Device removed (NULL if not a hot-plug capable driver) */
    void *private_data; /* Data private to rtapi_pci */
};

static inline const char *rtapi_pci_name(const struct rtapi_pci_dev *pdev)
{
    return pdev->dev_name;
}

int rtapi_pci_enable_device(struct rtapi_pci_dev *dev);
void rtapi__iomem *rtapi_pci_ioremap_bar(struct rtapi_pci_dev *pdev, int bar);

int rtapi_pci_register_driver(struct rtapi_pci_driver *driver);
void rtapi_pci_unregister_driver(struct rtapi_pci_driver *driver);
int rtapi_pci_enable_device(struct rtapi_pci_dev *dev);
int rtapi_pci_disable_device(struct rtapi_pci_dev *dev);

#define rtapi_pci_resource_start(dev, bar)    ((dev)->resource[(bar)].start)
#define rtapi_pci_resource_end(dev, bar)      ((dev)->resource[(bar)].end)
#define rtapi_pci_resource_flags(dev, bar)    ((dev)->resource[(bar)].flags)
#define rtapi_pci_resource_len(dev,bar) \
        ((rtapi_pci_resource_start((dev), (bar)) == 0 &&      \
          rtapi_pci_resource_end((dev), (bar)) ==             \
          rtapi_pci_resource_start((dev), (bar))) ? 0 :       \
                                                        \
         (rtapi_pci_resource_end((dev), (bar)) -              \
          rtapi_pci_resource_start((dev), (bar)) + 1))

static inline void rtapi_pci_set_drvdata(struct rtapi_pci_dev *pdev, void *data)
{
    pdev->driver_data = data;
}

void rtapi_iounmap(volatile void *addr);

int rtapi_pci_register_driver(struct rtapi_pci_driver *driver);
void rtapi_pci_unregister_driver(struct rtapi_pci_driver *driver);

RTAPI_END_DECLS
#endif
#endif
