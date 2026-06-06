/*
   XHC-HB04 Wireless MPG pendant LinuxCNC HAL module

   Copyright (C) 2013 Frederic Rible (frible@teaser.fr)
   Copyright (C) 2013 Rene Hopf (renehopf@mac.com)
   Copyright (C) 2014 Marius Alksnys (marius.alksnys@gmail.com)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the program; if not, write to the Free
   Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301 USA.

   Cmod port: hand-written cmod for XHC-HB04 wireless MPG pendant.
 */

#include "gomc_env.h"
#include "gomc_user.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <libusb-1.0/libusb.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <poll.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/time.h>

#include "iniparse.h"

/* -------------------------------------------------------------------------- */

#define NB_MAX_BUTTONS 32
#define STEPSIZE_BYTE 35
#define FLAGS_BYTE    36

#define DISPLAY_HOME_ICON           0x10
#define DISPLAY_HEIGHT_SETTING_ICON 0x20

#define STEPSIZE_DISPLAY_0          0x00
#define STEPSIZE_DISPLAY_1          0x01
#define STEPSIZE_DISPLAY_5          0x02
#define STEPSIZE_DISPLAY_10         0x03
#define STEPSIZE_DISPLAY_20         0x04
#define STEPSIZE_DISPLAY_30         0x05
#define STEPSIZE_DISPLAY_40         0x06
#define STEPSIZE_DISPLAY_50         0x07
#define STEPSIZE_DISPLAY_100        0x08
#define STEPSIZE_DISPLAY_500        0x09
#define STEPSIZE_DISPLAY_1000       0x0A
#define STEPSIZE_DISPLAY_P6         0x0B

#define MAX_STEPSIZE_SEQUENCE 10

#define STEP_UNDEFINED  -1
#define STEP_NONE      0x0
#define STEP_UP        0x1
#define STEP_DOWN      0x2

/* -------------------------------------------------------------------------- */

typedef enum {
    axis_off     = 0x00,
    axis_x       = 0x11,
    axis_y       = 0x12,
    axis_z       = 0x13,
    axis_a       = 0x18,
    axis_spindle = 0x14,
    axis_feed    = 0x15
} xhc_axis_t;

typedef struct {
    char pin_name[256];
    unsigned int code;
} xhc_button_t;

typedef struct {
    gomc_hal_float_t *x_wc, *y_wc, *z_wc, *a_wc;
    gomc_hal_float_t *x_mc, *y_mc, *z_mc, *a_mc;
    gomc_hal_float_t *feedrate_override, *feedrate;
    gomc_hal_float_t *spindle_override, *spindle_rps;
    gomc_hal_bit_t *button_pin[NB_MAX_BUTTONS];
    gomc_hal_bit_t *jog_enable_off;
    gomc_hal_bit_t *jog_enable_x;
    gomc_hal_bit_t *jog_enable_y;
    gomc_hal_bit_t *jog_enable_z;
    gomc_hal_bit_t *jog_enable_a;
    gomc_hal_bit_t *jog_enable_feedrate;
    gomc_hal_bit_t *jog_enable_spindle;
    gomc_hal_float_t *jog_scale;
    gomc_hal_s32_t *jog_counts, *jog_counts_neg;
    gomc_hal_float_t *jog_velocity;
    gomc_hal_float_t *jog_max_velocity;
    gomc_hal_float_t *jog_increment;
    gomc_hal_bit_t *jog_plus_x, *jog_plus_y, *jog_plus_z, *jog_plus_a;
    gomc_hal_bit_t *jog_minus_x, *jog_minus_y, *jog_minus_z, *jog_minus_a;
    gomc_hal_bit_t *stepsize_up;
    gomc_hal_bit_t *stepsize_down;
    gomc_hal_s32_t *stepsize;
    gomc_hal_bit_t *sleeping;
    gomc_hal_bit_t *connected;
    gomc_hal_bit_t *require_pendant;
    gomc_hal_bit_t *inch_icon;
    gomc_hal_bit_t *zero_x, *zero_y, *zero_z, *zero_a;
    gomc_hal_bit_t *gotozero_x, *gotozero_y, *gotozero_z, *gotozero_a;
    gomc_hal_bit_t *half_x, *half_y, *half_z, *half_a;
} xhc_hal_t;

