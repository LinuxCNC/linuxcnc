//
// This is a userspace HAL driver for the ShuttleXpress and ShuttlePRO
// devices by Contour Design.
//
// Copyright 2011, 2016, 2021 Sebastian Kuzminsky <seb@highlab.com>
// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
//
// Cmod port: hand-written cmod for Contour Design Shuttle USB jog devices.
//

#include "gomc_env.h"
#include "gomc_user.h"

#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/types.h>
#include <linux/hidraw.h>

#ifndef HIDIOCGRAWNAME
#define HIDIOCGRAWNAME(len) _IOC(_IOC_READ, 'H', 0x04, len)
#endif

#define MAX_BUTTONS 15
#define PACKET_LEN 5

/* --------------------------------------------------------------------------
 * Device table
 * -------------------------------------------------------------------------- */

typedef struct {
    const char *name;
    uint16_t vendor_id;
    uint16_t product_id;
    int num_buttons;
    uint16_t button_mask[MAX_BUTTONS];
} contour_dev_t;

static const contour_dev_t contour_dev[] = {
    {
        .name = "shuttlexpress",
        .vendor_id = 0x0b33,
        .product_id = 0x0020,
        .num_buttons = 5,
        .button_mask = { 0x0010, 0x0020, 0x0040, 0x0080, 0x0100 }
    },
    {
        .name = "shuttlepro",
        .vendor_id = 0x05f3,
        .product_id = 0x0240,
        .num_buttons = 13,
        .button_mask = { 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020,
                         0x0040, 0x0080, 0x0100, 0x0200, 0x0400, 0x0800, 0x1000 }
    },
    {
        .name = "shuttleproV2",
        .vendor_id = 0x0b33,
        .product_id = 0x0030,
        .num_buttons = 15,
        .button_mask = { 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020,
                         0x0040, 0x0080, 0x0100, 0x0200, 0x0400, 0x0800,
                         0x1000, 0x2000, 0x4000 }
    }
};

#define NUM_CONTOUR_DEVS (sizeof(contour_dev) / sizeof(contour_dev[0]))

/* --------------------------------------------------------------------------
 * Per-device HAL structure
 * -------------------------------------------------------------------------- */

typedef struct {
    gomc_hal_bit_t *button[MAX_BUTTONS];
    gomc_hal_bit_t *button_not[MAX_BUTTONS];
    gomc_hal_s32_t *counts;
    gomc_hal_float_t *spring_wheel_f;
    gomc_hal_s32_t *spring_wheel_s32;
} shuttle_hal_t;

typedef struct {
    int fd;
    char *device_file;
    shuttle_hal_t *hal;
    int read_first_event;
    int prev_count;
    const contour_dev_t *contour_type;
} shuttle_dev_t;

/* --------------------------------------------------------------------------
 * Module instance
 * -------------------------------------------------------------------------- */

typedef struct {
    cmod_t cmod;
    const cmod_env_t *env;
    int comp_id;
    int exit_fd;
    pthread_t thread;
    shuttle_dev_t *devices;
    int num_devices;
} shuttle_inst_t;

/* --------------------------------------------------------------------------
 * Read and process one HID packet
 * -------------------------------------------------------------------------- */

static int read_update(shuttle_dev_t *s) {
    int8_t packet[PACKET_LEN];
    int r = read(s->fd, packet, PACKET_LEN);
    if (r <= 0) return -1;

    uint16_t button = ((uint8_t)packet[4] << 8) | (uint8_t)packet[3];
    for (int i = 0; i < s->contour_type->num_buttons; i++) {
        *s->hal->button[i] = (button & s->contour_type->button_mask[i]) ? 1 : 0;
        *s->hal->button_not[i] = !*s->hal->button[i];
    }

    int curr_count = packet[1];
    if (!s->read_first_event) {
        *s->hal->counts = 0;
        s->prev_count = curr_count;
        s->read_first_event = 1;
    } else {
        int diff = curr_count - s->prev_count;
        if (diff > 128) diff -= 256;
        if (diff < -128) diff += 256;
        *s->hal->counts += diff;
        s->prev_count = curr_count;
    }

    *s->hal->spring_wheel_s32 = packet[0];
    *s->hal->spring_wheel_f = packet[0] / 7.0;
    return 0;
}

