/*
 * hal_input.c — cmod driver for Linux input devices (evdev).
 *
 * Interfaces keyboards, joysticks, mice, and other input devices to HAL pins.
 * Reads /dev/input/event* devices and exposes keys, relative axes, and absolute
 * axes as HAL pins.
 *
 * Originally a Python userspace component (bin/hal_input + lib/python/linux_event.py).
 *
 * Copyright (C) 2007 Jeff Epler <jepler@unpythonic.net>
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — C/cmod port
 * License: GPL v2+
 */

#include "gomc_env.h"
#include "gomc_user.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <ctype.h>
#include <linux/input.h>

#define COMP_NAME "hal_input"
#define MAX_DEVICES 8
#define MAX_KEYS    768
#define MAX_REL     16
#define MAX_ABS     64

/* ========================================================================== */
/* Evdev name tables                                                           */
/* ========================================================================== */

/* We build pin names by lowercasing the kernel event code name and replacing _ with - */
/* For keys, the kernel provides names like KEY_A, BTN_LEFT, etc. */
/* We need a lookup from code → name string for the codes the device supports. */

static const char *key_names[KEY_CNT];
static const char *rel_names[REL_CNT];
static const char *abs_names[ABS_CNT];
static bool tables_inited = false;

/* Helper to store a static name string */
static char name_buf[16384];
static int name_buf_pos = 0;

static const char *store_name(const char *prefix, int code)
{
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%s-%d", prefix, code);
    /* Convert to HAL name: lowercase, underscore→dash */
    int len = strlen(tmp);
    if (name_buf_pos + len + 1 > (int)sizeof(name_buf))
        return "unknown";
    char *p = &name_buf[name_buf_pos];
    for (int i = 0; i <= len; i++) {
        p[i] = (tmp[i] == '_') ? '-' : tolower((unsigned char)tmp[i]);
    }
    name_buf_pos += len + 1;
    return p;
}

static const char *store_str(const char *s)
{
    int len = strlen(s);
    if (name_buf_pos + len + 1 > (int)sizeof(name_buf))
        return "unknown";
    char *p = &name_buf[name_buf_pos];
    for (int i = 0; i <= len; i++)
        p[i] = (s[i] == '_') ? '-' : tolower((unsigned char)s[i]);
    name_buf_pos += len + 1;
    return p;
}

