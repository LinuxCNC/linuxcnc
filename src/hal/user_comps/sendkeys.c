//
//   Copyright (C) 2021 Andy Pugh
//   Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//
//   Cmod port: userspace HAL component that creates a virtual keyboard via
//   Linux uinput.  HAL pins drive key events into the kernel input subsystem.
//

#include "gomc_env.h"
#include "gomc_user.h"

#include <linux/uinput.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <poll.h>
#include <pthread.h>
#include <sys/eventfd.h>

/* --------------------------------------------------------------------------
 * Per-instance data (one per "config=..." stanza)
 * -------------------------------------------------------------------------- */

typedef struct {
    gomc_hal_u32_t *keycode;
    gomc_hal_s32_t *current_event;
    gomc_hal_bit_t *init;
    gomc_hal_bit_t **trigger;
    gomc_hal_u32_t *event;      /* array of event params */
} sendkeys_hal_t;

typedef struct {
    int num_codes;
    int num_events;
    int num_triggers;
    bool inited;
    int fd;
    bool *prev;
    gomc_hal_u32_t oldcode;
} sendkeys_param_t;

typedef struct {
    cmod_t cmod;
    const cmod_env_t *env;
    int comp_id;
    int exit_fd;
    pthread_t thread;
    sendkeys_hal_t *hal;
    sendkeys_param_t *param;
    int num_insts;
} sendkeys_inst_t;

/* -------------------------------------------------------------------------- */

static void emit(int fd, int type, int code, int val) {
    struct input_event ie;
    ie.type = type;
    ie.code = code;
    ie.value = val;
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;
    if (write(fd, &ie, sizeof(ie)) != (ssize_t)sizeof(ie)) {
        /* best-effort */
    }
}

/* --------------------------------------------------------------------------
 * Main loop — runs in its own thread
 * -------------------------------------------------------------------------- */

static void *sendkeys_loop(void *arg) {
    sendkeys_inst_t *inst = (sendkeys_inst_t *)arg;
    struct pollfd pfd = { .fd = inst->exit_fd, .events = POLLIN };

    while (!gomc_should_exit(inst->exit_fd)) {
        for (int i = 0; i < inst->num_insts; i++) {
            sendkeys_hal_t *hal = &inst->hal[i];
            sendkeys_param_t *param = &inst->param[i];

            if ((*hal->keycode & 0xC0) == 0x80)
                *hal->current_event = (*hal->keycode & 0x3f);
            else
                *hal->current_event = -1;

            if (!*hal->init) continue;

            /* Lazy init: set up uinput device once "init" pin goes true */
            if (*hal->init && !param->inited) {
                param->fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
                if (param->fd < 0) {
                    gomc_log_errorf(inst->env->log, "sendkeys",
                        "Cannot open /dev/uinput. Suggest chmod 666 /dev/uinput\n");
                    continue;
                }
                ioctl(param->fd, UI_SET_EVBIT, EV_KEY);
                for (int j = 0; j < param->num_events; j++) {
                    if (hal->event[j] > 0 && hal->event[j] < KEY_MAX)
                        ioctl(param->fd, UI_SET_KEYBIT, hal->event[j]);
                }
                struct uinput_user_dev uidev;
                memset(&uidev, 0, sizeof(uidev));
                snprintf(uidev.name, sizeof(uidev.name), "linuxcnc-hal");
                uidev.id.bustype = BUS_USB;
                uidev.id.vendor  = 0x1;
                uidev.id.product = 0x1;
                uidev.id.version = 1;
                if (write(param->fd, &uidev, sizeof(uidev)) != (ssize_t)sizeof(uidev)) {
                    close(param->fd);
                    param->fd = -1;
                    continue;
                }
                if (ioctl(param->fd, UI_DEV_CREATE) < 0) {
                    close(param->fd);
                    param->fd = -1;
                    continue;
                }
                param->inited = 1;
            }

            /* Process keycode changes */
            if (*hal->keycode != param->oldcode) {
                if ((*hal->keycode & 0x3F) < (unsigned)param->num_events &&
                    hal->event[*hal->keycode & 0x3F] != 0) {
                    if ((*hal->keycode & 0xC0) == 0xC0) { /* keydown */
                        emit(param->fd, EV_KEY, hal->event[*hal->keycode & 0x3F], 1);
                        emit(param->fd, EV_SYN, SYN_REPORT, 0);
                    } else if ((*hal->keycode & 0xC0) == 0x80) { /* keyup */
                        emit(param->fd, EV_KEY, hal->event[*hal->keycode & 0x3F], 0);
                        emit(param->fd, EV_SYN, SYN_REPORT, 0);
                    }
                }
                param->oldcode = *hal->keycode;
            }

            /* Process trigger pins */
            for (int j = 0; j < param->num_triggers; j++) {
                if (param->prev[j] != (bool)*hal->trigger[j]) {
                    if (*hal->trigger[j]) {
                        emit(param->fd, EV_KEY, hal->event[param->num_codes + j], 1);
                        emit(param->fd, EV_SYN, SYN_REPORT, 0);
                    } else {
                        emit(param->fd, EV_KEY, hal->event[param->num_codes + j], 0);
                        emit(param->fd, EV_SYN, SYN_REPORT, 0);
                    }
                    param->prev[j] = *hal->trigger[j];
                }
            }
        }
        poll(&pfd, 1, 10); /* ~10ms cycle */
    }
    return NULL;
}

