#ifndef RTAPI_PCI_H
#define RTAPI_PCI_H

#include "config.h"

/***********************************************************************
*                        PCI DEVICE SUPPORT                            *
************************************************************************/
#if defined(BUILD_SYS_USER_DSO)

/** struct rtapi_pcidev - Opaque data structure for the PCI device */
struct rtapi_pcidev;

/** rtapi_pci_get_device - Find a PCI device
 * @vendor: The vendor ID to search for
 * @device: The device ID to search for
 * @from: The device to start searching from. Can be NULL.
 */
extern
struct rtapi_pcidev * rtapi_pci_get_device(__u16 vendor, __u16 device,
					   struct rtapi_pcidev *from);

/** rtapi_pci_put_device - Free a PCI device obtained by rtapi_pci_get_device() */
extern
void rtapi_pci_put_device(struct rtapi_pcidev *dev);

/** rtapi_pci_ioremap - Remap I/O memory
 * Returns NULL on error.
 * @dev: The device
 * @bar: The PCI BAR to remap.
 * @size: The size of the mapping.
 */
extern
void __iomem * rtapi_pci_ioremap(struct rtapi_pcidev *dev, int bar, size_t size);

/** rtapi_pci_iounmap - Unmap an MMIO region
 * @dev: The device
 * @mmio: The MMIO region obtained by rtapi_pci_ioremap()
 */
extern
void rtapi_pci_iounmap(struct rtapi_pcidev *dev, void __iomem *mmio);

static inline
__u8 rtapi_pci_readb(const void __iomem *mmio)
{
#ifdef BUILD_SYS_USER_DSO
	return *((volatile const __u8 __iomem *)mmio);
#else
	return readb(mmio);
#endif
}

static inline
__u16 rtapi_pci_readw(const void __iomem *mmio)
{
#ifdef BUILD_SYS_USER_DSO
	return *((volatile const __u16 __iomem *)mmio);
#else
	return readw(mmio);
#endif
}

static inline
__u32 rtapi_pci_readl(const void __iomem *mmio)
{
#ifdef BUILD_SYS_USER_DSO
	return *((volatile const __u32 __iomem *)mmio);
#else
	return readl(mmio);
#endif
}

static inline
void rtapi_pci_writeb(void __iomem *mmio, unsigned int offset, __u8 value)
{
#ifdef BUILD_SYS_USER_DSO
	*((volatile __u8 __iomem *)mmio) = value;
#else
	writeb(value, mmio);
#endif
}

static inline
void rtapi_pci_writew(void __iomem *mmio, unsigned int offset, __u16 value)
{
#ifdef BUILD_SYS_USER_DSO
	*((volatile __u16 __iomem *)mmio) = value;
#else
	writew(value, mmio);
#endif
}

static inline
void rtapi_pci_writel(void __iomem *mmio, unsigned int offset, __u32 value)
{
#ifdef BUILD_SYS_USER_DSO
	*((volatile __u32 __iomem *)mmio) = value;
#else
	writel(value, mmio);
#endif
}
#endif

#endif // RTAPI_PCI_H