/* Initialize name tables from known defines */
static void init_name_tables(void)
{
    if (tables_inited) return;
    tables_inited = true;

    /* Fill all with generated names first */
    for (int i = 0; i < KEY_CNT; i++)
        key_names[i] = store_name("key", i);
    for (int i = 0; i < REL_CNT; i++)
        rel_names[i] = store_name("rel", i);
    for (int i = 0; i < ABS_CNT; i++)
        abs_names[i] = store_name("abs", i);

    /* Override well-known names */
#define K(code, name) if ((code) < KEY_CNT) key_names[code] = store_str(name)
#define R(code, name) if ((code) < REL_CNT) rel_names[code] = store_str(name)
#define A(code, name) if ((code) < ABS_CNT) abs_names[code] = store_str(name)

    /* Common keys */
    K(KEY_ESC, "key-esc"); K(KEY_1, "key-1"); K(KEY_2, "key-2");
    K(KEY_3, "key-3"); K(KEY_4, "key-4"); K(KEY_5, "key-5");
    K(KEY_6, "key-6"); K(KEY_7, "key-7"); K(KEY_8, "key-8");
    K(KEY_9, "key-9"); K(KEY_0, "key-0");
    K(KEY_A, "key-a"); K(KEY_B, "key-b"); K(KEY_C, "key-c");
    K(KEY_D, "key-d"); K(KEY_E, "key-e"); K(KEY_F, "key-f");
    K(KEY_G, "key-g"); K(KEY_H, "key-h"); K(KEY_I, "key-i");
    K(KEY_J, "key-j"); K(KEY_K, "key-k"); K(KEY_L, "key-l");
    K(KEY_M, "key-m"); K(KEY_N, "key-n"); K(KEY_O, "key-o");
    K(KEY_P, "key-p"); K(KEY_Q, "key-q"); K(KEY_R, "key-r");
    K(KEY_S, "key-s"); K(KEY_T, "key-t"); K(KEY_U, "key-u");
    K(KEY_V, "key-v"); K(KEY_W, "key-w"); K(KEY_X, "key-x");
    K(KEY_Y, "key-y"); K(KEY_Z, "key-z");
    K(KEY_SPACE, "key-space"); K(KEY_ENTER, "key-enter");
    K(KEY_TAB, "key-tab"); K(KEY_BACKSPACE, "key-backspace");
    K(KEY_LEFTSHIFT, "key-leftshift"); K(KEY_RIGHTSHIFT, "key-rightshift");
    K(KEY_LEFTCTRL, "key-leftctrl"); K(KEY_RIGHTCTRL, "key-rightctrl");
    K(KEY_LEFTALT, "key-leftalt"); K(KEY_RIGHTALT, "key-rightalt");
    K(KEY_UP, "key-up"); K(KEY_DOWN, "key-down");
    K(KEY_LEFT, "key-left"); K(KEY_RIGHT, "key-right");
    K(KEY_F1, "key-f1"); K(KEY_F2, "key-f2"); K(KEY_F3, "key-f3");
    K(KEY_F4, "key-f4"); K(KEY_F5, "key-f5"); K(KEY_F6, "key-f6");
    K(KEY_F7, "key-f7"); K(KEY_F8, "key-f8"); K(KEY_F9, "key-f9");
    K(KEY_F10, "key-f10"); K(KEY_F11, "key-f11"); K(KEY_F12, "key-f12");
    K(KEY_HOME, "key-home"); K(KEY_END, "key-end");
    K(KEY_PAGEUP, "key-pageup"); K(KEY_PAGEDOWN, "key-pagedown");
    K(KEY_INSERT, "key-insert"); K(KEY_DELETE, "key-delete");

    /* Buttons */
    K(BTN_LEFT, "btn-left"); K(BTN_RIGHT, "btn-right");
    K(BTN_MIDDLE, "btn-middle");
    K(BTN_TRIGGER, "btn-trigger"); K(BTN_THUMB, "btn-thumb");
    K(BTN_THUMB2, "btn-thumb2"); K(BTN_TOP, "btn-top");
    K(BTN_TOP2, "btn-top2"); K(BTN_PINKIE, "btn-pinkie");
    K(BTN_BASE, "btn-base"); K(BTN_BASE2, "btn-base2");
    K(BTN_BASE3, "btn-base3"); K(BTN_BASE4, "btn-base4");
    K(BTN_BASE5, "btn-base5"); K(BTN_BASE6, "btn-base6");
#ifdef BTN_DEAD
    K(BTN_DEAD, "btn-dead");
#endif
    K(BTN_A, "btn-a"); K(BTN_B, "btn-b"); K(BTN_C, "btn-c");
    K(BTN_X, "btn-x"); K(BTN_Y, "btn-y"); K(BTN_Z, "btn-z");
    K(BTN_TL, "btn-tl"); K(BTN_TR, "btn-tr");
    K(BTN_TL2, "btn-tl2"); K(BTN_TR2, "btn-tr2");
    K(BTN_SELECT, "btn-select"); K(BTN_START, "btn-start");
    K(BTN_MODE, "btn-mode");
    K(BTN_THUMBL, "btn-thumbl"); K(BTN_THUMBR, "btn-thumbr");

    /* Relative axes */
    R(REL_X, "rel-x"); R(REL_Y, "rel-y"); R(REL_Z, "rel-z");
    R(REL_RX, "rel-rx"); R(REL_RY, "rel-ry"); R(REL_RZ, "rel-rz");
    R(REL_HWHEEL, "rel-hwheel"); R(REL_DIAL, "rel-dial");
    R(REL_WHEEL, "rel-wheel"); R(REL_MISC, "rel-misc");

    /* Absolute axes */
    A(ABS_X, "abs-x"); A(ABS_Y, "abs-y"); A(ABS_Z, "abs-z");
    A(ABS_RX, "abs-rx"); A(ABS_RY, "abs-ry"); A(ABS_RZ, "abs-rz");
    A(ABS_THROTTLE, "abs-throttle"); A(ABS_RUDDER, "abs-rudder");
    A(ABS_WHEEL, "abs-wheel"); A(ABS_GAS, "abs-gas");
    A(ABS_BRAKE, "abs-brake");
    A(ABS_HAT0X, "abs-hat0x"); A(ABS_HAT0Y, "abs-hat0y");
    A(ABS_HAT1X, "abs-hat1x"); A(ABS_HAT1Y, "abs-hat1y");
    A(ABS_HAT2X, "abs-hat2x"); A(ABS_HAT2Y, "abs-hat2y");
    A(ABS_HAT3X, "abs-hat3x"); A(ABS_HAT3Y, "abs-hat3y");
    A(ABS_MISC, "abs-misc");

#undef K
#undef R
#undef A
}