typedef struct {
    xhc_hal_t *hal;
    xhc_axis_t axis;
    xhc_button_t buttons[NB_MAX_BUTTONS];
    unsigned char button_code;
    char old_inc_step_status;
    unsigned char button_step;
    int32_t last_jog_counts;
    struct timeval last_tv;
    struct timeval last_wakeup;
} xhc_t;

/* Stepsize sequences */
static const int stepsize_sequence_1[] = {1,10,100,1000, 0};
static const int stepsize_sequence_2[] = {1, 5, 10, 20, 0};
static const int stepsize_sequence_3[] = {1,10,100, 0};
static const int stepsize_sequence_4[] = {1, 5, 10, 20, 50, 100, 0};
static const int stepsize_sequence_5[] = {1,10, 50,100,1000, 0};

/* --------------------------------------------------------------------------
 * Module instance
 * -------------------------------------------------------------------------- */

typedef struct {
    cmod_t cmod;
    const cmod_env_t *env;
    int comp_id;
    int exit_fd;
    pthread_t thread;
    xhc_t xhc;
    const int *stepsize_sequence;
    int stepsize_idx;
    int stepsize_last_idx;
    bool wait_for_pendant;
    char *button_cfg_file;
} xhc_hb04_inst_t;

/* --------------------------------------------------------------------------
 * INI file reading (uses libiniparse)
 * -------------------------------------------------------------------------- */

static int read_button_cfg(xhc_hb04_inst_t *inst) {
    if (!inst->button_cfg_file) return 0;

    FILE *fd = fopen(inst->button_cfg_file, "r");
    if (!fd) {
        gomc_log_errorf(inst->env->log, "xhc-hb04",
            "cannot open button config: %s\n", inst->button_cfg_file);
        return -1;
    }

    int nb_buttons = 0;
    const char *bt;
    while (nb_buttons < NB_MAX_BUTTONS &&
           (bt = ini_find(fd, "BUTTON", "XHC-HB04", nb_buttons + 1)) != NULL) {
        if (sscanf(bt, "%x:%255s", &inst->xhc.buttons[nb_buttons].code,
                   inst->xhc.buttons[nb_buttons].pin_name) != 2) {
            gomc_log_errorf(inst->env->log, "xhc-hb04",
                "button config syntax error: %s\n", bt);
            fclose(fd);
            return -1;
        }
        nb_buttons++;
    }

    fclose(fd);
    return 0;
}

/* --------------------------------------------------------------------------
 * Display encoding
 * -------------------------------------------------------------------------- */

static int xhc_encode_float(float v, unsigned char *buf) {
    unsigned int int_v = (unsigned int)roundf(fabsf(v) * 10000.0f);
    unsigned short int_part = int_v / 10000;
    unsigned short fract_part = int_v % 10000;
    if (v < 0) fract_part = fract_part | 0x8000;
    *(unsigned short *)buf = int_part;
    *((unsigned short *)buf + 1) = fract_part;
    return 4;
}

static int xhc_encode_s16(int v, unsigned char *buf) {
    *(short *)buf = (short)v;
    return 2;
}

