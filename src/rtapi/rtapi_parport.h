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
#ifndef RTAPI_PARPORT_H
#define RTAPI_PARPORT_H

#include <rtapi.h>
#include <rtapi_io.h>
#include <linux/parport.h>

#define RTAPI_PARPORT_DATA_PORT(t) (t->base + 0)
#define RTAPI_PARPORT_STATUS_PORT(t) (t->base + 1)
#define RTAPI_PARPORT_CONTROL_PORT(t) (t->base + 2)
#define RTAPI_PARPORT_EPP_ADDR_PORT(t) (t->base + 3)
#define RTAPI_PARPORT_EPP_DATA_PORT(t) (t->base + 4)
#define RTAPI_PARPORT_ECR_PORT(t) (t->base_hi + 2)

typedef struct rtapi_parport_t
{
    unsigned short base;
    unsigned short base_hi;
#ifdef __KERNEL__
    struct pardevice *linux_dev;
    void *region;
    void *region_hi;
#else
    int fd;
#endif
} rtapi_parport_t;

RTAPI_BEGIN_DECLS

static inline int rtapi_parport_data_read(rtapi_parport_t *t) {
    return rtapi_inb(RTAPI_PARPORT_DATA_PORT(t));
}

static inline int rtapi_parport_control_read(rtapi_parport_t *t) {
    return rtapi_inb(RTAPI_PARPORT_CONTROL_PORT(t));
}

static inline int rtapi_parport_status_read(rtapi_parport_t *t) {
    return rtapi_inb(RTAPI_PARPORT_STATUS_PORT(t));
}

static inline unsigned char rtapi_parport_epp_data_readb(rtapi_parport_t *t) {
    return rtapi_inb(RTAPI_PARPORT_EPP_DATA_PORT(t));
}

static inline unsigned long rtapi_parport_epp_data_readl(rtapi_parport_t *t) {
    return rtapi_inl(RTAPI_PARPORT_EPP_DATA_PORT(t));
}

static inline unsigned long rtapi_parport_ecr_read(rtapi_parport_t *t) {
    return rtapi_inb(RTAPI_PARPORT_ECR_PORT(t));
}


static inline void rtapi_parport_data_write(rtapi_parport_t *t, unsigned char v) {
    rtapi_outb(v, RTAPI_PARPORT_DATA_PORT(t));
}

static inline void rtapi_parport_control_write(rtapi_parport_t *t, unsigned char v) {
    rtapi_outb(v, RTAPI_PARPORT_CONTROL_PORT(t));
}

static inline void rtapi_parport_status_write(rtapi_parport_t *t, unsigned char v) {
    rtapi_outb(v, RTAPI_PARPORT_STATUS_PORT(t));
}

static inline void rtapi_parport_epp_addr_write(rtapi_parport_t *t, unsigned char v) {
    rtapi_outb(v, RTAPI_PARPORT_EPP_ADDR_PORT(t));
}

static inline void rtapi_parport_epp_data_writeb(rtapi_parport_t *t, unsigned char v) {
    rtapi_outb(v, RTAPI_PARPORT_EPP_DATA_PORT(t));
}

static inline void rtapi_parport_epp_data_writel(rtapi_parport_t *t, unsigned long v) {
    rtapi_outl(v, RTAPI_PARPORT_EPP_DATA_PORT(t));
}

static inline void rtapi_parport_ecr_write(rtapi_parport_t *t, unsigned char v) {
    rtapi_outb(v, RTAPI_PARPORT_EPP_ADDR_PORT(t));
}

#ifdef __KERNEL__
static int
rtapi_parport_get(const char *name, rtapi_parport_t *port,
        unsigned short base, unsigned short base_hi, unsigned int modes)
{
    int retval = 0;
    struct parport *linux_port = 0;
    memset(port, 0, sizeof(rtapi_parport_t));

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
                name, NULL, NULL, NULL, 0, NULL);

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
                rtapi_request_region(base_hi, 3, name);
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
        port->region = rtapi_request_region(base, 8, name);
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
                rtapi_request_region(base_hi, 3, name);
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

static void rtapi_parport_release(rtapi_parport_t *port)
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
    memset(port, 0, sizeof(rtapi_parport_t));
}
#else
int rtapi_parport_get(const char *mod_name, rtapi_parport_t *port, unsigned short base, unsigned short base_hi, unsigned int modes);
void rtapi_parport_release(rtapi_parport_t *port);
#endif

RTAPI_END_DECLS

#endif