/* --------------------------------------------------------------------------
 * Main loop — select on all hidraw fds
 * -------------------------------------------------------------------------- */

static void *shuttle_loop(void *arg) {
    shuttle_inst_t *inst = (shuttle_inst_t *)arg;
    int nfds = inst->num_devices + 1;
    struct pollfd *pfds = calloc(nfds, sizeof(struct pollfd));
    if (!pfds) return NULL;

    /* First entry is the exit eventfd */
    pfds[0].fd = inst->exit_fd;
    pfds[0].events = POLLIN;
    for (int i = 0; i < inst->num_devices; i++) {
        pfds[i + 1].fd = inst->devices[i].fd;
        pfds[i + 1].events = POLLIN;
    }

    while (!gomc_should_exit(inst->exit_fd)) {
        int r = poll(pfds, nfds, 200);
        if (r < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            break;
        }
        if (pfds[0].revents & POLLIN) break; /* exit signal */

        for (int i = 0; i < inst->num_devices; i++) {
            if (pfds[i + 1].revents & POLLIN) {
                if (read_update(&inst->devices[i]) < 0) {
                    gomc_log_errorf(inst->env->log, "shuttle",
                        "error reading %s\n", inst->devices[i].device_file);
                    /* disable this fd */
                    pfds[i + 1].fd = -1;
                }
            }
        }
    }

    free(pfds);
    return NULL;
}

/* --------------------------------------------------------------------------
 * Probe a single hidraw device
 * -------------------------------------------------------------------------- */

static const contour_dev_t *probe_device(const char *path, int *fd_out) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    struct hidraw_devinfo devinfo;
    if (ioctl(fd, HIDIOCGRAWINFO, &devinfo) < 0) {
        close(fd);
        return NULL;
    }

    for (int i = 0; i < (int)NUM_CONTOUR_DEVS; i++) {
        if (devinfo.vendor == contour_dev[i].vendor_id &&
            devinfo.product == contour_dev[i].product_id) {
            *fd_out = fd;
            return &contour_dev[i];
        }
    }

    close(fd);
    return NULL;
}

/* --------------------------------------------------------------------------
 * Create HAL pins for one device
 * -------------------------------------------------------------------------- */

static int create_pins(shuttle_inst_t *inst, shuttle_dev_t *dev, int dev_idx) {
    const gomc_hal_t *hal = inst->env->hal;
    int r;

    dev->hal = (shuttle_hal_t *)hal->malloc(hal->ctx, sizeof(shuttle_hal_t));
    if (!dev->hal) return -1;
    memset(dev->hal, 0, sizeof(shuttle_hal_t));

    for (int i = 0; i < dev->contour_type->num_buttons; i++) {
        r = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT,
            &dev->hal->button[i], inst->comp_id,
            "shuttle.%d.button-%d", dev_idx, i);
        if (r != 0) return -1;

        r = gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT,
            &dev->hal->button_not[i], inst->comp_id,
            "shuttle.%d.button-%d-not", dev_idx, i);
        if (r != 0) return -1;
        *dev->hal->button_not[i] = 1;
    }

    r = gomc_hal_pin_s32_newf(hal, GOMC_HAL_OUT,
        &dev->hal->counts, inst->comp_id, "shuttle.%d.counts", dev_idx);
    if (r != 0) return -1;

    r = gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT,
        &dev->hal->spring_wheel_f, inst->comp_id,
        "shuttle.%d.spring-wheel-f", dev_idx);
    if (r != 0) return -1;

    r = gomc_hal_pin_s32_newf(hal, GOMC_HAL_OUT,
        &dev->hal->spring_wheel_s32, inst->comp_id,
        "shuttle.%d.spring-wheel-s32", dev_idx);
    if (r != 0) return -1;

    return 0;
}

/* --------------------------------------------------------------------------
 * cmod lifecycle
 * -------------------------------------------------------------------------- */