static void xhc_display_encode(xhc_hb04_inst_t *inst, unsigned char *data, int len) {
    xhc_t *xhc = &inst->xhc;
    unsigned char buf[6*7];
    unsigned char *p = buf;

    (void)len;
    memset(buf, 0, sizeof(buf));

    *p++ = 0xFE;
    *p++ = 0xFD;
    *p++ = 0x0C;

    if (xhc->axis == axis_a)
        p += xhc_encode_float(roundf(1000.0f * *(xhc->hal->a_wc)) / 1000.0f, p);
    else
        p += xhc_encode_float(roundf(1000.0f * *(xhc->hal->x_wc)) / 1000.0f, p);
    p += xhc_encode_float(roundf(1000.0f * *(xhc->hal->y_wc)) / 1000.0f, p);
    p += xhc_encode_float(roundf(1000.0f * *(xhc->hal->z_wc)) / 1000.0f, p);
    if (xhc->axis == axis_a)
        p += xhc_encode_float(roundf(1000.0f * *(xhc->hal->a_mc)) / 1000.0f, p);
    else
        p += xhc_encode_float(roundf(1000.0f * *(xhc->hal->x_mc)) / 1000.0f, p);
    p += xhc_encode_float(roundf(1000.0f * *(xhc->hal->y_mc)) / 1000.0f, p);
    p += xhc_encode_float(roundf(1000.0f * *(xhc->hal->z_mc)) / 1000.0f, p);
    p += xhc_encode_s16((int)roundf(100.0f * *(xhc->hal->feedrate_override)), p);
    p += xhc_encode_s16((int)roundf(100.0f * *(xhc->hal->spindle_override)), p);
    p += xhc_encode_s16((int)roundf(60.0f * *(xhc->hal->feedrate)), p);
    p += xhc_encode_s16((int)roundf(60.0f * *(xhc->hal->spindle_rps)), p);

    switch (*(xhc->hal->stepsize)) {
    case    0: buf[STEPSIZE_BYTE] = STEPSIZE_DISPLAY_0; break;
    case    1: buf[STEPSIZE_BYTE] = STEPSIZE_DISPLAY_1; break;
    case    5: buf[STEPSIZE_BYTE] = STEPSIZE_DISPLAY_5; break;
    case   10: buf[STEPSIZE_BYTE] = STEPSIZE_DISPLAY_10; break;
    case   20: buf[STEPSIZE_BYTE] = STEPSIZE_DISPLAY_20; break;
    case   30: buf[STEPSIZE_BYTE] = STEPSIZE_DISPLAY_30; break;
    case   40: buf[STEPSIZE_BYTE] = STEPSIZE_DISPLAY_40; break;
    case   50: buf[STEPSIZE_BYTE] = STEPSIZE_DISPLAY_50; break;
    case  100: buf[STEPSIZE_BYTE] = STEPSIZE_DISPLAY_100; break;
    case  500: buf[STEPSIZE_BYTE] = STEPSIZE_DISPLAY_500; break;
    case 1000: buf[STEPSIZE_BYTE] = STEPSIZE_DISPLAY_1000; break;
    default:   buf[STEPSIZE_BYTE] = STEPSIZE_DISPLAY_0; break;
    }

    buf[FLAGS_BYTE] = 0;
    if (*(xhc->hal->inch_icon))
        buf[FLAGS_BYTE] |= 0x80;

    /* Multiplex to 6 USB transactions */
    p = buf;
    for (int packet = 0; packet < 6; packet++) {
        for (int i = 0; i < 8; i++) {
            if (i == 0) data[i + 8*packet] = 6;
            else data[i + 8*packet] = *p++;
        }
    }
}

static void xhc_set_display(libusb_device_handle *dev_handle, xhc_hb04_inst_t *inst) {
    unsigned char data[6*8];
    xhc_display_encode(inst, data, sizeof(data));

    for (int packet = 0; packet < 6; packet++) {
        libusb_control_transfer(dev_handle,
            LIBUSB_DT_HID,
            LIBUSB_REQUEST_SET_CONFIGURATION,
            0x0306, 0x00,
            data + 8*packet, 8, 0);
    }
}

/* --------------------------------------------------------------------------
 * Velocity computation
 * -------------------------------------------------------------------------- */

static void compute_velocity(xhc_hb04_inst_t *inst) {
    xhc_t *xhc = &inst->xhc;
    struct timeval now, delta_tv;
    gettimeofday(&now, NULL);

    if (xhc->last_tv.tv_sec == 0) xhc->last_tv = now;
    timersub(&now, &xhc->last_tv, &delta_tv);
    float elapsed = delta_tv.tv_sec + 1e-6f * delta_tv.tv_usec;
    if (elapsed <= 0) return;

    float delta_pos = (*(xhc->hal->jog_counts) - xhc->last_jog_counts) * *(xhc->hal->jog_scale);
    float velocity = *(xhc->hal->jog_max_velocity) * 60.0f * *(xhc->hal->jog_scale);
    float k = 0.05f;

    if (delta_pos != 0.0f) {
        *(xhc->hal->jog_velocity) = (1 - k) * *(xhc->hal->jog_velocity) + k * velocity;
        *(xhc->hal->jog_increment) = fabsf(delta_pos);
        *(xhc->hal->jog_plus_x)  = (delta_pos > 0) && *(xhc->hal->jog_enable_x);
        *(xhc->hal->jog_minus_x) = (delta_pos < 0) && *(xhc->hal->jog_enable_x);
        *(xhc->hal->jog_plus_y)  = (delta_pos > 0) && *(xhc->hal->jog_enable_y);
        *(xhc->hal->jog_minus_y) = (delta_pos < 0) && *(xhc->hal->jog_enable_y);
        *(xhc->hal->jog_plus_z)  = (delta_pos > 0) && *(xhc->hal->jog_enable_z);
        *(xhc->hal->jog_minus_z) = (delta_pos < 0) && *(xhc->hal->jog_enable_z);
        *(xhc->hal->jog_plus_a)  = (delta_pos > 0) && *(xhc->hal->jog_enable_a);
        *(xhc->hal->jog_minus_a) = (delta_pos < 0) && *(xhc->hal->jog_enable_a);
        xhc->last_jog_counts = *(xhc->hal->jog_counts);
        xhc->last_tv = now;
    } else {
        *(xhc->hal->jog_velocity) = (1 - k) * *(xhc->hal->jog_velocity);
        if (elapsed > 0.25f) {
            *(xhc->hal->jog_velocity) = 0;
            *(xhc->hal->jog_plus_x) = 0;
            *(xhc->hal->jog_minus_x) = 0;
            *(xhc->hal->jog_plus_y) = 0;
            *(xhc->hal->jog_minus_y) = 0;
            *(xhc->hal->jog_plus_z) = 0;
            *(xhc->hal->jog_minus_z) = 0;
            *(xhc->hal->jog_plus_a) = 0;
            *(xhc->hal->jog_minus_a) = 0;
        }
    }
}

