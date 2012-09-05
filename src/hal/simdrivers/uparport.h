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

#ifdef SIM

// drop root privilegs after init
// this is a stopgap measure until rtapi_app knows how to handle this
#define DROP_PRIV 1 

#define MAX_PARPORT 10 // a tad on the high side
struct ppres  {
    unsigned int state;
    unsigned int reg_start;
    unsigned int reg_end;
    unsigned int irq;
};
#else
#include <linux/parport.h>
#endif
typedef struct hal_parport_t
{
    unsigned short base;
    unsigned short base_hi;
#ifndef SIM
    struct pardevice *linux_dev;
#else
    struct ppres pp_res; // retrieved from sysfs
    int parport_fd;  // for ioctl()
#endif
    void *region;
    void *region_hi;
} hal_parport_t;

#ifdef SIM
float cpu_MHz(void) 
{
    char *path = "/proc/cpuinfo",  *s, line[1024];
    float freq;
    char *cpu_line = "cpu MHz";

    // parse /proc/cpuinfo for the line:
    // cpu MHz		: 2378.041
    FILE *f = fopen(path,"r");
    if (!f) {
	perror(path);
	return -1.0;
    }
    while (fgets(line, sizeof(line), f)) {
	if (!strncmp(line, cpu_line, strlen(cpu_line))) {
	    s = strchr(line, ':');
	    if (s && 1 == sscanf(s, ":%g", &freq)) {
		fclose(f);
		return freq;
	    }
	}
    }
    fclose(f);
    return -1.0;
}

int get_ppdev_res(int dev, struct ppres *ppres)
{
    FILE *f;
    char path[1024], value[100],line[1024], *s;
    unsigned from, to;

    // parse this:
    // $ cat /sys/class/ppdev/parport0/device/resources 
    // state = active
    // io 0x378-0x37f
    // irq 7
    ppres->state = 0;
    ppres->reg_start = 0;
    ppres->reg_end = 0;
    ppres->irq = 0;

    sprintf(path,"/sys/class/ppdev/parport%d/device/resources",
	    dev);

    f = fopen(path, "r");
    if (!f) {
	return -1;
    }
    while (fgets(line, sizeof(line), f)) {
	if (!strncmp(line, "state", 5)) {
	    s = strchr(line, '=');
	    if (s && 1 == sscanf(s, "= %s", value)) {
		ppres->state = !strcmp(value,"active");
	    } else  {
		rtapi_print_msg(RTAPI_MSG_ERR, 
				"get_ppdev_res: cant parse '%s'\n",
				line);
		fclose(f);
		return -1;
	    }
	}
	if (!strncmp(line, "irq", 3)) {
	    if (1 != sscanf(line+4, " %d", &ppres->irq)) {
		rtapi_print_msg(RTAPI_MSG_ERR, 
				"get_ppdev_res: cant parse '%s'\n",
				line);
		fclose(f);
		return -1;
	    }
	}
	if (!strncmp(line, "io", 2)) {
	    if (2 != sscanf(line+2, " 0x%x-0x%x", &from, &to)) {
		rtapi_print_msg(RTAPI_MSG_ERR,
				"get_ppdev_res: cant parse '%s'\n",
				line);
		fclose(f);
		return -1;
		    
	    } else {
		ppres->reg_start = from;
		ppres->reg_end = to;
	    }
	}
    }
    fclose(f);
    return 0;
}
#endif

static int
hal_parport_get(int comp_id, hal_parport_t *port,
        unsigned short base, unsigned short base_hi, unsigned int modes)
{
    int i, direction;
    struct ppres pp_res;
    char path[PATH_MAX];
#ifndef SIM
    int retval = 0;
    struct parport *linux_port = 0;
#endif
    memset(port, 0, sizeof(hal_parport_t));

    // investigate sysfs for ppdev-created parport%d devices
    for (i = 0; i < MAX_PARPORT; i++) {
	if (!get_ppdev_res(i, &pp_res)) {
	    if (base == i)
		break;
	    if (pp_res.reg_start == base)
		break;
	} 
    }
    if (i == MAX_PARPORT) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"PARPORT: ERROR: cant find port %d (0x%x)\n",
			base, base);
	return -ENODEV;
    }
    rtapi_print_msg(RTAPI_MSG_ERR,
		    "PARPORT: using base=0x%x\n",
		    pp_res.reg_start);

    snprintf(path, sizeof(path),"/dev/parport%d", i);
    if ((port->parport_fd = open(path,O_RDWR)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"PARPORT: ERROR: cant open %s: %s\n", 
			path, strerror(errno));
	return -EPERM;
    }
    if (ioctl(port->parport_fd, PPCLAIM) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"PARPORT: ERROR: cant reserve %s: %s\n", 
			path, strerror(errno));
	return -EPERM;
    }
    if (ioperm(pp_res.reg_start, 3, 1) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"PARPORT: ERROR: cant get access to 0x%x (not running as root?)\n",
			pp_res.reg_start);
	return -EPERM;
    }
    port->base = pp_res.reg_start;
    port->base_hi = port->base + 0x400; // dubious..

    direction = modes == PARPORT_MODE_TRISTATE;
    if (ioctl(port->parport_fd, PPDATADIR, &direction) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"PARPORT: ERROR: cant set direction (PPDATADIR) on 0x%x\n",
			pp_res.reg_start);
	return -EPERM;
    }

#ifdef DROP_PRIV
    // not a good place, but good until simdriver #2..
    rtapi_print_msg(RTAPI_MSG_ERR,
		    "PARPORT: dropping root permissions to uid %d\n",
		    getuid());
    setuid(getuid()); 
#endif

#ifndef SIM
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
#endif
    return 0;
}

void hal_parport_release(hal_parport_t *port)
{
#ifndef SIM
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
#else
    if (ioperm(port->pp_res.reg_start, 3, 0) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"PARPORT: ERROR: releasing ioperm(0x%x)\n",
			port->pp_res.reg_start);
    }
    if (ioctl(port->parport_fd, PPRELEASE) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"PARPORT: ERROR: releasing parport 0x%x: %s\n", 
			port->pp_res.reg_start, strerror(errno));
    }
#endif

}
#endif