/* --------------------------------------------------------------------------
 * Parse config string: "config=S<codes>T<triggers>,S<codes>T<triggers>,..."
 * -------------------------------------------------------------------------- */

static int parse_config(sendkeys_inst_t *inst, int argc, const char **argv) {
    int *codes = NULL;
    int *pins = NULL;
    int index = -1;

    for (int i = 0; i < argc; i++) {
        int ptr = 0;
        bool shift = false;

        if (strncmp(argv[i], "config=", 7) == 0)
            ptr = 6; /* point at '=' so it triggers first index++ */
        else
            continue;

        while (argv[i][ptr]) {
            switch (argv[i][ptr]) {
            case '=':
            case ' ':
            case ',':
                index++;
                codes = realloc(codes, (index + 1) * sizeof(int));
                pins = realloc(pins, (index + 1) * sizeof(int));
                if (!codes || !pins) { free(codes); free(pins); return -1; }
                codes[index] = 0;
                pins[index] = 0;
                shift = false;
                break;
            case 's': case 'S':
                shift = false;
                break;
            case 't': case 'T':
                shift = true;
                break;
            default:
                if (argv[i][ptr] >= '0' && argv[i][ptr] <= '9') {
                    if (shift)
                        pins[index] = pins[index] * 10 + argv[i][ptr] - '0';
                    else
                        codes[index] = codes[index] * 10 + argv[i][ptr] - '0';
                }
                break;
            }
            ptr++;
        }
    }

    inst->num_insts = index + 1;
    if (inst->num_insts <= 0) {
        gomc_log_errorf(inst->env->log, "sendkeys",
            "no config= argument provided (e.g. config=S5T2)\n");
        free(codes); free(pins); return -1;
    }

    /* Allocate HAL and param arrays */
    inst->hal = (sendkeys_hal_t *)inst->env->hal->malloc(
        inst->env->hal->ctx, inst->num_insts * sizeof(sendkeys_hal_t));
    inst->param = (sendkeys_param_t *)calloc(inst->num_insts, sizeof(sendkeys_param_t));
    if (!inst->hal || !inst->param) { free(codes); free(pins); return -1; }

    for (int i = 0; i < inst->num_insts; i++) {
        sendkeys_hal_t *hal = &inst->hal[i];
        sendkeys_param_t *param = &inst->param[i];
        int r;

        r = gomc_hal_pin_u32_newf(inst->env->hal, GOMC_HAL_IN,
            &hal->keycode, inst->comp_id, "sendkeys.%d.keycode", i);
        if (r != 0) goto fail;

        r = gomc_hal_pin_s32_newf(inst->env->hal, GOMC_HAL_OUT,
            &hal->current_event, inst->comp_id, "sendkeys.%d.current-event", i);
        if (r != 0) goto fail;

        r = gomc_hal_pin_bit_newf(inst->env->hal, GOMC_HAL_IN,
            &hal->init, inst->comp_id, "sendkeys.%d.init", i);
        if (r != 0) goto fail;

        param->num_codes = codes[i];
        param->num_triggers = pins[i];
        param->num_events = codes[i] + pins[i];
        param->fd = -1;

        /* Event params (scan-event-NN + pin-event-NN) */
        hal->event = (gomc_hal_u32_t *)inst->env->hal->malloc(
            inst->env->hal->ctx, param->num_events * sizeof(gomc_hal_u32_t));
        if (!hal->event) goto fail;
        memset((void *)hal->event, 0, param->num_events * sizeof(gomc_hal_u32_t));

        for (int j = 0; j < param->num_codes; j++) {
            r = gomc_hal_param_u32_newf(inst->env->hal, GOMC_HAL_RW,
                &hal->event[j], inst->comp_id, "sendkeys.%d.scan-event-%02d", i, j);
            if (r != 0) goto fail;
        }
        for (int j = 0; j < param->num_triggers; j++) {
            r = gomc_hal_param_u32_newf(inst->env->hal, GOMC_HAL_RW,
                &hal->event[j + param->num_codes], inst->comp_id,
                "sendkeys.%d.pin-event-%02d", i, j);
            if (r != 0) goto fail;
        }

        /* Trigger pins */
        hal->trigger = (gomc_hal_bit_t **)inst->env->hal->malloc(
            inst->env->hal->ctx, param->num_triggers * sizeof(gomc_hal_bit_t *));
        if (!hal->trigger && param->num_triggers > 0) goto fail;
        param->prev = calloc(param->num_triggers, sizeof(bool));
        if (!param->prev && param->num_triggers > 0) goto fail;

        for (int j = 0; j < param->num_triggers; j++) {
            r = gomc_hal_pin_bit_newf(inst->env->hal, GOMC_HAL_IN,
                &hal->trigger[j], inst->comp_id, "sendkeys.%d.trigger-%02d", i, j);
            if (r != 0) goto fail;
        }
    }

    free(codes);
    free(pins);
    return 0;

fail:
    free(codes);
    free(pins);
    return -1;
}