/* --------------------------------------------------------------------------
 * Step handling
 * -------------------------------------------------------------------------- */

static void handle_step(xhc_hb04_inst_t *inst) {
    xhc_t *xhc = &inst->xhc;
    int _inc_step_status = STEP_NONE;
    int _stepsize = *(xhc->hal->stepsize);

    if (*(xhc->hal->stepsize_up)) {
        _inc_step_status = STEP_UP;
        if (*(xhc->hal->stepsize_down))
            _inc_step_status = STEP_NONE;
    } else if (*(xhc->hal->stepsize_down)) {
        _inc_step_status = STEP_DOWN;
    }

    if (_inc_step_status != xhc->old_inc_step_status) {
        if (_inc_step_status == STEP_UP) {
            inst->stepsize_idx++;
            if (inst->stepsize_sequence[inst->stepsize_idx] == 0)
                inst->stepsize_idx = 0;
        }
        if (_inc_step_status == STEP_DOWN) {
            inst->stepsize_idx--;
            if (inst->stepsize_idx < 0)
                inst->stepsize_idx = inst->stepsize_last_idx;
        }
        _stepsize = inst->stepsize_sequence[inst->stepsize_idx];
    }

    xhc->old_inc_step_status = _inc_step_status;
    *(xhc->hal->stepsize) = _stepsize;
    *(xhc->hal->jog_scale) = *(xhc->hal->stepsize) * 0.001f;
}

/* --------------------------------------------------------------------------
 * USB callback (libusb async transfer)
 * -------------------------------------------------------------------------- */

typedef struct {
    xhc_hb04_inst_t *inst;
    int do_reconnect;
} xhc_usb_ctx_t;

static unsigned char usb_in_buf[32];

