/*
 * mitsub_vfd.c — cmod driver for Mitsubishi VFDs (A500/E500/D700/E700/F700).
 *
 * Uses the Mitsubishi COMPUTER-LINK protocol over RS-485 (default 9600 8N2).
 * Supports multiple VFDs on the same bus via slave addressing.
 *
 * Originally a Python userspace component (bin/mitsub_vfd).
 *
 * Copyright (C) 2017 Chris Morley
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — C/cmod port
 * License: GPL v2+
 */

#include "gomc_env.h"
#include "gomc_user.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <pthread.h>
#include <sys/eventfd.h>

#define COMP_NAME "mitsub_vfd"
#define MAX_VFDS  8

/* ========================================================================== */
/* HAL pins (per VFD)                                                          */
/* ========================================================================== */

typedef struct {
    gomc_hal_bit_t   *fwd;          /* IN */
    gomc_hal_bit_t   *run;          /* IN */
    gomc_hal_bit_t   *estop;        /* IN */
    gomc_hal_bit_t   *debug;        /* IN */
    gomc_hal_bit_t   *monitor;      /* IN */
    gomc_hal_float_t *motor_cmd;    /* IN */
    gomc_hal_float_t *scale_cmd;    /* IN */
    gomc_hal_float_t *scale_fb;     /* IN */
    gomc_hal_float_t *scale_amps;   /* IN */
    gomc_hal_float_t *scale_volts;  /* IN */
    gomc_hal_float_t *scale_power;  /* IN */
    gomc_hal_float_t *scale_user;   /* IN */

    gomc_hal_bit_t   *up_to_speed;  /* OUT */
    gomc_hal_bit_t   *alarm;        /* OUT */
    gomc_hal_float_t *motor_fb;     /* OUT */
    gomc_hal_float_t *motor_amps;   /* OUT */
    gomc_hal_float_t *motor_volts;  /* OUT */
    gomc_hal_float_t *motor_power;  /* OUT */
    gomc_hal_float_t *motor_user;   /* OUT */
    gomc_hal_bit_t   *stat_bit[8];  /* OUT */
} vfd_pins_t;

typedef struct {
    char      name[32];
    char      slave[4];  /* "00"-"31" */
    vfd_pins_t pins;
    /* Cached state for edge detection */
    bool      last_run;
    bool      last_fwd;
    bool      last_estop;
    double    last_cmd;
} vfd_t;

/* ========================================================================== */
/* Instance                                                                    */
/* ========================================================================== */

typedef struct {
    const cmod_env_t *env;
    const gomc_log_t *log;
    int               hal_id;

    int               serial_fd;
    char              port[128];
    int               baud;

    vfd_t             vfds[MAX_VFDS];
    int               n_vfds;

    pthread_t         thread;
    volatile bool     running;
    int               exit_fd;
} mitsub_inst_t;

/* ========================================================================== */
/* Serial helpers                                                              */
/* ========================================================================== */

static speed_t baud_to_speed(int baud)
{
    switch (baud) {
        case 4800:  return B4800;
        case 9600:  return B9600;
        case 19200: return B19200;
        default:    return B9600;
    }
}

static int serial_open(mitsub_inst_t *inst)
{
    int fd = open(inst->port, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "cannot open %s: %s",
                        inst->port, strerror(errno));
        return -1;
    }

    struct termios tio;
    memset(&tio, 0, sizeof(tio));
    tio.c_cflag = baud_to_speed(inst->baud) | CS8 | CSTOPB | CLOCAL | CREAD;
    tio.c_iflag = IGNPAR;
    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 1; /* 100ms timeout */
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &tio);

    inst->serial_fd = fd;
    return 0;
}

static void serial_close(mitsub_inst_t *inst)
{
    if (inst->serial_fd >= 0) {
        close(inst->serial_fd);
        inst->serial_fd = -1;
    }
}

/* Flush input buffer */
static void serial_flush(mitsub_inst_t *inst)
{
    tcflush(inst->serial_fd, TCIFLUSH);
}

/* Read available response bytes. Returns length read. */
static int serial_poll(mitsub_inst_t *inst, char *buf, int maxlen)
{
    usleep(50000); /* 50ms wait for response */
    int pos = 0;
    while (pos < maxlen - 1) {
        ssize_t n = read(inst->serial_fd, &buf[pos], 1);
        if (n <= 0) break;
        pos++;
    }
    buf[pos] = '\0';
    return pos;
}

/* ========================================================================== */
/* Mitsubishi COMPUTER-LINK protocol                                           */
/* ========================================================================== */

/*
 * Frame format: ENQ + slave(2) + cmd(2) + '1' + [data] + checksum(2)
 * Checksum: sum of ASCII values of slave+cmd+'1'+data, take last 2 hex digits
 */
