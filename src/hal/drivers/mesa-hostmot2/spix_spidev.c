/*
 * This is a component for hostmot2 over SPI for linuxcnc.
 * Copyright (c) 2024 B.Stultiens <lcnc@vagrearg.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include <rtapi.h>

#define HM2_LLIO_NAME "spix_spidev"

#include "hostmot2-lowlevel.h"

#include "spix.h"


/*
 * Our SPI port descriptor extends the spix port descriptor.
 */
typedef struct __spi_port_t {
	spix_port_t	spix;	// The upstream container
	int			fd;		// Spidev file descriptor
	uint32_t	clkw;	// Write clock setting
	uint32_t	clkr;	// Read clock setting
} spidev_port_t;

/* Forward decls */
static int spidev_detect(const char *dtcs[]);
static int spidev_setup(int probemask);
static int spidev_cleanup(void);
static const spix_port_t *spidev_open(int port, const spix_args_t *args);
static int spidev_close(const spix_port_t *sp);
static int spi_transfer(const spix_port_t *sp, uint32_t *wptr, size_t txlen, int rw);

#define PORT_MAX	5
static spidev_port_t spi_ports[PORT_MAX] = {
	{ .spix = { .width = 8, .name = "/dev/spidev0.0", .transfer = spi_transfer }, .fd = -1 },
	{ .spix = { .width = 8, .name = "/dev/spidev0.1", .transfer = spi_transfer }, .fd = -1 },
	{ .spix = { .width = 8, .name = "/dev/spidev1.0", .transfer = spi_transfer }, .fd = -1 },
	{ .spix = { .width = 8, .name = "/dev/spidev1.1", .transfer = spi_transfer }, .fd = -1 },
	{ .spix = { .width = 8, .name = "/dev/spidev1.2", .transfer = spi_transfer }, .fd = -1 },
};

/*
 * The driver interface structure
 */
spix_driver_t spix_driver_spidev = {
	.name		= HM2_LLIO_NAME,
	.num_ports	= PORT_MAX,

	.detect		= spidev_detect,
	.setup		= spidev_setup,
	.cleanup	= spidev_cleanup,
	.open		= spidev_open,
	.close		= spidev_close,
};

static int driver_enabled;	// Set to non-zero when spidev_setup() is successfully called
static int port_probe_mask;	// Which ports are requested


/*********************************************************************/
/*
 * Transfer a buffer of words to the SPI port and fill the same buffer with the
 * data coming from the SPI port.
 */
static int spi_transfer(const spix_port_t *sp, uint32_t *wptr, size_t txlen, int rw)
{
	spidev_port_t *sdp = (spidev_port_t *)sp;
	struct spi_ioc_transfer sit;
	uint32_t u;

	if(!txlen)
		return 1;	// Nothing to do, return success
	if(sdp->fd < 0)
		return 0;	// Error, no file descriptor

	// Using 8-bit transfers; need to byte-swap to big-endian to get the word's
	// most significant bit shifted out first.
	for(u = 0; u < txlen; u++)
		wptr[u] = htobe32(wptr[u]);

	memset(&sit, 0, sizeof(sit));
	sit.tx_buf = (uintptr_t)wptr;
	sit.rx_buf = (uintptr_t)wptr;
	sit.len = txlen * sizeof(uint32_t);
	sit.speed_hz = rw ? sdp->clkr : sdp->clkw;
	sit.bits_per_word = 8;
	sit.delay_usecs = 10;	// Magic
	if(ioctl(sdp->fd, SPI_IOC_MESSAGE(1), &sit) < 0) {
		LL_ERR("%s: SPI transfer failed: %s", sdp->spix.name, strerror(errno));
		return 0;
	}

	// Put the received words into host order
	for(u = 0; u < txlen; u++)
		wptr[u] = be32toh(wptr[u]);

	return 1;
}

/*************************************************/
/*
 * Open and setup a /dev/spidevX.Y port
 */