static void cb_response_in(struct libusb_transfer *transfer) {
    xhc_usb_ctx_t *uctx = (xhc_usb_ctx_t *)transfer->user_data;
    xhc_hb04_inst_t *inst = uctx->inst;
    xhc_t *xhc = &inst->xhc;

    if (!*(xhc->hal->connected)) return;

    if (transfer->actual_length > 0) {
        xhc->button_code = usb_in_buf[1];
        xhc->axis = (xhc_axis_t)usb_in_buf[3];

        *(xhc->hal->jog_counts) += ((signed char)usb_in_buf[4]);
        *(xhc->hal->jog_counts_neg) = -*(xhc->hal->jog_counts);
        *(xhc->hal->jog_enable_off)      = (xhc->axis == axis_off);
        *(xhc->hal->jog_enable_x)        = (xhc->axis == axis_x);
        *(xhc->hal->jog_enable_y)        = (xhc->axis == axis_y);
        *(xhc->hal->jog_enable_z)        = (xhc->axis == axis_z);
        *(xhc->hal->jog_enable_a)        = (xhc->axis == axis_a);
        *(xhc->hal->jog_enable_feedrate) = (xhc->axis == axis_feed);
        *(xhc->hal->jog_enable_spindle)  = (xhc->axis == axis_spindle);

        for (int i = 0; i < NB_MAX_BUTTONS; i++) {
            if (!xhc->hal->button_pin[i]) continue;
            *(xhc->hal->button_pin[i]) = (xhc->button_code == xhc->buttons[i].code);
            if (strcmp("button-zero", xhc->buttons[i].pin_name) == 0) {
                *(xhc->hal->zero_x) = (xhc->button_code == xhc->buttons[i].code) && (xhc->axis == axis_x);
                *(xhc->hal->zero_y) = (xhc->button_code == xhc->buttons[i].code) && (xhc->axis == axis_y);
                *(xhc->hal->zero_z) = (xhc->button_code == xhc->buttons[i].code) && (xhc->axis == axis_z);
                *(xhc->hal->zero_a) = (xhc->button_code == xhc->buttons[i].code) && (xhc->axis == axis_a);
            }
            if (strcmp("button-goto-zero", xhc->buttons[i].pin_name) == 0) {
                *(xhc->hal->gotozero_x) = (xhc->button_code == xhc->buttons[i].code) && (xhc->axis == axis_x);
                *(xhc->hal->gotozero_y) = (xhc->button_code == xhc->buttons[i].code) && (xhc->axis == axis_y);
                *(xhc->hal->gotozero_z) = (xhc->button_code == xhc->buttons[i].code) && (xhc->axis == axis_z);
                *(xhc->hal->gotozero_a) = (xhc->button_code == xhc->buttons[i].code) && (xhc->axis == axis_a);
            }
            if (strcmp("button-half", xhc->buttons[i].pin_name) == 0) {
                *(xhc->hal->half_x) = (xhc->button_code == xhc->buttons[i].code) && (xhc->axis == axis_x);
                *(xhc->hal->half_y) = (xhc->button_code == xhc->buttons[i].code) && (xhc->axis == axis_y);
                *(xhc->hal->half_z) = (xhc->button_code == xhc->buttons[i].code) && (xhc->axis == axis_z);
                *(xhc->hal->half_a) = (xhc->button_code == xhc->buttons[i].code) && (xhc->axis == axis_a);
            }
            if (strcmp("button-step", xhc->buttons[i].pin_name) == 0)
                xhc->button_step = xhc->buttons[i].code;
        }

        /* Detect pendant going to sleep */
        if (usb_in_buf[0] == 0x04 &&
            usb_in_buf[1] == 0 && usb_in_buf[2] == 0 &&
            usb_in_buf[3] == 0 && usb_in_buf[4] == 0 &&
            usb_in_buf[5] == 0) {
            *(xhc->hal->sleeping) = 1;
        } else {
            gettimeofday(&xhc->last_wakeup, NULL);
            *(xhc->hal->sleeping) = 0;
        }
    }

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED ||
        transfer->status == LIBUSB_TRANSFER_TIMED_OUT) {
        libusb_submit_transfer(transfer);
    } else {
        uctx->do_reconnect = 1;
    }
}

/* --------------------------------------------------------------------------
 * Main loop
 * -------------------------------------------------------------------------- */