static int prepare_and_send(mitsub_inst_t *inst, const char *slave,
                            const char *cmd, const char *data)
{
    char frame[32];
    char body[20];
    int sum = 0;

    /* Build body: slave + cmd + '1' + data */
    if (data)
        snprintf(body, sizeof(body), "%s%s1%s", slave, cmd, data);
    else
        snprintf(body, sizeof(body), "%s%s1", slave, cmd);

    for (int i = 0; body[i]; i++)
        sum += (unsigned char)body[i];

    /* Frame: ENQ(0x05) + body + checksum(2 hex chars) */
    snprintf(frame, sizeof(frame), "\x05%s%c%c", body,
             "0123456789ABCDEF"[(sum >> 4) & 0xF],
             "0123456789ABCDEF"[sum & 0xF]);

    ssize_t len = strlen(frame);
    return (write(inst->serial_fd, frame, len) == len) ? 0 : -1;
}

/* Parse a read response — extract hex data from position 3..end-2 */
static int parse_response(const char *resp, int len, const char *field,
                          int data_len, unsigned int *val)
{
    /* Minimum response: STX(1) + slave(2) + data(data_len) + checksum(2) */
    (void)field;
    if (len < 3 + data_len + 2)
        return -1;
    /* Data starts at offset 3 (after STX + slave) */
    char hex[8];
    memcpy(hex, &resp[3], data_len);
    hex[data_len] = '\0';
    return (sscanf(hex, "%X", val) == 1) ? 0 : -1;
}

/* ========================================================================== */
/* VFD commands                                                                */
/* ========================================================================== */

static int read_monitor(mitsub_inst_t *inst, vfd_t *v, const char *reg,
                        int data_len, unsigned int *val)
{
    serial_flush(inst);
    if (prepare_and_send(inst, v->slave, reg, NULL) < 0)
        return -1;
    char resp[32];
    int rlen = serial_poll(inst, resp, sizeof(resp));
    return parse_response(resp, rlen, reg, data_len, val);
}

static void poll_vfd(mitsub_inst_t *inst, vfd_t *v)
{
    vfd_pins_t *p = &v->pins;
    unsigned int val;

    if (!*p->monitor) return;

    /* Status bits (register 7A, 2 hex chars = 1 byte) */
    if (read_monitor(inst, v, "7A", 2, &val) == 0) {
        *p->stat_bit[0] = (val >> 0) & 1;
        *p->stat_bit[1] = (val >> 1) & 1;
        *p->stat_bit[2] = (val >> 2) & 1;
        *p->stat_bit[3] = *p->up_to_speed = (val >> 3) & 1;
        *p->stat_bit[4] = (val >> 4) & 1;
        *p->stat_bit[5] = (val >> 5) & 1;
        *p->stat_bit[6] = (val >> 6) & 1;
        *p->stat_bit[7] = *p->alarm = (val >> 7) & 1;
    }

    /* Running frequency (register 6F, 4 hex chars) — in 0.01 Hz units */
    if (read_monitor(inst, v, "6F", 4, &val) == 0) {
        double scale = *p->scale_fb;
        if (scale == 0.0) scale = 1.0;
        *p->motor_fb = val * 0.01 * scale;
    }

    /* Current (register 70, 4 hex chars) — in 0.01 A units */
    if (read_monitor(inst, v, "70", 4, &val) == 0) {
        double scale = *p->scale_amps;
        if (scale == 0.0) scale = 1.0;
        *p->motor_amps = val * 0.01 * scale;
    }

    /* Voltage (register 71, 4 hex chars) — in 0.01 V units */
    if (read_monitor(inst, v, "71", 4, &val) == 0) {
        double scale = *p->scale_volts;
        if (scale == 0.0) scale = 1.0;
        *p->motor_volts = val * 0.01 * scale;
    }

    /* Power = V * A * sqrt(3) ≈ V * A * 2.7 (3-phase approx) */
    *p->motor_power = *p->motor_volts * *p->motor_amps * 2.7;

    /* User-selected monitor (register 72, 4 hex chars) */
    if (read_monitor(inst, v, "72", 4, &val) == 0) {
        double scale = *p->scale_user;
        if (scale == 0.0) scale = 1.0;
        *p->motor_user = val * scale;
    }
}

