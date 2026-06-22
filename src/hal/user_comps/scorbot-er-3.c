/*
 * scorbot-er-3.c — cmod driver for Scorbot ER-3 robot arm control box.
 *
 * Interfaces the control box over RS-232 serial (9600 baud, 8N2).
 * Commands 8 joints via delta encoder counts and reads limit switches.
 *
 * Originally a Python userspace component (bin/scorbot-er-3).
 *
 * Copyright (C) 2013-2016 Sebastian Kuzminsky <seb@highlab.com>
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — C/cmod port
 * License: GPL v2+
 */

#include "gomc_env.h"
#include "gomc_user.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/eventfd.h>

#define COMP_NAME    "scorbot-er-3"
#define PIN_PREFIX   "scorbot-er-3"
#define N_JOINTS     8
#define SERIAL_BAUD  B9600
#define LOOP_PERIOD_US 1000  /* 1ms */

/* ========================================================================== */
/* HAL pins                                                                    */
/* ========================================================================== */

typedef struct {
    gomc_hal_bit_t   *limit_sw[N_JOINTS];     /* OUT */
    gomc_hal_float_t *motor_pos_cmd[N_JOINTS]; /* IN */
    gomc_hal_float_t *scale[N_JOINTS];         /* IN */
    gomc_hal_s32_t   *motor_max_vel[N_JOINTS]; /* IN */
} hal_pins_t;

/* ========================================================================== */
/* Instance data                                                               */
/* ========================================================================== */

typedef struct {
    const cmod_env_t *env;
    const gomc_log_t *log;
    int               hal_id;
    hal_pins_t       *pins;

    int               serial_fd;
    char              port[128];

    int               old_counts[N_JOINTS];
    int               old_max_vel[N_JOINTS];

    pthread_t         thread;
    volatile bool     running;
    int               exit_fd;
} scorbot_inst_t;

/* ========================================================================== */
/* Serial helpers                                                              */
/* ========================================================================== */

static int serial_open(scorbot_inst_t *inst)
{
    int fd = open(inst->port, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "cannot open %s: %s",
                        inst->port, strerror(errno));
        return -1;
    }

    struct termios tio;
    memset(&tio, 0, sizeof(tio));
    tio.c_cflag = SERIAL_BAUD | CS8 | CSTOPB | CLOCAL | CREAD;
    tio.c_iflag = IGNPAR;
    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 10; /* 1s timeout */
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &tio);

    inst->serial_fd = fd;
    return 0;
}

static void serial_close(scorbot_inst_t *inst)
{
    if (inst->serial_fd >= 0) {
        close(inst->serial_fd);
        inst->serial_fd = -1;
    }
}

static int serial_write_str(scorbot_inst_t *inst, const char *s)
{
    size_t len = strlen(s);
    ssize_t n = write(inst->serial_fd, s, len);
    if (n < 0) {
        gomc_log_warnf(inst->log, COMP_NAME, "serial write error: %s", strerror(errno));
        return -1;
    }
    return 0;
}

static int serial_read_bytes(scorbot_inst_t *inst, unsigned char *buf, int count)
{
    int got = 0;
    while (got < count) {
        ssize_t n = read(inst->serial_fd, buf + got, count - got);
        if (n < 0) {
            if (errno == EINTR) continue;
            gomc_log_warnf(inst->log, COMP_NAME, "serial read error: %s", strerror(errno));
            return -1;
        }
        if (n == 0) {
            gomc_log_warnf(inst->log, COMP_NAME, "serial read timeout");
            return -1;
        }
        got += n;
    }
    return 0;
}

/* ========================================================================== */
/* Main loop thread                                                            */
/* ========================================================================== */

static void *scorbot_thread(void *arg)
{
    scorbot_inst_t *inst = (scorbot_inst_t *)arg;
    hal_pins_t *p = inst->pins;
    char cmd[32];

    while (inst->running && !gomc_should_exit(inst->exit_fd)) {
        for (int j = 0; j < N_JOINTS; j++) {
            /* Compute new encoder counts from position * scale */
            double pos = *p->motor_pos_cmd[j];
            double sc  = *p->scale[j];
            int new_counts;

            if (isnan(pos))
                new_counts = inst->old_counts[j];
            else
                new_counts = (int)(pos * sc);

            /* Send delta move if counts changed */
            if (new_counts != inst->old_counts[j]) {
                int delta = new_counts - inst->old_counts[j];
                snprintf(cmd, sizeof(cmd), "%dm%d\n\r", j + 1, delta);
                serial_write_str(inst, cmd);
                inst->old_counts[j] = new_counts;
            }

            /* Update velocity if changed */
            int vel = *p->motor_max_vel[j];
            if (vel != inst->old_max_vel[j]) {
                snprintf(cmd, sizeof(cmd), "%dv%d\n", j + 1, vel);
                serial_write_str(inst, cmd);
                inst->old_max_vel[j] = vel;
            }

            /* Request limit switch status */
            snprintf(cmd, sizeof(cmd), "%dl", j + 1);
            serial_write_str(inst, cmd);
        }

        /* Read 8 bytes of limit switch data */
        unsigned char data[N_JOINTS];
        if (serial_read_bytes(inst, data, N_JOINTS) == 0) {
            for (int j = 0; j < N_JOINTS; j++)
                *p->limit_sw[j] = (data[j] != '0');
        }

        usleep(LOOP_PERIOD_US);
    }

    return NULL;
}

