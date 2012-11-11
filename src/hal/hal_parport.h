//    Copyright (C) 2009 Jeff Epler
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
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

#ifndef HAL_PARPORT_COMMON_H
#define HAL_PARPORT_COMMON_H

#include <linux/parport.h>

typedef struct hal_parport_t
{
    unsigned short base;
    unsigned short base_hi;
    struct pardevice *linux_dev;
    void *region;
    void *region_hi;
    int dev_fd;
} hal_parport_t;


#if defined(BUILD_SYS_USER_DSO)

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/ppdev.h>

#if defined(USE_PORTABLE_PARPORT_IO)
static unsigned int hal_parport_error_count;
#else
#include <sys/io.h>
#endif

static int
hal_parport_get(int comp_id, hal_parport_t *port,
		unsigned short base, unsigned short base_hi, unsigned int modes)
{
	FILE *fd;
	char devname[64] = { 0, };
	char devpath[96] = { 0, };

	memset(port, 0, sizeof(*port));
	port->dev_fd = -1;
	port->base = base;
	port->base_hi = base_hi;

	if (base < 0x20) {
		/* base is the parallel port number. */
		snprintf(devname, sizeof(devname), "parport%u", base);
		goto found_dev;
	} else {
		char *buf = NULL;
		size_t bufsize = 0;

		/* base is the I/O port base. */
		fd = fopen("/proc/ioports", "r");
		if (!fd) {
			rtapi_print_msg(RTAPI_MSG_ERR, "Failed to open /proc/ioports: %s\n",
					strerror(errno));
			return -ENODEV;
		}
		while (getline(&buf, &bufsize, fd) > 0) {
			char *b = buf;
			unsigned int start, end;
			int count;

			while (b[0] == ' ' || b[0] == '\t') /* Strip leading whitespace */
				b++;
			count = sscanf(b, "%x-%x : %63s", &start, &end, devname);
			if (count != 3)
				continue;
			if (strncmp(devname, "parport", 7) != 0)
				continue;
			if (start == base) {
				fclose(fd);
				free(buf);
				goto found_dev;
			}
		}
		fclose(fd);
		free(buf);
	}
	rtapi_print_msg(RTAPI_MSG_ERR,
			"Did not find parallel port with base address 0x%03X\n",
			base);
	return -ENODEV;
found_dev:
	snprintf(devpath, sizeof(devpath), "/dev/%s", devname);

#if defined(USE_PORTABLE_PARPORT_IO)
	rtapi_print_msg(RTAPI_MSG_INFO, "Using parallel port %s (base 0x%03X) with ioctl I/O\n",
			devpath, base);
#else
	rtapi_print_msg(RTAPI_MSG_INFO, "Using parallel port %s (base 0x%03X) with direct inb/outb I/O\n",
			devpath, base);
#endif
	port->dev_fd = open(devpath, O_RDWR);
	if (port->dev_fd < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "Failed to open parallel port %s: %s\n",
				devpath, strerror(errno));
		return -ENODEV;
	}
	if (ioctl(port->dev_fd, PPEXCL)) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"Failed to get exclusive access to parallel port %s\n",
				devpath);
		close(port->dev_fd);
		return -ENODEV;
	}
	if (ioctl(port->dev_fd, PPCLAIM)) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"Failed to claim parallel port %s\n",
				devpath);
		close(port->dev_fd);
		return -ENODEV;
	}
	return 0;
}

static void hal_parport_release(hal_parport_t *port)
{
	if (port->dev_fd < 0)
		return;
	ioctl(port->dev_fd, PPRELEASE);
	close(port->dev_fd);
}

#if defined(USE_PORTABLE_PARPORT_IO)
static inline unsigned char hal_parport_read_status(hal_parport_t *port)
{
	unsigned char x;

	if (ioctl(port->dev_fd, PPRSTATUS, &x)) {
		if (hal_parport_error_count < 10) {
			hal_parport_error_count++;
			rtapi_print_msg(RTAPI_MSG_ERR, "Failed to read parport status.\n");
			return 0;
		}
	}

	return x;
}