static void *xhc_hb04_loop(void *arg) {
    xhc_hb04_inst_t *inst = (xhc_hb04_inst_t *)arg;
    xhc_t *xhc = &inst->xhc;
    libusb_context *ctx = NULL;
    libusb_device_handle *dev_handle = NULL;
    struct libusb_transfer *transfer_in = NULL;
    xhc_usb_ctx_t uctx = { .inst = inst, .do_reconnect = 0 };

    *(xhc->hal->stepsize) = inst->stepsize_sequence[0];
    *(xhc->hal->require_pendant) = inst->wait_for_pendant ? 1 : 0;

    while (!gomc_should_exit(inst->exit_fd)) {
        /* Wait after reconnect */
        if (uctx.do_reconnect) {
            struct pollfd pfd = { .fd = inst->exit_fd, .events = POLLIN };
            poll(&pfd, 1, 5000);
            if (gomc_should_exit(inst->exit_fd)) break;
            uctx.do_reconnect = 0;
        }

        int r = libusb_init(&ctx);
        if (r < 0) {
            gomc_log_errorf(inst->env->log, "xhc-hb04", "libusb_init failed\n");
            break;
        }

        gomc_log_infof(inst->env->log, "xhc-hb04", "waiting for XHC-HB04 device\n");
        *(xhc->hal->connected) = 0;

        /* Poll for device */
        int wait_count = 0;
        do {
            libusb_device **devs;
            libusb_get_device_list(ctx, &devs);
            dev_handle = libusb_open_device_with_vid_pid(ctx, 0x10CE, 0xEB70);
            libusb_free_device_list(devs, 1);
            if (!dev_handle) {
                struct pollfd pfd = { .fd = inst->exit_fd, .events = POLLIN };
                poll(&pfd, 1, 1000);
                wait_count++;
                if (inst->wait_for_pendant && wait_count > 10) {
                    gomc_log_errorf(inst->env->log, "xhc-hb04",
                        "pendant not found, timeout\n");
                    libusb_exit(ctx);
                    return NULL;
                }
            }
        } while (!dev_handle && !gomc_should_exit(inst->exit_fd));

        if (gomc_should_exit(inst->exit_fd)) {
            libusb_exit(ctx);
            break;
        }

        if (dev_handle) {
            gomc_log_infof(inst->env->log, "xhc-hb04", "found XHC-HB04 device\n");

            if (libusb_kernel_driver_active(dev_handle, 0) == 1)
                libusb_detach_kernel_driver(dev_handle, 0);

            r = libusb_claim_interface(dev_handle, 0);
            if (r < 0) {
                gomc_log_errorf(inst->env->log, "xhc-hb04",
                    "libusb_claim_interface failed: %s\n", libusb_strerror(r));
                libusb_close(dev_handle);
                libusb_exit(ctx);
                break;
            }

            transfer_in = libusb_alloc_transfer(0);
            libusb_fill_bulk_transfer(transfer_in, dev_handle,
                (0x1 | LIBUSB_ENDPOINT_IN),
                usb_in_buf, sizeof(usb_in_buf),
                cb_response_in, &uctx, 0);
            libusb_submit_transfer(transfer_in);

            *(xhc->hal->connected) = 1;
            xhc_set_display(dev_handle, inst);

            while (!gomc_should_exit(inst->exit_fd) && !uctx.do_reconnect) {
                struct timeval tv = { .tv_sec = 0, .tv_usec = 30000 };
                libusb_handle_events_timeout(ctx, &tv);
                compute_velocity(inst);
                handle_step(inst);
                xhc_set_display(dev_handle, inst);
            }

            *(xhc->hal->connected) = 0;
            gomc_log_infof(inst->env->log, "xhc-hb04", "connection lost\n");

            if (*(xhc->hal->require_pendant) && !gomc_should_exit(inst->exit_fd)) {
                /* Exit if pendant required but lost */
                libusb_cancel_transfer(transfer_in);
                struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
                libusb_handle_events_timeout(ctx, &tv);
                libusb_free_transfer(transfer_in);
                libusb_release_interface(dev_handle, 0);
                libusb_close(dev_handle);
                libusb_exit(ctx);
                return NULL;
            }

            libusb_cancel_transfer(transfer_in);
            struct timeval tv2 = { .tv_sec = 1, .tv_usec = 0 };
            libusb_handle_events_timeout(ctx, &tv2);
            libusb_free_transfer(transfer_in);
            libusb_release_interface(dev_handle, 0);
            libusb_close(dev_handle);
        }

        libusb_exit(ctx);
        ctx = NULL;
        dev_handle = NULL;
    }

    return NULL;
}

/* --------------------------------------------------------------------------
 * HAL pin creation
 * -------------------------------------------------------------------------- */