static void send_command(mitsub_inst_t *inst, vfd_t *v)
{
    vfd_pins_t *p = &v->pins;

    /* E-stop handling */
    bool estop = *p->estop;
    if (estop != v->last_estop) {
        v->last_estop = estop;
        if (!estop) {
            /* Stop on estop loss */
            prepare_and_send(inst, v->slave, "FA", "00");
            usleep(50000);
            gomc_log_warnf(inst->log, COMP_NAME, "%s: stopped due to estop", v->name);
            return;
        } else {
            /* Reset VFD after estop clear */
            prepare_and_send(inst, v->slave, "FD", NULL);
            usleep(50000);
            gomc_log_infof(inst->log, COMP_NAME, "%s: estop cleared", v->name);
        }
    }

    /* Run/direction changes */
    bool run = *p->run;
    bool fwd = *p->fwd;
    if (run != v->last_run || fwd != v->last_fwd) {
        const char *data;
        if (run)
            data = fwd ? "02" : "04";
        else
            data = "00";
        prepare_and_send(inst, v->slave, "FA", data);
        usleep(50000);
        v->last_run = run;
        v->last_fwd = fwd;
    }

    /* Frequency command (register ED, 4 hex chars in 0.01 Hz) */
    double cmd = *p->motor_cmd;
    if (cmd != v->last_cmd) {
        double scale = *p->scale_cmd;
        if (scale == 0.0) scale = 1.0;
        int freq = (int)(fabs(cmd) * 100.0 * scale);
        if (freq > 40000) freq = 40000;
        if (freq < 0) freq = 0;
        char data[8];
        snprintf(data, sizeof(data), "%04X", freq);
        prepare_and_send(inst, v->slave, "ED", data);
        usleep(50000);
        v->last_cmd = cmd;
    }
}

/* ========================================================================== */
/* Main loop thread                                                            */
/* ========================================================================== */

static void *mitsub_thread(void *arg)
{
    mitsub_inst_t *inst = (mitsub_inst_t *)arg;

    if (serial_open(inst) < 0)
        return NULL;

    /* Set special user monitor to power (0E = kW) */
    for (int i = 0; i < inst->n_vfds; i++) {
        prepare_and_send(inst, inst->vfds[i].slave, "F3", "0E");
        usleep(50000);
    }

    while (inst->running && !gomc_should_exit(inst->exit_fd)) {
        for (int i = 0; i < inst->n_vfds; i++) {
            if (!inst->running) break;
            send_command(inst, &inst->vfds[i]);
            poll_vfd(inst, &inst->vfds[i]);
        }
    }

    /* Kill all outputs on exit */
    for (int i = 0; i < inst->n_vfds; i++) {
        prepare_and_send(inst, inst->vfds[i].slave, "FA", "00");
        usleep(50000);
    }

    serial_close(inst);
    return NULL;
}

/* ========================================================================== */
/* HAL pin creation                                                            */
/* ========================================================================== */

static int create_vfd_pins(mitsub_inst_t *inst, vfd_t *v)
{
    const gomc_hal_t *hal = inst->env->hal;
    int id = inst->hal_id;
    vfd_pins_t *p = &v->pins;
    const char *n = v->name;

#define P(dir, type, ptr, pname) \
    if (gomc_hal_pin_##type##_newf(hal, dir, ptr, id, "%s." pname, n) < 0) return -1

    P(GOMC_HAL_IN,  bit,   &p->fwd,         "fwd");
    P(GOMC_HAL_IN,  bit,   &p->run,         "run");
    P(GOMC_HAL_IN,  bit,   &p->estop,       "estop");
    P(GOMC_HAL_IN,  bit,   &p->debug,       "debug");
    P(GOMC_HAL_IN,  bit,   &p->monitor,     "monitor");
    P(GOMC_HAL_IN,  float, &p->motor_cmd,   "motor-cmd");
    P(GOMC_HAL_IN,  float, &p->scale_cmd,   "scale-cmd");
    P(GOMC_HAL_IN,  float, &p->scale_fb,    "scale-fb");
    P(GOMC_HAL_IN,  float, &p->scale_amps,  "scale-amps");
    P(GOMC_HAL_IN,  float, &p->scale_volts, "scale-volts");
    P(GOMC_HAL_IN,  float, &p->scale_power, "scale-power");
    P(GOMC_HAL_IN,  float, &p->scale_user,  "scale-user");
    P(GOMC_HAL_OUT, bit,   &p->up_to_speed, "up-to-speed");
    P(GOMC_HAL_OUT, bit,   &p->alarm,       "alarm");
    P(GOMC_HAL_OUT, float, &p->motor_fb,    "motor-fb");
    P(GOMC_HAL_OUT, float, &p->motor_amps,  "motor-amps");
    P(GOMC_HAL_OUT, float, &p->motor_volts, "motor-volts");
    P(GOMC_HAL_OUT, float, &p->motor_power, "motor-power");
    P(GOMC_HAL_OUT, float, &p->motor_user,  "motor-user");

    for (int b = 0; b < 8; b++) {
        if (gomc_hal_pin_bit_newf(hal, GOMC_HAL_OUT, &p->stat_bit[b],
                id, "%s.stat-bit-%d", n, b) < 0)
            return -1;
    }
#undef P

    /* Set defaults */
    *p->scale_cmd   = 1.0;
    *p->scale_fb    = 1.0;
    *p->scale_amps  = 1.0;
    *p->scale_volts = 1.0;
    *p->scale_power = 1.0;
    *p->scale_user  = 1.0;
    *p->fwd = 1;

    return 0;
}

/* ========================================================================== */
/* cmod lifecycle                                                              */
/* ========================================================================== */

static int mitsub_Init(cmod_t *self) { (void)self; return 0; }

static int mitsub_Start(cmod_t *self)
{
    mitsub_inst_t *inst = (mitsub_inst_t *)self->priv;
    inst->running = true;
    if (pthread_create(&inst->thread, NULL, mitsub_thread, inst) != 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "thread creation failed");
        return -1;
    }
    return 0;
}