/* ========================================================================== */
/* Per-device data                                                             */
/* ========================================================================== */

typedef struct {
    gomc_hal_bit_t   *pin;      /* key state */
    gomc_hal_bit_t   *pin_not;  /* inverted */
} key_pin_t;

typedef struct {
    gomc_hal_float_t *position;
    gomc_hal_s32_t   *counts;
    gomc_hal_bit_t   *reset;
    gomc_hal_float_t *scale;
} rel_pin_t;

typedef struct {
    gomc_hal_float_t *position;
    gomc_hal_s32_t   *counts;
    gomc_hal_bit_t   *is_pos;
    gomc_hal_bit_t   *is_neg;
    gomc_hal_float_t *scale;
    gomc_hal_float_t *offset_pin;
    gomc_hal_s32_t   *fuzz;
    gomc_hal_s32_t   *flat;
    int               abs_min;
    int               abs_max;
} abs_pin_t;

typedef struct {
    int         fd;
    char        path[128];

    /* Bitmasks of which codes are supported */
    uint8_t     key_bits[(KEY_CNT + 7) / 8];
    uint8_t     rel_bits[(REL_CNT + 7) / 8];
    uint8_t     abs_bits[(ABS_CNT + 7) / 8];

    /* Pin storage (only allocated for supported codes) */
    key_pin_t   keys[KEY_CNT];
    bool        key_used[KEY_CNT];
    int         n_keys;

    rel_pin_t   rels[REL_CNT];
    bool        rel_used[REL_CNT];
    int         n_rels;

    abs_pin_t   abss[ABS_CNT];
    bool        abs_used[ABS_CNT];
    int         n_abss;

    char        parts[8]; /* K=keys R=rel A=abs L=leds */
} input_dev_t;

/* ========================================================================== */
/* Instance                                                                    */
/* ========================================================================== */

typedef struct {
    const cmod_env_t *env;
    const gomc_log_t *log;
    int               hal_id;

    input_dev_t       devs[MAX_DEVICES];
    int               n_devs;

    pthread_t         thread;
    volatile bool     running;
    int               exit_fd;
} input_inst_t;

/* ========================================================================== */
/* Evdev helpers                                                               */
/* ========================================================================== */

static bool test_bit(const uint8_t *bits, int n)
{
    return (bits[n / 8] >> (n % 8)) & 1;
}