/* --------------------------------------------------------------------------
 * cmod lifecycle
 * -------------------------------------------------------------------------- */

static void sendkeys_destroy(cmod_t *self) {
    sendkeys_inst_t *inst = (sendkeys_inst_t *)self;
    const gomc_hal_t *hal = inst->env->hal;

    if (inst->exit_fd >= 0) {
        uint64_t val = 1;
        if (write(inst->exit_fd, &val, sizeof(val)) != sizeof(val)) { /* ignore */ }
        pthread_join(inst->thread, NULL);
        close(inst->exit_fd);
    }

    for (int i = 0; i < inst->num_insts; i++) {
        if (inst->param[i].fd >= 0) {
            ioctl(inst->param[i].fd, UI_DEV_DESTROY);
            close(inst->param[i].fd);
        }
        free(inst->param[i].prev);
    }
    free(inst->param);

    if (inst->comp_id > 0)
        hal->exit(hal->ctx, inst->comp_id);
    inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
}

static int sendkeys_start(cmod_t *self) {
    sendkeys_inst_t *inst = (sendkeys_inst_t *)self;
    if (pthread_create(&inst->thread, NULL, sendkeys_loop, inst) != 0)
        return -1;
    return 0;
}

static void sendkeys_stop(cmod_t *self) {
    sendkeys_inst_t *inst = (sendkeys_inst_t *)self;
    if (inst->exit_fd >= 0) {
        uint64_t val = 1;
        if (write(inst->exit_fd, &val, sizeof(val)) != sizeof(val)) { /* ignore */ }
        pthread_join(inst->thread, NULL);
    }
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out) {
    (void)name;

    sendkeys_inst_t *inst = (sendkeys_inst_t *)env->rtapi->calloc(
        env->rtapi->ctx, sizeof(sendkeys_inst_t));
    if (!inst) return -1;

    inst->cmod.Start = sendkeys_start;
    inst->cmod.Stop = sendkeys_stop;
    inst->cmod.Destroy = sendkeys_destroy;
    inst->env = env;
    inst->exit_fd = -1;

    inst->comp_id = env->hal->init(env->hal->ctx, "sendkeys",
                                   env->dl_handle, GOMC_HAL_COMP_USER);
    if (inst->comp_id < 0) {
        env->rtapi->free(env->rtapi->ctx, inst);
        return -1;
    }

    if (parse_config(inst, argc, argv) != 0) {
        env->hal->exit(env->hal->ctx, inst->comp_id);
        env->rtapi->free(env->rtapi->ctx, inst);
        return -1;
    }

    inst->exit_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (inst->exit_fd < 0) {
        env->hal->exit(env->hal->ctx, inst->comp_id);
        env->rtapi->free(env->rtapi->ctx, inst);
        return -1;
    }

    env->hal->ready(env->hal->ctx, inst->comp_id);
    *out = &inst->cmod;
    return 0;
}