static void mitsub_Stop(cmod_t *self)
{
    mitsub_inst_t *inst = (mitsub_inst_t *)self->priv;
    inst->running = false;
    pthread_join(inst->thread, NULL);
}

static void mitsub_Destroy(cmod_t *self)
{
    mitsub_inst_t *inst = (mitsub_inst_t *)self->priv;
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
 * Usage: load mitsub_vfd [--port /dev/ttyUSB0] [--baud 9600] [name=slave ...]
 *
 * Examples:
 *   load mitsub_vfd spindle=02 coolant=01
 *   load mitsub_vfd --port /dev/ttyS0 --baud 4800 spindle=02
 */
int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)name;

    mitsub_inst_t *inst = calloc(1, sizeof(mitsub_inst_t));
    if (!inst) return -1;

    inst->env = env;
    inst->log = env->log;
    inst->serial_fd = -1;
    inst->exit_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    inst->baud = 9600;
    strncpy(inst->port, "/dev/ttyS0", sizeof(inst->port) - 1);

    /* Parse args */
    for (int i = 0; i < argc; i++) {
        if ((strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) && i + 1 < argc) {
            strncpy(inst->port, argv[++i], sizeof(inst->port) - 1);
        } else if ((strcmp(argv[i], "--baud") == 0 || strcmp(argv[i], "-b") == 0) && i + 1 < argc) {
            inst->baud = atoi(argv[++i]);
        } else if (strchr(argv[i], '=')) {
            /* name=slave_number */
            if (inst->n_vfds >= MAX_VFDS) {
                gomc_log_errorf(inst->log, COMP_NAME, "too many VFDs (max %d)", MAX_VFDS);
                free(inst);
                return -1;
            }
            vfd_t *v = &inst->vfds[inst->n_vfds];
            const char *eq = strchr(argv[i], '=');
            int nlen = (int)(eq - argv[i]);
            if (nlen > 31) nlen = 31;
            memcpy(v->name, argv[i], nlen);
            v->name[nlen] = '\0';
            strncpy(v->slave, eq + 1, sizeof(v->slave) - 1);
            inst->n_vfds++;
        }
    }

    /* Default: single VFD named "mitsub_vfd" at slave 00 */
    if (inst->n_vfds == 0) {
        vfd_t *v = &inst->vfds[0];
        strcpy(v->name, COMP_NAME);
        strcpy(v->slave, "00");
        inst->n_vfds = 1;
    }

    /* HAL init */
    const gomc_hal_t *hal = env->hal;
    inst->hal_id = hal->init(hal->ctx, COMP_NAME, env->dl_handle, GOMC_HAL_COMP_USER);
    if (inst->hal_id < 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "hal init failed");
        free(inst);
        return -1;
    }

    for (int i = 0; i < inst->n_vfds; i++) {
        if (create_vfd_pins(inst, &inst->vfds[i]) < 0) {
            hal->exit(hal->ctx, inst->hal_id);
            free(inst);
            return -1;
        }
    }

    hal->ready(hal->ctx, inst->hal_id);

    gomc_log_infof(inst->log, COMP_NAME, "port=%s baud=%d vfds=%d",
                   inst->port, inst->baud, inst->n_vfds);

    cmod_t *mod = calloc(1, sizeof(cmod_t));
    if (!mod) {
        hal->exit(hal->ctx, inst->hal_id);
        free(inst);
        return -1;
    }
    mod->Init    = mitsub_Init;
    mod->Start   = mitsub_Start;
    mod->Stop    = mitsub_Stop;
    mod->Destroy = mitsub_Destroy;
    mod->priv    = inst;
    *out = mod;
    return 0;
}