static int open_input_device(input_inst_t *inst, input_dev_t *dev)
{
    dev->fd = open(dev->path, O_RDONLY | O_NONBLOCK);
    if (dev->fd < 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "cannot open %s: %s",
                        dev->path, strerror(errno));
        return -1;
    }

    /* Get supported event types */
    uint8_t evbits[(EV_CNT + 7) / 8];
    memset(evbits, 0, sizeof(evbits));
    ioctl(dev->fd, EVIOCGBIT(0, sizeof(evbits)), evbits);

    if (strchr(dev->parts, 'K') && test_bit(evbits, EV_KEY))
        ioctl(dev->fd, EVIOCGBIT(EV_KEY, sizeof(dev->key_bits)), dev->key_bits);
    if (strchr(dev->parts, 'R') && test_bit(evbits, EV_REL))
        ioctl(dev->fd, EVIOCGBIT(EV_REL, sizeof(dev->rel_bits)), dev->rel_bits);
    if (strchr(dev->parts, 'A') && test_bit(evbits, EV_ABS))
        ioctl(dev->fd, EVIOCGBIT(EV_ABS, sizeof(dev->abs_bits)), dev->abs_bits);

    return 0;
}

/* ========================================================================== */
/* HAL pin creation for one device                                             */
/* ========================================================================== */

static int create_device_pins(input_inst_t *inst, input_dev_t *dev, int idx)
{
    const gomc_hal_t *hal = inst->env->hal;
    int id = inst->hal_id;

    /* Key pins */
    for (int k = 0; k < KEY_CNT; k++) {
        if (!test_bit(dev->key_bits, k)) continue;
        const char *name = key_names[k];
        if (gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &dev->keys[k].pin,
                id, "input.%d.%s", idx, name) < 0) return -1;
        if (gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &dev->keys[k].pin_not,
                id, "input.%d.%s-not", idx, name) < 0) return -1;
        *dev->keys[k].pin_not = 1;
        dev->key_used[k] = true;
        dev->n_keys++;
    }

    /* Relative axis pins */
    for (int r = 0; r < REL_CNT; r++) {
        if (!test_bit(dev->rel_bits, r)) continue;
        const char *name = rel_names[r];
        rel_pin_t *rp = &dev->rels[r];
        if (gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &rp->position,
                id, "input.%d.%s-position", idx, name) < 0) return -1;
        if (gomc_hal_pin_s32_newf(hal, GOMC_HAL_OUT, &rp->counts,
                id, "input.%d.%s-counts", idx, name) < 0) return -1;
        if (gomc_hal_pin_bit_newf(hal, GOMC_HAL_IN, &rp->reset,
                id, "input.%d.%s-reset", idx, name) < 0) return -1;
        if (gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &rp->scale,
                id, "input.%d.%s-scale", idx, name) < 0) return -1;
        *rp->scale = 1.0;
        dev->rel_used[r] = true;
        dev->n_rels++;
    }

    /* Absolute axis pins */
    for (int a = 0; a < ABS_CNT; a++) {
        if (!test_bit(dev->abs_bits, a)) continue;
        const char *name = abs_names[a];
        abs_pin_t *ap = &dev->abss[a];

        struct input_absinfo absinfo;
        if (ioctl(dev->fd, EVIOCGABS(a), &absinfo) < 0)
            continue;

        if (gomc_hal_pin_float_newf(hal, GOMC_HAL_OUT, &ap->position,
                id, "input.%d.%s-position", idx, name) < 0) return -1;
        if (gomc_hal_pin_s32_newf(hal, GOMC_HAL_OUT, &ap->counts,
                id, "input.%d.%s-counts", idx, name) < 0) return -1;
        if (gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &ap->is_pos,
                id, "input.%d.%s-is-pos", idx, name) < 0) return -1;
        if (gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &ap->is_neg,
                id, "input.%d.%s-is-neg", idx, name) < 0) return -1;
        if (gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &ap->scale,
                id, "input.%d.%s-scale", idx, name) < 0) return -1;
        if (gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &ap->offset_pin,
                id, "input.%d.%s-offset", idx, name) < 0) return -1;
        if (gomc_hal_pin_s32_newf(hal, GOMC_HAL_IN, &ap->fuzz,
                id, "input.%d.%s-fuzz", idx, name) < 0) return -1;
        if (gomc_hal_pin_s32_newf(hal, GOMC_HAL_IN, &ap->flat,
                id, "input.%d.%s-flat", idx, name) < 0) return -1;

        ap->abs_min = absinfo.minimum;
        ap->abs_max = absinfo.maximum;

        double center = (absinfo.minimum + absinfo.maximum) / 2.0;
        double halfrange = (absinfo.maximum - absinfo.minimum) / 2.0;
        if (halfrange == 0.0) halfrange = 1.0;

        *ap->scale = halfrange;
        *ap->offset_pin = center;
        *ap->fuzz = absinfo.fuzz;
        *ap->flat = absinfo.flat;
        *ap->counts = absinfo.value;
        *ap->position = (absinfo.value - center) / halfrange;

        dev->abs_used[a] = true;
        dev->n_abss++;
    }

    return 0;
}

