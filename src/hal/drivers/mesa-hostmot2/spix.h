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
#ifndef HAL_HM2_SPIX_H
#define HAL_HM2_SPIX_H

/*
 * Select which SPI channel(s) to probe. There are many SPI interfaces exposed
 * on the 40-pin I/O header. We only use the traditional ones, SPI0 and SPI1.
 * SPI0 has four chip selects and SPI1 has three chip selects.
 *
 * Boards will be numbered in the order found. The probe scan is ordered in the
 * following way:
 * - SPI0 - CE0
 * - SPI0 - CE1
 * - SPI1 - CE0
 * - SPI1 - CE1
 * - SPI1 - CE2
 *
 * There are other possible SPI ports and CE combinations. However, most will
 * collide in one or another way with older RPi assignments and uses. Two
 * accessible ports should be more than enough for all practical uses.
 */
#define SPIX_PROBE_SPI0_CE0		(1 << 0)
#define SPIX_PROBE_SPI0_CE1		(1 << 1)
#define SPIX_PROBE_SPI0_MASK	(SPIX_PROBE_SPI0_CE0 | SPIX_PROBE_SPI0_CE1)
#define SPIX_PROBE_SPI1_CE0		(1 << 2)
#define SPIX_PROBE_SPI1_CE1		(1 << 3)
#define SPIX_PROBE_SPI1_CE2		(1 << 4)
#define SPIX_PROBE_SPI1_MASK	(SPIX_PROBE_SPI1_CE0 | SPIX_PROBE_SPI1_CE1 | SPIX_PROBE_SPI1_CE2)

/*
 * The driver is informed of any MISO pull-up/down for each port
 *
 * Note: These must follow the GPIO_GPPUD_* values defined in
 * spi_common_rpspi.h but we do not want to include that header here.
 */
enum {
	SPIX_PULL_OFF = 0,	// GPIO_GPPUD_OFF
	SPIX_PULL_DOWN = 1,	// GPIO_GPPUD_PULLDOWN
	SPIX_PULL_UP = 2,	// GPIO_GPPUD_PULLUP
};

/*
 * The base structure for the SPI port low level hardware driver. It may attach
 * any data privately.
 */
typedef struct __spix_port_t {
	int			width;		// The transfer width 8, 16 or 32 (to check bitshifted cookie)
	int			miso_pull;	// Whether the MISO line is pulled in a direction
	const char	*name;		// SPIx/CEy string (handy for messages)

	/*
	 * int transfer(spix_port_t *port, uint32_t *buffer, size_t nelem, int rw)
	 *
	 * Perform a complete SPI transfer. Transfer 'nelem' words from a buffer
	 * pointed to by 'buffer'. The argument 'rw' indicates write when 0 (zero)
	 * or read when non-zero.
	 * On success it should return 1 (one). On error it should return 0 (zero).
	 */
	int (*transfer)(const struct __spix_port_t *port, uint32_t *buffer, size_t nelem, int rw);
} spix_port_t;

#define SPIX_MAX_BOARDS	5			// One on each (traditional) CE for SPI ports 0 and 1
#define SPIX_MAX_MSG	(127+1)		// The docs say that the max. burstlen == 127 words (i.e. cmd+message <= 1+127)

/*
 * Module arguments passed to lower level
 */
typedef struct __spix_args_t {
	uint32_t	clkw;		// The requested write clock
	uint32_t	clkr;		// The requested read clock
	const char	*spidev;	// spidev only: device node path override
} spix_args_t;

/*
 * SPI low level interface to hardware drivers
 */
typedef struct __spix_driver_t {
	const char *name;			// Human indicator for board
	int			num_ports;		// How many ports this driver supports
	char		model[127+1];	// Human readable platform name
	char		dtc[127+1];		// The device-tree matched string

	/*
	 * int detect(const char *dtcs[])
	 * Detect the board supported by this driver on basis of the string-list
	 * provided via 'dtc'. The 'dtc' argument will be NULL if
	 * /proc/device-tree/compatible does not exist and the low-level driver
	 * needs to determine its own fate.
	 * The driver should return 0 (zero) if it finds itself fit for the
	 * hardware or non-zero if it does not support the hardware.
	 */
	int (*detect)(const char *dtcs[]);

	/*
	 * int setup(int probemask)
	 * Setup internal structures and data for the driver for all ports in the
	 * 'probemask' argument. The mask is an inclusive or of the SPIX_PROBE_*
	 * bit values.
	 * Returns 0 (zero) on success.
	 *
	 * Warning: calling setup() on a driver which did not acknowledge its
	 * capability to handle the hardware in detect() may cause your system to
	 * become unstable.
	 */
	int (*setup)(int probemask);

	/*
	 * int cleanup(void)
	 * Clean up internal structures and data for the driver.
	 * Returns 0 (zero) on success.
	 */
	int (*cleanup)(void);

	/*
	 * spix_port_t *open(int port, const spix_args_t *args)
	 *
	 * Open 'port' (number 0...N) with specified write clock 'clkw' and read
	 * clock 'clkr' (both in Hz). The driver will check the values.
	 * Return value is a reference to the port to use or NULL on failure.
	 */
	const spix_port_t *(*open)(int port, const spix_args_t *args);

	/*
	 * close()
	 * Close port and free all internal resources associated with the port.
	 * Returns 0 (zero) on success.
	 */
	int (*close)(const spix_port_t *port);

} spix_driver_t;


/*
 * Reads file 'fname' into 'buffer' of size 'bufsize' and returns the size read
 * or negative on error. At most 'bufsize' - 1 characters are read from the
 * file. The buffer is always NUL-terminated.
 */
ssize_t spix_read_file(const char *fname, void *buffer, size_t bufsize);

/*
 * HM2 command interface format (all 32-bit words):
 *   <cmd-word> [0..127 data-words]
 *
 * Command word format:
 *  MSB....................................LSB
 *   aaaa aaaa aaaa aaaa cccc i nnn nnnn 0000
 * - a: register address to read/write
 * - c: read (0xa) or write (0xb) command
 * - i: auto address increment enable if 1
 * - n: number of data words [1..127] to follow (burst length)
 * - 0: unused, should be zero
 */
#define SPIX_HM2_CMD_READ		0x0000a000
#define SPIX_HM2_CMD_WRITE		0x0000b000
#define SPIX_HM2_CMD_ADDRINC	0x00000800
__attribute__((always_inline)) static inline uint32_t spix_cmd_read(uint32_t addr, uint32_t msglen, int aib)
{
	return (addr << 16) | SPIX_HM2_CMD_READ | (aib ? SPIX_HM2_CMD_ADDRINC : 0) | ((msglen & 0x7f) << 4);
}

__attribute__((always_inline)) static inline uint32_t spix_cmd_write(uint32_t addr, uint32_t msglen, int aib)
{
	return (addr << 16) | SPIX_HM2_CMD_WRITE | (aib ? SPIX_HM2_CMD_ADDRINC : 0) | ((msglen & 0x7f) << 4);
}

__attribute__((always_inline)) static inline uint32_t spix_min(uint32_t a, uint32_t b)
{
	return a <= b ? a : b;
}

#endif
// vim: ts=4