static inline unsigned char hal_parport_read_data(hal_parport_t *port)
{
	unsigned char x;

	if (ioctl(port->dev_fd, PPRDATA, &x)) {
		if (hal_parport_error_count < 10) {
			hal_parport_error_count++;
			rtapi_print_msg(RTAPI_MSG_ERR, "Failed to read parport data.\n");
			return 0;
		}
	}

	return x;
}

static inline void hal_parport_write_data(hal_parport_t *port, unsigned char x)
{
	if (ioctl(port->dev_fd, PPWDATA, &x)) {
		if (hal_parport_error_count < 10) {
			hal_parport_error_count++;
			rtapi_print_msg(RTAPI_MSG_ERR, "Failed to write parport data.\n");
			return;
		}
	}
}

static inline unsigned char hal_parport_read_control(hal_parport_t *port)
{
	unsigned char x;

	if (ioctl(port->dev_fd, PPRCONTROL, &x)) {
		if (hal_parport_error_count < 10) {
			hal_parport_error_count++;
			rtapi_print_msg(RTAPI_MSG_ERR, "Failed to read parport control.\n");
			return 0;
		}
	}

	return x;
}

static inline void hal_parport_write_control(hal_parport_t *port, unsigned char x)
{
	if (ioctl(port->dev_fd, PPWCONTROL, &x)) {
		if (hal_parport_error_count < 10) {
			hal_parport_error_count++;
			rtapi_print_msg(RTAPI_MSG_ERR, "Failed to write parport control.\n");
			return;
		}
	}
}

static inline void hal_parport_set_datadir(hal_parport_t *port, int enable_inputs)
{
	enable_inputs = !!enable_inputs;
	if (ioctl(port->dev_fd, PPDATADIR, &enable_inputs)) {
		if (hal_parport_error_count < 10) {
			hal_parport_error_count++;
			rtapi_print_msg(RTAPI_MSG_ERR, "Failed to set parport data direction.\n");
			return;
		}
	}
}
#endif // defined(USE_PORTABLE_PARPORT_IO)



#else /***** REALTIME HYPERVISOR *****/