/* ========================================================================== */
/* Event processing                                                            */
/* ========================================================================== */

static void process_events(input_dev_t *dev)
{
    struct input_event ev;
    while (read(dev->fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        if (ev.type == EV_KEY && dev->key_used[ev.code]) {
            *dev->keys[ev.code].pin = ev.value ? 1 : 0;
            *dev->keys[ev.code].pin_not = ev.value ? 0 : 1;
        } else if (ev.type == EV_REL && ev.code < REL_CNT && dev->rel_used[ev.code]) {
            *dev->rels[ev.code].counts += ev.value;
        } else if (ev.type == EV_ABS && ev.code < ABS_CNT && dev->abs_used[ev.code]) {
            abs_pin_t *ap = &dev->abss[ev.code];
            int flat = *ap->flat;
            int center = (int)*ap->offset_pin;
            int fuzz = *ap->fuzz;
            int value;

            if (ev.value < center - flat || ev.value > center + flat)
                value = ev.value;
            else
                value = center;

            if (abs(value - (int)*ap->counts) > fuzz)
                *ap->counts = value;
        }
    }

    /* Update positions for relative axes */
    for (int r = 0; r < REL_CNT; r++) {
        if (!dev->rel_used[r]) continue;
        rel_pin_t *rp = &dev->rels[r];
        if (*rp->reset) *rp->counts = 0;
        double scale = *rp->scale;
        if (scale == 0.0) scale = 1.0;
        *rp->position = (double)*rp->counts / scale;
    }

    /* Update positions for absolute axes */
    for (int a = 0; a < ABS_CNT; a++) {
        if (!dev->abs_used[a]) continue;
        abs_pin_t *ap = &dev->abss[a];
        double scale = *ap->scale;
        double offset = *ap->offset_pin;
        if (scale == 0.0) scale = 1.0;
        double pos = ((double)*ap->counts - offset) / scale;
        *ap->position = pos;
        *ap->is_pos = (pos > 0.01);
        *ap->is_neg = (pos < -0.01);
    }
}

/* ========================================================================== */
/* Main loop thread                                                            */
/* ========================================================================== */

static void *input_thread(void *arg)
{
    input_inst_t *inst = (input_inst_t *)arg;

    struct pollfd pfds[MAX_DEVICES + 1];
    int nfds = 0;

    for (int i = 0; i < inst->n_devs; i++) {
        pfds[nfds].fd = inst->devs[i].fd;
        pfds[nfds].events = POLLIN;
        nfds++;
    }
    /* Add exit_fd for clean shutdown */
    pfds[nfds].fd = inst->exit_fd;
    pfds[nfds].events = POLLIN;
    nfds++;

    while (inst->running && !gomc_should_exit(inst->exit_fd)) {
        int ret = poll(pfds, nfds, 10); /* 10ms timeout */
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        for (int i = 0; i < inst->n_devs; i++) {
            if (pfds[i].revents & POLLIN)
                process_events(&inst->devs[i]);
        }
    }

    return NULL;
}

/* ========================================================================== */
/* cmod lifecycle                                                              */
/* ========================================================================== */

static int input_Init(cmod_t *self) { (void)self; return 0; }

static int input_Start(cmod_t *self)
{
    input_inst_t *inst = (input_inst_t *)self->priv;
    inst->running = true;
    if (pthread_create(&inst->thread, NULL, input_thread, inst) != 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "thread creation failed");
        return -1;
    }
    return 0;
}