static int port_configure(spidev_port_t *sdp, const spix_args_t *args)
{
	int fd;
	uint8_t b;
	uint32_t w;
	int e;
	uint32_t clkw = args->clkw;	// Requested write and read clocks
	uint32_t clkr = args->clkr;

	// Module argument override of device node path
	if(args->spidev)
		sdp->spix.name = args->spidev;

	if((fd = open(sdp->spix.name, O_RDWR)) < 0) {
		LL_ERR("%s: Cannot open port: %s\n", sdp->spix.name, strerror(e = errno));
		return -e;
	}

	w = SPI_MODE_0;	// CPOL=0, CPHA=0
	if(ioctl(fd, SPI_IOC_WR_MODE32, &w) < 0) {
		LL_ERR("%s: Cannot set mode 0: %s\n", sdp->spix.name, strerror(e = errno));
		close(fd);
		return -e;
	}

	b = 0;	// Set MSB-first
	if(ioctl(fd, SPI_IOC_WR_LSB_FIRST, &b) < 0) {
		LL_ERR("%s: Cannot set MSB-first order: %s\n", sdp->spix.name, strerror(e = errno));
		close(fd);
		return -e;
	}

	b = 8;	// Set 8-bit per word transfer
    if(ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &b) < 0) {
		LL_ERR("%s: Cannot set MSB-first order: %s\n", sdp->spix.name, strerror(e = errno));
		close(fd);
		return -e;
	}

	// Test setting the write speed (overridden in transfer anyway)
	if(ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &clkw) < 0) {
		LL_ERR("%s: Cannot set write clock speed to %u: %s\n", sdp->spix.name, clkw, strerror(e = errno));
		close(fd);
		return -e;
	}

	// Read back write clock
	if(ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &clkw) < 0) {
		LL_ERR("%s: Cannot get write clock speed: %s\n", sdp->spix.name, strerror(e = errno));
		close(fd);
		return -e;
	}
	sdp->clkw = clkw;

	// Test setting the read speed (overridden in transfer anyway)
	if(ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &clkr) < 0) {
		LL_ERR("%s: Cannot set read clock speed to %u: %s\n", sdp->spix.name, clkr, strerror(e = errno));
		close(fd);
		return -e;
	}

	// Read back read clock
	if(ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &clkr) < 0) {
		LL_ERR("%s: Cannot get write clock speed: %s\n", sdp->spix.name, strerror(e = errno));
		close(fd);
		return -e;
	}
	sdp->clkr = clkr;

	sdp->fd = fd;
	return 0;
}

/*************************************************/
/*
 * Detect the presence of the hardware on basis of the device-tree compatible
 * string-list.
 * On success returns 0 (zero) and sets the driver information to the dtc
 * string and tries to set a human readable string as model.
 *
 * The spix_spidev driver always succeeds with detection. We could check the
 * presence of /dev/spidev* entries, but the port opens will fail if they do
 * not exists.
 */
static int spidev_detect(const char *dtcs[])
{
	// Set the matched dtc and model informational strings
	if(dtcs[0])
		strncpy(spix_driver_spidev.dtc, dtcs[0], sizeof(spix_driver_spidev.dtc)-1);
	else
		strncpy(spix_driver_spidev.dtc, "spix_spidev-unknown-dtc", sizeof(spix_driver_spidev.dtc)-1);
	if(spix_read_file("/proc/device-tree/model", spix_driver_spidev.model, sizeof(spix_driver_spidev.model)) < 0)
		strncpy(spix_driver_spidev.model, "??? Unknown board ???", sizeof(spix_driver_spidev.model)-1);

	return 0;
}

/*
 * Setup the driver.
 * Not much to do for spidev.
 */
static int spidev_setup(int probemask)
{
	if(driver_enabled) {
		LL_ERR("Driver is already setup.\n");
		return -EBUSY;
	}

	port_probe_mask = probemask;

	driver_enabled = 1;

	return 0;
}

/*
 * Cleanup the driver
 * - close any open ports
 */
static int spidev_cleanup(void)
{
	int i;

	if(!driver_enabled) {
		LL_ERR("Driver is not setup.\n");
		return -ENODEV;
	}

	// Close any ports not closed already
	for(i = 0; i < PORT_MAX; i++) {
		if(spi_ports[i].fd >= 0)
			spix_driver_spidev.close(&spi_ports[i].spix);
	}

	driver_enabled = 0;
	return 0;
}

/*
 * Open a SPI port at index 'port' using 'args' to configure.
 */
static const spix_port_t *spidev_open(int port, const spix_args_t *args)
{
	spidev_port_t *sdp;

	if(!driver_enabled) {
		LL_ERR("Driver is not setup.\n");
		return NULL;
	}

	if(port < 0 || port >= PORT_MAX) {
		LL_ERR("open(): SPI port %d out of range.\n", port);
		return NULL;
	}

	sdp = &spi_ports[port];

	if(!(port_probe_mask & (1 << port))) {
		LL_ERR("%s: SPI port %d not setup, was not in probe mask (%02x).\n", sdp->spix.name, port, port_probe_mask);
		return NULL;
	}

	if(sdp->fd >= 0) {
		LL_ERR("%s: SPI port already open.\n", sdp->spix.name);
		return NULL;
	}

	if(port_configure(sdp, args) < 0)
		return NULL;

	LL_INFO("%s: write clock rate calculated: %u Hz\n", sdp->spix.name, sdp->clkw);
	LL_INFO("%s: read clock rate calculated: %u Hz\n",  sdp->spix.name, sdp->clkr);

	return &sdp->spix;
}

/*
 * Close a SPI port.
 */
static int spidev_close(const spix_port_t *sp)
{
	spidev_port_t *sdp = (spidev_port_t *)sp;

	if(!driver_enabled) {
		LL_ERR("Driver is not setup.\n");
		return -ENODEV;
	}

	if(!sp) {
		LL_ERR("close(): trying to close port NULL\n");
		return -EINVAL;
	}

	if(sdp->fd < 0) {
		LL_ERR("%s: close(): SPI port not open.\n", sdp->spix.name);
		return -ENODEV;
	}

	close(sdp->fd);
	sdp->fd = -1;

	return 0;
}

// vim: ts=4