/* ========================================================================== */
/* HAL pin creation                                                            */
/* ========================================================================== */

static int create_hal_pins(scorbot_inst_t *inst)
{
    const gomc_hal_t *hal = inst->env->hal;
    int comp_id = inst->hal_id;
    hal_pins_t *p = inst->pins;

    for (int j = 0; j < N_JOINTS; j++) {
        if (gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &p->limit_sw[j],
                comp_id, "%s.joint%d.limit-sw", PIN_PREFIX, j) < 0)
            return -1;
        if (gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &p->motor_pos_cmd[j],
                comp_id, "%s.joint%d.motor-pos-cmd", PIN_PREFIX, j) < 0)
            return -1;
        if (gomc_hal_pin_float_newf(hal, GOMC_HAL_IN, &p->scale[j],
                comp_id, "%s.joint%d.scale", PIN_PREFIX, j) < 0)
            return -1;
        if (gomc_hal_pin_s32_newf(hal, GOMC_HAL_IN, &p->motor_max_vel[j],
                comp_id, "%s.joint%d.motor-max-vel", PIN_PREFIX, j) < 0)
            return -1;
    }
    return 0;
}

/* ========================================================================== */
/* cmod lifecycle                                                              */
/* ========================================================================== */

static int scorbot_Init(cmod_t *self)  { (void)self; return 0; }

static int scorbot_Start(cmod_t *self)
{
    scorbot_inst_t *inst = (scorbot_inst_t *)self->priv;

    if (serial_open(inst) < 0)
        return -1;

    /* Disable "interrupts" from the control box */
    serial_write_str(inst, "X");

    inst->running = true;
    if (pthread_create(&inst->thread, NULL, scorbot_thread, inst) != 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "thread creation failed");
        serial_close(inst);
        return -1;
    }

    return 0;
}

static void scorbot_Stop(cmod_t *self)
{
    scorbot_inst_t *inst = (scorbot_inst_t *)self->priv;
    inst->running = false;
    pthread_join(inst->thread, NULL);
    serial_close(inst);
}

static void scorbot_Destroy(cmod_t *self)
{
    scorbot_inst_t *inst = (scorbot_inst_t *)self->priv;
    if (inst->hal_id > 0)
        inst->env->hal->exit(inst->env->hal->ctx, inst->hal_id);
    free(inst);
    free(self);
}

/* ========================================================================== */
/* New — constructor                                                           */
/* ========================================================================== */

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)name;

    scorbot_inst_t *inst = calloc(1, sizeof(scorbot_inst_t));
    if (!inst) return -1;

    inst->env = env;
    inst->log = env->log;
    inst->serial_fd = -1;
    inst->exit_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    strncpy(inst->port, "/dev/ttyS0", sizeof(inst->port) - 1);

    /* Parse args */
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "port=", 5) == 0)
            strncpy(inst->port, argv[i] + 5, sizeof(inst->port) - 1);
    }

    /* Set default scale and velocity */
    for (int j = 0; j < N_JOINTS; j++) {
        inst->old_max_vel[j] = 0; /* force update on first iteration */
    }

    /* HAL init */
    const gomc_hal_t *hal = env->hal;
    inst->hal_id = hal->init(hal->ctx, COMP_NAME, env->dl_handle, GOMC_HAL_COMP_USER);
    if (inst->hal_id < 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "hal init failed");
        free(inst);
        return -1;
    }

    inst->pins = hal->malloc(hal->ctx, sizeof(hal_pins_t));
    if (!inst->pins) {
        gomc_log_errorf(inst->log, COMP_NAME, "hal malloc failed");
        hal->exit(hal->ctx, inst->hal_id);
        free(inst);
        return -1;
    }
    memset(inst->pins, 0, sizeof(hal_pins_t));

    if (create_hal_pins(inst) < 0) {
        hal->exit(hal->ctx, inst->hal_id);
        free(inst);
        return -1;
    }

    /* Set default scale=1.0 and velocity=9 */
    for (int j = 0; j < N_JOINTS; j++) {
        *inst->pins->scale[j] = 1.0;
        *inst->pins->motor_max_vel[j] = 9;
    }

    hal->ready(hal->ctx, inst->hal_id);

    /* Build cmod */
    cmod_t *mod = calloc(1, sizeof(cmod_t));
    if (!mod) {
        hal->exit(hal->ctx, inst->hal_id);
        free(inst);
        return -1;
    }
    mod->Init    = scorbot_Init;
    mod->Start   = scorbot_Start;
    mod->Stop    = scorbot_Stop;
    mod->Destroy = scorbot_Destroy;
    mod->priv    = inst;
    *out = mod;
    return 0;
}