static void shuttle_destroy(cmod_t *self) {
    shuttle_inst_t *inst = (shuttle_inst_t *)self;

    if (inst->exit_fd >= 0) {
        uint64_t val = 1;
        if (write(inst->exit_fd, &val, sizeof(val)) != sizeof(val)) { /* ignore */ }
        pthread_join(inst->thread, NULL);
        close(inst->exit_fd);
    }

    for (int i = 0; i < inst->num_devices; i++) {
        if (inst->devices[i].fd >= 0)
            close(inst->devices[i].fd);
        free(inst->devices[i].device_file);
    }
    free(inst->devices);

    if (inst->comp_id > 0)
        inst->env->hal->exit(inst->env->hal->ctx, inst->comp_id);
    inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
}

static int shuttle_start(cmod_t *self) {
    shuttle_inst_t *inst = (shuttle_inst_t *)self;
    if (pthread_create(&inst->thread, NULL, shuttle_loop, inst) != 0)
        return -1;
    return 0;
}

static void shuttle_stop(cmod_t *self) {
    shuttle_inst_t *inst = (shuttle_inst_t *)self;
    if (inst->exit_fd >= 0) {
        uint64_t val = 1;
        if (write(inst->exit_fd, &val, sizeof(val)) != sizeof(val)) { /* ignore */ }
        pthread_join(inst->thread, NULL);
    }
}

/* --------------------------------------------------------------------------
 * Constructor
 * -------------------------------------------------------------------------- */

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out) {
    (void)name;

    shuttle_inst_t *inst = (shuttle_inst_t *)env->rtapi->calloc(
        env->rtapi->ctx, sizeof(shuttle_inst_t));
    if (!inst) return -1;

    inst->cmod.Start = shuttle_start;
    inst->cmod.Stop = shuttle_stop;
    inst->cmod.Destroy = shuttle_destroy;
    inst->env = env;
    inst->exit_fd = -1;

    inst->comp_id = env->hal->init(env->hal->ctx, "shuttle",
                                   env->dl_handle, GOMC_HAL_COMP_USER);
    if (inst->comp_id < 0) {
        env->rtapi->free(env->rtapi->ctx, inst);
        return -1;
    }

    /* Determine device list: from argv or glob /dev/hidraw* */
    char **names = NULL;
    int num_names = 0;
    glob_t glob_buf;
    int glob_used = 0;

    if (argc > 0) {
        /* Devices passed as arguments */
        names = (char **)argv;
        num_names = argc;
    } else {
        int r = glob("/dev/hidraw*", 0, NULL, &glob_buf);
        if (r == 0) {
            names = glob_buf.gl_pathv;
            num_names = glob_buf.gl_pathc;
            glob_used = 1;
        }
    }

    /* Probe each device */
    for (int i = 0; i < num_names; i++) {
        int fd;
        const contour_dev_t *type = probe_device(names[i], &fd);
        if (!type) continue;

        inst->num_devices++;
        inst->devices = realloc(inst->devices,
            inst->num_devices * sizeof(shuttle_dev_t));
        if (!inst->devices) goto fail;

        shuttle_dev_t *dev = &inst->devices[inst->num_devices - 1];
        memset(dev, 0, sizeof(*dev));
        dev->fd = fd;
        dev->device_file = strdup(names[i]);
        dev->contour_type = type;

        if (create_pins(inst, dev, inst->num_devices - 1) != 0)
            goto fail;

        gomc_log_infof(env->log, "shuttle", "found %s on %s\n",
            type->name, names[i]);
    }

    if (glob_used) globfree(&glob_buf);

    if (inst->num_devices == 0) {
        gomc_log_errorf(env->log, "shuttle", "no devices found\n");
        goto fail;
    }

    inst->exit_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (inst->exit_fd < 0) goto fail;

    env->hal->ready(env->hal->ctx, inst->comp_id);
    *out = &inst->cmod;
    return 0;

fail:
    if (glob_used) globfree(&glob_buf);
    shuttle_destroy(&inst->cmod);
    return -1;
}