static int create_hal_pins(xhc_hb04_inst_t *inst) {
    const gomc_hal_t *hal = inst->env->hal;
    xhc_t *xhc = &inst->xhc;
    int r = 0;
    const char *modname = "xhc-hb04";

    xhc->hal = (xhc_hal_t *)hal->malloc(hal->ctx, sizeof(xhc_hal_t));
    if (!xhc->hal) return -1;
    memset(xhc->hal, 0, sizeof(xhc_hal_t));

    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &xhc->hal->x_mc, inst->comp_id, "%s.x.pos-absolute", modname);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &xhc->hal->y_mc, inst->comp_id, "%s.y.pos-absolute", modname);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &xhc->hal->z_mc, inst->comp_id, "%s.z.pos-absolute", modname);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &xhc->hal->a_mc, inst->comp_id, "%s.a.pos-absolute", modname);

    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &xhc->hal->x_wc, inst->comp_id, "%s.x.pos-relative", modname);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &xhc->hal->y_wc, inst->comp_id, "%s.y.pos-relative", modname);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &xhc->hal->z_wc, inst->comp_id, "%s.z.pos-relative", modname);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &xhc->hal->a_wc, inst->comp_id, "%s.a.pos-relative", modname);

    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &xhc->hal->feedrate, inst->comp_id, "%s.feed-value", modname);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &xhc->hal->feedrate_override, inst->comp_id, "%s.feed-override", modname);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &xhc->hal->spindle_rps, inst->comp_id, "%s.spindle-rps", modname);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &xhc->hal->spindle_override, inst->comp_id, "%s.spindle-override", modname);

    for (int i = 0; i < NB_MAX_BUTTONS; i++) {
        if (!xhc->buttons[i].pin_name[0]) continue;
        r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->button_pin[i], inst->comp_id,
            "%s.%s", modname, xhc->buttons[i].pin_name);
        if (strcmp("button-zero", xhc->buttons[i].pin_name) == 0) {
            r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->zero_x, inst->comp_id, "%s.%s-x", modname, xhc->buttons[i].pin_name);
            r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->zero_y, inst->comp_id, "%s.%s-y", modname, xhc->buttons[i].pin_name);
            r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->zero_z, inst->comp_id, "%s.%s-z", modname, xhc->buttons[i].pin_name);
            r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->zero_a, inst->comp_id, "%s.%s-a", modname, xhc->buttons[i].pin_name);
        }
        if (strcmp("button-goto-zero", xhc->buttons[i].pin_name) == 0) {
            r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->gotozero_x, inst->comp_id, "%s.%s-x", modname, xhc->buttons[i].pin_name);
            r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->gotozero_y, inst->comp_id, "%s.%s-y", modname, xhc->buttons[i].pin_name);
            r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->gotozero_z, inst->comp_id, "%s.%s-z", modname, xhc->buttons[i].pin_name);
            r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->gotozero_a, inst->comp_id, "%s.%s-a", modname, xhc->buttons[i].pin_name);
        }
        if (strcmp("button-half", xhc->buttons[i].pin_name) == 0) {
            r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->half_x, inst->comp_id, "%s.%s-x", modname, xhc->buttons[i].pin_name);
            r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->half_y, inst->comp_id, "%s.%s-y", modname, xhc->buttons[i].pin_name);
            r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->half_z, inst->comp_id, "%s.%s-z", modname, xhc->buttons[i].pin_name);
            r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->half_a, inst->comp_id, "%s.%s-a", modname, xhc->buttons[i].pin_name);
        }
        if (strcmp("button-step", xhc->buttons[i].pin_name) == 0)
            xhc->button_step = xhc->buttons[i].code;
    }

    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->sleeping, inst->comp_id, "%s.sleeping", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->connected, inst->comp_id, "%s.connected", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN,  &xhc->hal->stepsize_up, inst->comp_id, "%s.stepsize-up", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN,  &xhc->hal->stepsize_down, inst->comp_id, "%s.stepsize-down", modname);
    r |= gomc_hal_pin_s32_newf(hal, GOMC_HAL_OUT, &xhc->hal->stepsize, inst->comp_id, "%s.stepsize", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->require_pendant, inst->comp_id, "%s.require_pendant", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN,  &xhc->hal->inch_icon, inst->comp_id, "%s.inch-icon", modname);

    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_enable_off, inst->comp_id, "%s.jog.enable-off", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_enable_x, inst->comp_id, "%s.jog.enable-x", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_enable_y, inst->comp_id, "%s.jog.enable-y", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_enable_z, inst->comp_id, "%s.jog.enable-z", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_enable_a, inst->comp_id, "%s.jog.enable-a", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_enable_feedrate, inst->comp_id, "%s.jog.enable-feed-override", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_enable_spindle, inst->comp_id, "%s.jog.enable-spindle-override", modname);

    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_scale, inst->comp_id, "%s.jog.scale", modname);
    r |= gomc_hal_pin_s32_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_counts, inst->comp_id, "%s.jog.counts", modname);
    r |= gomc_hal_pin_s32_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_counts_neg, inst->comp_id, "%s.jog.counts-neg", modname);

    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_velocity, inst->comp_id, "%s.jog.velocity", modname);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_IN,  &xhc->hal->jog_max_velocity, inst->comp_id, "%s.jog.max-velocity", modname);
    r |= gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_increment, inst->comp_id, "%s.jog.increment", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_plus_x, inst->comp_id, "%s.jog.plus-x", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_minus_x, inst->comp_id, "%s.jog.minus-x", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_plus_y, inst->comp_id, "%s.jog.plus-y", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_minus_y, inst->comp_id, "%s.jog.minus-y", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_plus_z, inst->comp_id, "%s.jog.plus-z", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_minus_z, inst->comp_id, "%s.jog.minus-z", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_plus_a, inst->comp_id, "%s.jog.plus-a", modname);
    r |= gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &xhc->hal->jog_minus_a, inst->comp_id, "%s.jog.minus-a", modname);

    return r;
}