static int
hal_parport_get(int comp_id, hal_parport_t *port,
        unsigned short base, unsigned short base_hi, unsigned int modes)
{
    int retval = 0;
    struct parport *linux_port = 0;
    memset(port, 0, sizeof(hal_parport_t));

    // I/O addresses 1..16 are assumed to be linux parport numbers
    if(base < 16) {
        linux_port = parport_find_number(base);
        if(!linux_port)
        {
            rtapi_print_msg(RTAPI_MSG_ERR,
                    "PARPORT: ERROR: linux parport %d not found\n",
                    base);
            return -ENODEV;
        }
    } else {
        linux_port = parport_find_base(base);
    }

    if(linux_port)
    {
        if((modes & linux_port->modes) != modes)
        {
            rtapi_print_msg(RTAPI_MSG_WARN,
                "PARPORT: linux parport %s does not support mode %x.\n"
                "PARPORT: continuing anyway.\n",
                linux_port->name, modes);
        }
        rtapi_print_msg(RTAPI_MSG_INFO,
                  "PARPORT: Using Linux parport %s at ioaddr=0x%lx:0x%lx\n",
                  linux_port->name, linux_port->base, linux_port->base_hi);
        port->linux_dev = parport_register_device(linux_port,
                hal_comp_name(comp_id), NULL, NULL, NULL, 0, NULL);

        if(!port->linux_dev)
        {
            parport_put_port(linux_port);
            rtapi_print_msg(RTAPI_MSG_ERR,
                "PARPORT: ERROR: port %s register failed\n", linux_port->name);
            return -EIO;
        }

        retval = parport_claim(port->linux_dev);
        if(retval < 0)
        {
            parport_put_port(linux_port);
            parport_unregister_device(port->linux_dev);
            rtapi_print_msg(RTAPI_MSG_ERR,
                "PARPORT: ERROR: port %s claim failed\n", linux_port->name);
            return retval;
        }

        port->base = linux_port->base;
        if(linux_port->base_hi > 0) {
            port->base_hi = linux_port->base_hi;
        } else if(base_hi != (unsigned short)-1) {
            if(base_hi == 0) base_hi = port->base + 0x400;
            rtapi_print_msg(RTAPI_MSG_DBG,
                "PARPORT: DEBUG: linux reports no ioaddr_hi, using 0x%x",
                base_hi);
            port->region_hi =
                rtapi_request_region(base_hi, 3, hal_comp_name(comp_id));
            if(port->region_hi) {
                rtapi_print_msg(RTAPI_MSG_DBG,
                    "PARPORT: DEBUG: got requested region starting at 0x%x",
                    base_hi);
                port->base_hi = base_hi;
            } else {
                rtapi_print_msg(RTAPI_MSG_DBG,
                    "PARPORT: DEBUG: did not get requested region starting at 0x%x",
                    base_hi);
            }
        }
        parport_put_port(linux_port);
    } else {
        if(base_hi == 0) base_hi = base + 0x400;

        port->base = base;
        rtapi_print_msg(RTAPI_MSG_INFO,
                  "Using direct parport at ioaddr=0x%x:0x%x\n", base, base_hi);

        // SPP access needs only 3 bytes, but EPP needs 8.  Allocating 8
        // is likely to always be OK, and it simplifies things (since the
        // exact allocation size is also needed at deallocation).
        port->region = rtapi_request_region(base, 8, hal_comp_name(comp_id));
        if(!port->region)
        {
            rtapi_print_msg(RTAPI_MSG_ERR,
                "PARPORT: ERROR: request_region(0x%x) failed\n", base);
            return -EBUSY;
        }

        if(base_hi != (unsigned short)-1)
        {
            port->base_hi = base_hi;
            port->region_hi =
                rtapi_request_region(base_hi, 3, hal_comp_name(comp_id));
            if(!port->region_hi)
            {
                rtapi_print_msg(RTAPI_MSG_ERR,
                    "PARPORT: ERROR: request_region(0x%x) failed\n", base_hi);
                rtapi_release_region(port->base, 8);
                return -EBUSY;
            }
        }
    }
    return 0;
}

void hal_parport_release(hal_parport_t *port)
{
    if(port->linux_dev)
    {
        rtapi_print_msg(RTAPI_MSG_INFO,
	    "PARPORT: Releasing Linux parport at ioaddr=0x%lx:0x%lx\n",
	    port->linux_dev->port->base, port->linux_dev->port->base_hi);
        parport_release(port->linux_dev);
        parport_unregister_device(port->linux_dev);
    }
    if(port->region) {
        rtapi_print_msg(RTAPI_MSG_INFO,
	    "PARPORT: Releasing I/O region ioaddr=0x%x\n", port->base);
	rtapi_release_region(port->base, 8);
    }
    if(port->region_hi) {
        rtapi_print_msg(RTAPI_MSG_INFO,
	    "PARPORT: Releasing high I/O region ioaddr=0x%x\n",
                port->base_hi);
	rtapi_release_region(port->base_hi, 3);
    }
    memset(port, 0, sizeof(hal_parport_t));
}

static inline unsigned char hal_parport_read_status(hal_parport_t *port)
{
	return rtapi_inb(port->base + 1);
}

static inline unsigned char hal_parport_read_data(hal_parport_t *port)
{
	return rtapi_inb(port->base + 0);
}

static inline void hal_parport_write_data(hal_parport_t *port, unsigned char x)
{
	rtapi_outb(x, port->base + 0);
}

static inline unsigned char hal_parport_read_control(hal_parport_t *port)
{
	return rtapi_inb(port->base + 2);
}

static inline void hal_parport_write_control(hal_parport_t *port, unsigned char x)
{
	rtapi_outb(x, port->base + 2);
}

static inline void hal_parport_set_datadir(hal_parport_t *port, int enable_inputs)
{
	if (enable_inputs)
		hal_parport_write_control(port, hal_parport_read_control(port) | 0x20);
	else
		hal_parport_write_control(port, hal_parport_read_control(port) & ~0x20);
}

#endif /* BUILD_SYS_USER_DSO */

#endif
