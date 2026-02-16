//    Copyright 2014-2026 Jeff Epler
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
#include <stdatomic.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/ppdev.h>
#include <rtapi.h>
#include <rtapi_parport.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __linux__
#include <sys/fsuid.h>
#endif
#include <unistd.h>

#define MAX_PARPORTS 16

struct portinfo {
    int port_id;
    unsigned short base;
    unsigned short base_hi;
    int in_use;
};

static struct portinfo parports[MAX_PARPORTS];
static int parports_initialized = 0;

static uid_t euid, ruid;
static _Atomic int with_root_level = 0;

static void with_root_enter(void) {
    if(atomic_fetch_add(&with_root_level, 1) == 0) {
#ifdef __linux__
        setfsuid(euid);
#endif
    }
}

static void with_root_exit(void) {
    if(atomic_fetch_sub(&with_root_level, 1) == 1) {
#ifdef __linux__
        setfsuid(ruid);
#endif
    }
}

static void map_parports(void) {
    int i;
    for(i=0; i<MAX_PARPORTS; i++) {
        const char path_template[] = "/proc/sys/dev/parport/parport%d/base-addr";
        char path[sizeof(path_template)];
        snprintf(path, sizeof(path), path_template, i);

        FILE *f = fopen(path, "r");
        if(!f) {
            if(errno != ENOENT)
                rtapi_print_msg(RTAPI_MSG_ERR, "fopen(%s): %s\n", path, strerror(errno));
            continue;
        }
        struct portinfo pi;
        pi.port_id = i;
        pi.in_use = 0;
        if(fscanf(f, "%hd %hd", &pi.base, &pi.base_hi) != 2) {
            rtapi_print_msg(RTAPI_MSG_ERR, "Failed to parse base-addr for port #%d\n", i);
            fclose(f);
            continue;
        }
        fclose(f);

        /* Store by port ID and by base address */
        pi.in_use = 1;
        parports[i] = pi;
    }
    parports_initialized = 1;
}

static struct portinfo* find_parport_by_base(unsigned short base) {
    int i;
    for(i=0; i<MAX_PARPORTS; i++) {
        if(parports[i].in_use && (parports[i].base == base || parports[i].port_id == base)) {
            return &parports[i];
        }
    }
    return NULL;
}

int rtapi_parport_get(const char *mod_name, rtapi_parport_t *port, unsigned short base, unsigned short base_hi, unsigned int modes) {
    struct portinfo *pi;
    
    with_root_enter();

    memset(port, 0, sizeof(*port));
    port->fd = -1;

    if(!parports_initialized) map_parports();
    
    pi = find_parport_by_base(base);
    if(pi == NULL) {
        if(base < 16){
            rtapi_print_msg(RTAPI_MSG_ERR, "Linux parallel port %d not found\n", base);
            with_root_exit();
            return -ENOENT;}
        rtapi_print_msg(RTAPI_MSG_ERR, "No parport registered at 0x%x. "
                    "This is not always an error. Continuing.\n", base);
        port->base = base;
        port->base_hi = base_hi;
        with_root_exit();
        return 0;
    } else {
        const char port_template[] = "/dev/parport%d";
        char port_path[sizeof(port_template)];
        int ppmodes;
        
        port->base = pi->base;
        port->base_hi = pi->base_hi;

        snprintf(port_path, sizeof(port_path), port_template, pi->port_id);
        port->fd = open(port_path, O_RDWR);

        if(port->fd < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "open(%s): %s\n", port_path, strerror(errno));
            with_root_exit();
            return -errno;
        }

        if(ioctl(port->fd, PPCLAIM) < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "ioctl(%s, PPCLAIM): %s\n", port_path, strerror(errno));
            close(port->fd);
            port->fd = -1;
            with_root_exit();
            return -errno;
        }

        ppmodes = ~0;

        if(ioctl(port->fd, PPGETMODES, &ppmodes) < 0) {
            rtapi_print_msg(RTAPI_MSG_WARN, "ioctl(%s, PPGETMODES): %s\n", port_path, strerror(errno));
        }

        if((modes & ppmodes) != modes)
        {
            rtapi_print_msg(RTAPI_MSG_WARN,
                "PARPORT: linux parport %s does not support mode %x.\n"
                "PARPORT: continuing anyway.\n",
                port_path, modes);
        }

    }
    with_root_exit();
    return 0;
}

void rtapi_parport_release(rtapi_parport_t *port) {
    close(port->fd);
    port->fd = -1;
}

/* Initialize euid/ruid for with_root functions */
void __attribute__((constructor)) rtapi_parport_init(void) {
    euid = geteuid();
    ruid = getuid();
}