/* --------------------------------------------------------------------------
 * Parse arguments: -I <file> -s <n> -x
 * In cmod context these come as argv: "I=file", "s=1", "x" etc.
 * -------------------------------------------------------------------------- */

static void parse_args(xhc_hb04_inst_t *inst, int argc, const char **argv) {
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "I=", 2) == 0) {
            inst->button_cfg_file = strdup(argv[i] + 2);
        } else if (strncmp(argv[i], "s=", 2) == 0) {
            switch (argv[i][2]) {
            case '2': inst->stepsize_sequence = stepsize_sequence_2; break;
            case '3': inst->stepsize_sequence = stepsize_sequence_3; break;
            case '4': inst->stepsize_sequence = stepsize_sequence_4; break;
            case '5': inst->stepsize_sequence = stepsize_sequence_5; break;
            default:  inst->stepsize_sequence = stepsize_sequence_1; break;
            }
        } else if (strcmp(argv[i], "x") == 0) {
            inst->wait_for_pendant = true;
        }
    }

    /* Compute last valid stepsize index */
    for (int idx = 0; idx < MAX_STEPSIZE_SEQUENCE; idx++) {
        if (inst->stepsize_sequence[idx] == 0) {
            inst->stepsize_last_idx = idx - 1;
            break;
        }
    }
}

/* --------------------------------------------------------------------------
 * cmod lifecycle
 * -------------------------------------------------------------------------- */

static void xhc_hb04_destroy(cmod_t *self) {
    xhc_hb04_inst_t *inst = (xhc_hb04_inst_t *)self;

    if (inst->exit_fd >= 0) {
        uint64_t val = 1;
        if (write(inst->exit_fd, &val, sizeof(val)) != sizeof(val)) { /* ignore */ }
        pthread_join(inst->thread, NULL);
        close(inst->exit_fd);
    }

    free(inst->button_cfg_file);

    if (inst->comp_id > 0)
        inst->env->hal->exit(inst->env->hal->ctx, inst->comp_id);
    inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
}

static int xhc_hb04_start(cmod_t *self) {
    xhc_hb04_inst_t *inst = (xhc_hb04_inst_t *)self;
    if (pthread_create(&inst->thread, NULL, xhc_hb04_loop, inst) != 0)
        return -1;
    return 0;
}

static void xhc_hb04_stop(cmod_t *self) {
    xhc_hb04_inst_t *inst = (xhc_hb04_inst_t *)self;
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

    xhc_hb04_inst_t *inst = (xhc_hb04_inst_t *)env->rtapi->calloc(
        env->rtapi->ctx, sizeof(xhc_hb04_inst_t));
    if (!inst) return -1;

    inst->cmod.Start = xhc_hb04_start;
    inst->cmod.Stop = xhc_hb04_stop;
    inst->cmod.Destroy = xhc_hb04_destroy;
    inst->env = env;
    inst->exit_fd = -1;
    inst->stepsize_sequence = stepsize_sequence_1;
    inst->xhc.old_inc_step_status = STEP_UNDEFINED;

    parse_args(inst, argc, argv);

    inst->comp_id = env->hal->init(env->hal->ctx, "xhc-hb04",
                                   env->dl_handle, GOMC_HAL_COMP_USER);
    if (inst->comp_id < 0) {
        free(inst->button_cfg_file);
        env->rtapi->free(env->rtapi->ctx, inst);
        return -1;
    }

    if (read_button_cfg(inst) != 0)
        goto fail;

    if (create_hal_pins(inst) != 0)
        goto fail;

    inst->exit_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (inst->exit_fd < 0) goto fail;

    env->hal->ready(env->hal->ctx, inst->comp_id);
    *out = &inst->cmod;
    return 0;

fail:
    xhc_hb04_destroy(&inst->cmod);
    return -1;
}
