//    Copyright 2014-2026 Jeff Epler <jepler@unpythonic.net>
//    Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
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

// Standalone librtapi_parport.so — no hal/rtapi dependency.
// Provides rtapi_parport_get() and rtapi_parport_release() for EPP parallel
// port access on x86/x86_64 Linux systems.

#include <stdatomic.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/ppdev.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fsuid.h>
#include <unistd.h>

#include "rtapi_parport.h"

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
    if(atomic_fetch_add(&with_root_level, 1) == 0)
        setfsuid(euid);
}

static void with_root_exit(void) {
    if(atomic_fetch_sub(&with_root_level, 1) == 1)
        setfsuid(ruid);
}

static void map_parports(void) {
    for(int i = 0; i < MAX_PARPORTS; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/proc/sys/dev/parport/parport%d/base-addr", i);

        FILE *f = fopen(path, "r");
        if(!f) {
            if(errno != ENOENT)
                fprintf(stderr, "rtapi_parport: fopen(%s): %s\n", path, strerror(errno));
            continue;
        }
        struct portinfo pi;
        pi.port_id = i;
        pi.in_use = 0;
        if(fscanf(f, "%hd %hd", &pi.base, &pi.base_hi) != 2) {
            fprintf(stderr, "rtapi_parport: failed to parse base-addr for port #%d\n", i);
            fclose(f);
            continue;
        }
        fclose(f);
        pi.in_use = 1;
        parports[i] = pi;
    }
    parports_initialized = 1;
}

static struct portinfo *find_parport_by_base(unsigned short base) {
    for(int i = 0; i < MAX_PARPORTS; i++) {
        if(parports[i].in_use && (parports[i].base == base || parports[i].port_id == base))
            return &parports[i];
    }
    return NULL;
}

int rtapi_parport_get(const char *mod_name, rtapi_parport_t *port,
                      unsigned short base, unsigned short base_hi,
                      unsigned int modes)
{
    struct portinfo *pi;

    with_root_enter();

    memset(port, 0, sizeof(*port));
    port->fd = -1;

    if(!parports_initialized)
        map_parports();

    pi = find_parport_by_base(base);
    if(pi == NULL) {
        if(base < 16) {
            fprintf(stderr, "rtapi_parport: Linux parallel port %d not found\n", base);
            with_root_exit();
            return -ENOENT;
        }
        fprintf(stderr, "rtapi_parport: no parport registered at 0x%x. "
                "This is not always an error. Continuing.\n", base);
        port->base = base;
        port->base_hi = base_hi;
        with_root_exit();
        return 0;
    }

    char port_path[32];
    snprintf(port_path, sizeof(port_path), "/dev/parport%d", pi->port_id);

    port->base = pi->base;
    port->base_hi = pi->base_hi;

    port->fd = open(port_path, O_RDWR);
    if(port->fd < 0) {
        fprintf(stderr, "rtapi_parport: open(%s): %s\n", port_path, strerror(errno));
        with_root_exit();
        return -errno;
    }

    if(ioctl(port->fd, PPCLAIM) < 0) {
        fprintf(stderr, "rtapi_parport: ioctl(%s, PPCLAIM): %s\n", port_path, strerror(errno));
        close(port->fd);
        port->fd = -1;
        with_root_exit();
        return -errno;
    }

    int ppmodes = ~0;
    if(ioctl(port->fd, PPGETMODES, &ppmodes) < 0) {
        fprintf(stderr, "rtapi_parport: ioctl(%s, PPGETMODES): %s\n", port_path, strerror(errno));
    }

    if((modes & (unsigned)ppmodes) != modes) {
        fprintf(stderr, "rtapi_parport: linux parport %s does not support mode %x. "
                "Continuing anyway.\n", port_path, modes);
    }

    with_root_exit();
    return 0;
}

void rtapi_parport_release(rtapi_parport_t *port) {
    if(port->fd >= 0) {
        close(port->fd);
        port->fd = -1;
    }
}

void __attribute__((constructor)) rtapi_parport_init(void) {
    euid = geteuid();
    ruid = getuid();
}
