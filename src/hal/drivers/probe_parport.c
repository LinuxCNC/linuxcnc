/********************************************************************
* Description:  probe_parport.c
*               This file, 'probe_parport.c', is a HAL component that 
*               does "PNP" probing for the standard PC parallel port.
*               If your parallel port does not work with emc2, it may if you
*               load this driver before you load hal_parport, e.g.:
*                   loadrt probe_parport
*                   loadrt hal_parport cfg=378
*
*               This driver is particularly likely to be helpful if a message
*               similar to this one is logged in 'dmesg' when you 'sudo
*               modprobe -i parport_pc':
*                   parport: PnPBIOS parport detected.
*
* Author: Jeff Epler
* License: GPL Version 2
*    
* Copyright (c) 2006 All rights reserved.
*
* Last change: 
********************************************************************/

/* Low-level parallel-port routines for 8255-based PC-style hardware.
 * 
 * Authors: Phil Blundell <philb@gnu.org>
 *          Tim Waugh <tim@cyberelk.demon.co.uk>
 *	    Jose Renau <renau@acm.org>
 *          David Campbell <campbell@torque.net>
 *          Andrea Arcangeli
 *
 * based on work by Grant Guenther <grant@torque.net> and Phil Blundell.
 *
 * Cleaned up include files - Russell King <linux@arm.uk.linux.org>
 * DMA support - Bert De Jonghe <bert@sophis.be>
 * Many ECP bugs fixed.  Fred Barnes & Jamie Lokier, 1999
 * More PCI support now conditional on CONFIG_PCI, 03/2001, Paul G. 
 * Various hacks, Fred Barnes, 04/2001
 * Updated probing logic - Adam Belay <ambx1@neo.rr.com>
 */

#include "config.h"
#include "hal.h"
#include "rtapi.h"
#include "rtapi_app.h"

static int comp_id;

#if defined(BUILD_SYS_USER_DSO)

int rtapi_app_main(void) {
    comp_id = hal_init("probe_parport");
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "PROBE_PARPORT: ERROR: hal_init() failed\n");
        return -1;
    }
    hal_ready(comp_id);

    return 0;
}

void rtapi_app_exit(void) {
    hal_exit(comp_id);
}

#else

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/pnp.h>
#include <linux/sysctl.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/uaccess.h>

#include <linux/via.h>

MODULE_AUTHOR("Jeff Epler");
MODULE_DESCRIPTION("Parallel Port PNP driver for EMC HAL");
MODULE_LICENSE("GPL");

static int pnp_registered_parport = 0;

static const struct pnp_device_id probe_parport_pnp_tbl[] = {
	/* Standard LPT Printer Port */
	{.id = "PNP0400", .driver_data = 0},
	/* ECP Printer Port */
	{.id = "PNP0401", .driver_data = 0},
	{ }
};

MODULE_DEVICE_TABLE(pnp,probe_parport_pnp_tbl);

static int probe_parport_pnp_probe(struct pnp_dev *dev, const struct pnp_device_id *id)
{
	unsigned long io_lo, io_hi;

	if (pnp_port_valid(dev,0) &&
		!(pnp_port_flags(dev,0) & IORESOURCE_DISABLED)) {
		io_lo = pnp_port_start(dev,0);
	} else
		return -EINVAL;

	if (pnp_port_valid(dev,1) &&
		!(pnp_port_flags(dev,1) & IORESOURCE_DISABLED)) {
		io_hi = pnp_port_start(dev,1);
	} else
		io_hi = 0;

	rtapi_print_msg(RTAPI_MSG_INFO, "parport: PnPBIOS parport detected, io_lo=%lx io_hi=%lx\n",
                io_lo, io_hi);

	pnp_set_drvdata(dev,NULL);
	return 0;
}

static void probe_parport_pnp_remove(struct pnp_dev *dev)
{
}

/* we only need the pnp layer to activate the device, at least for now */
static struct pnp_driver probe_parport_pnp_driver = {
	.name		= "probe_parport",
	.id_table	= probe_parport_pnp_tbl,
	.probe		= probe_parport_pnp_probe,
	.remove		= probe_parport_pnp_remove,
};


int rtapi_app_main(void) {
    int r;

    comp_id = hal_init("probe_parport");
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "PROBE_PARPORT: ERROR: hal_init() failed\n");
        return -1;
    }


    r = pnp_register_driver (&probe_parport_pnp_driver);
    if (r >= 0) {
        pnp_registered_parport = 1;
    } else {
        rtapi_print_msg(RTAPI_MSG_WARN, "PROBE_PARPORT: no PnPBIOS parports were detected (%d)\n", r);
    }

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) {
    if (pnp_registered_parport)
            pnp_unregister_driver (&probe_parport_pnp_driver);
    hal_exit(comp_id);
}

#endif