static void input_Stop(cmod_t *self)
{
    input_inst_t *inst = (input_inst_t *)self->priv;
    inst->running = false;
    /* Signal exit_fd to wake poll */
    uint64_t val = 1;
    (void)write(inst->exit_fd, &val, sizeof(val));
    pthread_join(inst->thread, NULL);
}

static void input_Destroy(cmod_t *self)
{
    input_inst_t *inst = (input_inst_t *)self->priv;
    for (int i = 0; i < inst->n_devs; i++) {
        if (inst->devs[i].fd >= 0)
            close(inst->devs[i].fd);
    }
    if (inst->hal_id > 0)
        inst->env->hal->exit(inst->env->hal->ctx, inst->hal_id);
    if (inst->exit_fd >= 0)
        close(inst->exit_fd);
    free(inst);
    free(self);
}

/* ========================================================================== */
/* Constructor                                                                 */
/* ========================================================================== */

/*
 * Usage: load hal_input [-KRAL] /dev/input/event0 [-KR] /dev/input/event1 ...
 *
 * Flags before a device path control what event types to expose:
 *   K = keys/buttons, R = relative axes, A = absolute axes, L = LEDs
 *   Default is KRAL (all).
 */
int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)name;

    init_name_tables();

    input_inst_t *inst = calloc(1, sizeof(input_inst_t));
    if (!inst) return -1;

    inst->env = env;
    inst->log = env->log;
    inst->exit_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    /* Parse args: [-flags] device_path ... */
    char parts[8] = "KRAL";
    for (int i = 0; i < argc; i++) {
        if (argv[i][0] == '-' && !strchr(argv[i], '/')) {
            /* Flags for next device */
            strncpy(parts, argv[i] + 1, sizeof(parts) - 1);
            parts[sizeof(parts) - 1] = '\0';
        } else {
            /* Device path */
            if (inst->n_devs >= MAX_DEVICES) {
                gomc_log_errorf(inst->log, COMP_NAME, "too many devices (max %d)", MAX_DEVICES);
                free(inst);
                return -1;
            }
            input_dev_t *dev = &inst->devs[inst->n_devs];
            strncpy(dev->path, argv[i], sizeof(dev->path) - 1);
            strncpy(dev->parts, parts, sizeof(dev->parts) - 1);
            dev->fd = -1;

            if (open_input_device(inst, dev) < 0) {
                free(inst);
                return -1;
            }
            inst->n_devs++;
            /* Reset parts for next device */
            strcpy(parts, "KRAL");
        }
    }

    if (inst->n_devs == 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "no input devices specified");
        free(inst);
        return -1;
    }

    /* HAL init */
    const gomc_hal_t *hal = env->hal;
    inst->hal_id = hal->init(hal->ctx, COMP_NAME, env->dl_handle, GOMC_HAL_COMP_USER);
    if (inst->hal_id < 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "hal init failed");
        for (int i = 0; i < inst->n_devs; i++) close(inst->devs[i].fd);
        free(inst);
        return -1;
    }

    for (int i = 0; i < inst->n_devs; i++) {
        if (create_device_pins(inst, &inst->devs[i], i) < 0) {
            hal->exit(hal->ctx, inst->hal_id);
            for (int j = 0; j < inst->n_devs; j++) close(inst->devs[j].fd);
            free(inst);
            return -1;
        }
    }

    hal->ready(hal->ctx, inst->hal_id);

    gomc_log_infof(inst->log, COMP_NAME, "%d device(s) opened", inst->n_devs);

    cmod_t *mod = calloc(1, sizeof(cmod_t));
    if (!mod) {
        hal->exit(hal->ctx, inst->hal_id);
        for (int i = 0; i < inst->n_devs; i++) close(inst->devs[i].fd);
        free(inst);
        return -1;
    }
    mod->Init    = input_Init;
    mod->Start   = input_Start;
    mod->Stop    = input_Stop;
    mod->Destroy = input_Destroy;
    mod->priv    = inst;
    *out = mod;
    return 0;
}
