/*
 * pmx485.c — cmod driver for Hypertherm Powermax plasma cutters (RS-485).
 *
 * Communicates using Modbus ASCII protocol (19200 8E1) to read/write
 * cutting mode, current, pressure, fault codes, and arc on time.
 *
 * Originally a Python userspace component (bin/pmx485).
 *
 * Copyright (C) 2019-2021 Phillip A Carter
 * Copyright (C) 2020-2021 Gregory D Carl
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

#define COMP_NAME "pmx485"

/* Modbus ASCII protocol constants */
#define ADDRESS     "01"
#define REG_READ    "04"
#define REG_WRITE   "06"
#define VALID_READ  "0402"

/* Register addresses */
#define REG_MODE         "2093"
#define REG_CURRENT      "2094"
#define REG_PRESSURE     "2096"
#define REG_FAULT        "2098"
#define REG_CURRENT_MIN  "2099"
#define REG_CURRENT_MAX  "209A"
#define REG_PRESSURE_MIN "209C"
#define REG_PRESSURE_MAX "209D"
#define REG_ARC_TIME_LOW "209E"
#define REG_ARC_TIME_HI  "209F"

/* ========================================================================== */
/* HAL pins                                                                    */
/* ========================================================================== */

typedef struct {
    gomc_hal_float_t *mode_set;       /* IN */
    gomc_hal_float_t *current_set;    /* IN */
    gomc_hal_float_t *pressure_set;   /* IN */
    gomc_hal_bit_t   *enable;         /* IN */
    gomc_hal_float_t *mode;           /* OUT */
    gomc_hal_float_t *current;        /* OUT */
    gomc_hal_float_t *pressure;       /* OUT */
    gomc_hal_float_t *fault;          /* OUT */
    gomc_hal_bit_t   *status;         /* OUT */
    gomc_hal_float_t *current_min;    /* OUT */
    gomc_hal_float_t *current_max;    /* OUT */
    gomc_hal_float_t *pressure_min;   /* OUT */
    gomc_hal_float_t *pressure_max;   /* OUT */
    gomc_hal_float_t *arc_time;       /* OUT */
} hal_pins_t;

/* ========================================================================== */
/* Instance                                                                    */
/* ========================================================================== */

typedef struct {
    const cmod_env_t *env;
    const gomc_log_t *log;
    int               hal_id;
    hal_pins_t       *pins;

    int               serial_fd;
    char              port[128];

    pthread_t         thread;
    volatile bool     running;
    int               exit_fd;
} pmx_inst_t;

/* ========================================================================== */
/* Serial helpers                                                              */
/* ========================================================================== */

static int serial_open(pmx_inst_t *inst)
{
    int fd = open(inst->port, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "cannot open %s: %s",
                        inst->port, strerror(errno));
        return -1;
    }

    struct termios tio;
    memset(&tio, 0, sizeof(tio));
    tio.c_cflag = B19200 | CS8 | PARENB | CLOCAL | CREAD;  /* 8E1 */
    tio.c_iflag = IGNPAR;
    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 1; /* 100ms timeout */
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &tio);

    inst->serial_fd = fd;
    return 0;
}

static void serial_close(pmx_inst_t *inst)
{
    if (inst->serial_fd >= 0) {
        close(inst->serial_fd);
        inst->serial_fd = -1;
    }
}

/* Read a line (up to \n) with timeout. Returns length or -1. */
static int serial_readline(pmx_inst_t *inst, char *buf, int maxlen)
{
    int pos = 0;
    while (pos < maxlen - 1) {
        ssize_t n = read(inst->serial_fd, &buf[pos], 1);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break; /* timeout */
        if (buf[pos] == '\n') { pos++; break; }
        pos++;
    }
    buf[pos] = '\0';
    return pos;
}

/* ========================================================================== */
/* Modbus ASCII framing                                                        */
/* ========================================================================== */

/* Compute LRC checksum over hex string (pairs of hex chars) */
static unsigned char compute_lrc(const char *hex, int len)
{
    unsigned char lrc = 0;
    for (int i = 0; i < len; i += 2) {
        unsigned int val;
        if (sscanf(&hex[i], "%2X", &val) != 1)
            return 0;
        lrc += (unsigned char)val;
    }
    return (unsigned char)(((~lrc) + 1) & 0xFF);
}

/* Write a register. Returns 1 on success, 0 on failure. */
static int write_register(pmx_inst_t *inst, const char *reg, int value)
{
    char data[16], packet[24], reply[24];

    snprintf(data, sizeof(data), "%s%s%s%04X", ADDRESS, REG_WRITE, reg, (unsigned)value & 0xFFFF);
    if (strlen(data) != 12) return 0;

    unsigned char lrc = compute_lrc(data, 12);
    snprintf(packet, sizeof(packet), ":%s%02X\r\n", data, lrc);

    ssize_t wlen = strlen(packet);
    if (write(inst->serial_fd, packet, wlen) != wlen)
        return 0;

    int rlen = serial_readline(inst, reply, sizeof(reply));
    if (rlen <= 0) return 0;

    /* Echo match = success */
    return (strcmp(reply, packet) == 0) ? 1 : 0;
}

/* Read a register. Returns value on success, -1 on failure. */
static int read_register(pmx_inst_t *inst, const char *reg)
{
    char data[16], packet[24], reply[24];

    snprintf(data, sizeof(data), "%s%s%s0001", ADDRESS, REG_READ, reg);
    if (strlen(data) != 12) return -1;

    unsigned char lrc = compute_lrc(data, 12);
    snprintf(packet, sizeof(packet), ":%s%02X\r\n", data, lrc);

    ssize_t wlen = strlen(packet);
    if (write(inst->serial_fd, packet, wlen) != wlen)
        return -1;

    int rlen = serial_readline(inst, reply, sizeof(reply));
    if (rlen < 15) return -1;

    /* Validate: ":01040200xx...LRC\r\n" — header is ":0104 02" */
    char expected_hdr[8];
    snprintf(expected_hdr, sizeof(expected_hdr), ":%s%s", ADDRESS, VALID_READ);
    if (strncmp(reply, expected_hdr, 7) != 0)
        return -1;

    /* Verify LRC: data is reply[1..10], LRC at reply[11..12] */
    unsigned char calc_lrc = compute_lrc(&reply[1], 10);
    unsigned int recv_lrc;
    if (sscanf(&reply[11], "%2X", &recv_lrc) != 1)
        return -1;
    if (calc_lrc != (unsigned char)recv_lrc)
        return -1;

    /* Value is at reply[7..10] */
    unsigned int val;
    if (sscanf(&reply[7], "%4X", &val) != 1)
        return -1;

    return (int)val;
}

/* ========================================================================== */
/* Higher-level commands                                                       */
/* ========================================================================== */

static void close_machine(pmx_inst_t *inst)
{
    write_register(inst, REG_MODE, 0);
    write_register(inst, REG_CURRENT, 0);
    write_register(inst, REG_PRESSURE, 0);
}

static bool open_machine(pmx_inst_t *inst)
{
    hal_pins_t *p = inst->pins;
    int mode = write_register(inst, REG_MODE, (int)*p->mode_set);
    int cur  = write_register(inst, REG_CURRENT, (int)(*p->current_set * 64.0));
    int prs  = write_register(inst, REG_PRESSURE, (int)(*p->pressure_set * 128.0));
    return (mode && cur && prs);
}

static bool get_limits(pmx_inst_t *inst)
{
    hal_pins_t *p = inst->pins;
    int cmin = read_register(inst, REG_CURRENT_MIN);
    if (cmin >= 0) *p->current_min = cmin / 64.0;
    int cmax = read_register(inst, REG_CURRENT_MAX);
    if (cmax >= 0) *p->current_max = cmax / 64.0;
    int pmin = read_register(inst, REG_PRESSURE_MIN);
    if (pmin >= 0) *p->pressure_min = pmin / 128.0;
    int pmax = read_register(inst, REG_PRESSURE_MAX);
    if (pmax >= 0) *p->pressure_max = pmax / 128.0;
    return (cmin >= 0 && cmax >= 0 && pmin >= 0 && pmax >= 0);
}

/* ========================================================================== */
/* Main loop thread                                                            */
/* ========================================================================== */

static void *pmx_thread(void *arg)
{
    pmx_inst_t *inst = (pmx_inst_t *)arg;
    hal_pins_t *p = inst->pins;
    bool started = false;
    bool was_enabled = false;
    int error_count = 0;

    while (inst->running && !gomc_should_exit(inst->exit_fd)) {
        bool enabled = *p->enable;

        /* Detect enable edge */
        if (was_enabled != enabled) {
            was_enabled = enabled;
            if (!enabled) {
                close_machine(inst);
                serial_close(inst);
                *p->status = 0;
                started = false;
            }
        }

        if (enabled) {
            if (!started) {
                if (inst->serial_fd < 0) {
                    if (serial_open(inst) < 0) {
                        usleep(100000);
                        continue;
                    }
                }
                if (open_machine(inst)) {
                    started = get_limits(inst);
                }
                if (!started) {
                    usleep(100000);
                    continue;
                }
            } else {
                bool ok = true;
                int val;

                /* Set/get mode */
                if ((int)*p->mode_set != (int)*p->mode) {
                    if (write_register(inst, REG_MODE, (int)*p->mode_set)) {
                        *p->mode = *p->mode_set;
                        get_limits(inst);
                    } else ok = false;
                } else {
                    val = read_register(inst, REG_MODE);
                    if (val >= 0) *p->mode = val;
                    else ok = false;
                }

                /* Set/get current */
                if ((int)(*p->current_set * 10) != (int)(*p->current * 10)) {
                    if (write_register(inst, REG_CURRENT, (int)(*p->current_set * 64)))
                        *p->current = *p->current_set;
                    else ok = false;
                } else {
                    val = read_register(inst, REG_CURRENT);
                    if (val >= 0) *p->current = val / 64.0;
                    else ok = false;
                }

                /* Set/get pressure */
                if ((int)(*p->pressure_set * 10) != (int)(*p->pressure * 10)) {
                    if (write_register(inst, REG_PRESSURE, (int)(*p->pressure_set * 128)))
                        *p->pressure = *p->pressure_set;
                    else ok = false;
                } else {
                    val = read_register(inst, REG_PRESSURE);
                    if (val >= 0) *p->pressure = val / 128.0;
                    else ok = false;
                }

                /* Get fault */
                val = read_register(inst, REG_FAULT);
                if (val >= 0) *p->fault = val;
                else ok = false;

                /* Get arc time */
                int atl = read_register(inst, REG_ARC_TIME_LOW);
                int ath = read_register(inst, REG_ARC_TIME_HI);
                if (atl >= 0 && ath >= 0)
                    *p->arc_time = (double)((ath << 16) | atl);
                else ok = false;

                /* Status tracking */
                if (ok) {
                    *p->status = 1;
                    error_count = 0;
                } else {
                    error_count++;
                    if (error_count > 3) {
                        error_count = 0;
                        *p->status = 0;
                        started = false;
                        close_machine(inst);
                        serial_close(inst);
                        usleep(100000);
                    }
                }
            }
        } else {
            usleep(100000);
        }
    }

    if (started) {
        close_machine(inst);
        serial_close(inst);
    }
    return NULL;
}

/* ========================================================================== */
/* HAL pin creation                                                            */
/* ========================================================================== */

static int create_hal_pins(pmx_inst_t *inst)
{
    const gomc_hal_t *hal = inst->env->hal;
    int id = inst->hal_id;
    hal_pins_t *p = inst->pins;

#define P(dir, type, ptr, name) \
    if (gomc_hal_pin_##type##_newf(hal, dir, ptr, id, "%s." name, COMP_NAME) < 0) return -1

    P(GOMC_HAL_IN,  float, &p->mode_set,     "mode_set");
    P(GOMC_HAL_IN,  float, &p->current_set,  "current_set");
    P(GOMC_HAL_IN,  float, &p->pressure_set, "pressure_set");
    P(GOMC_HAL_IN,  bit,   &p->enable,       "enable");
    P(GOMC_HAL_OUT, float, &p->mode,         "mode");
    P(GOMC_HAL_OUT, float, &p->current,      "current");
    P(GOMC_HAL_OUT, float, &p->pressure,     "pressure");
    P(GOMC_HAL_OUT, float, &p->fault,        "fault");
    P(GOMC_HAL_OUT, bit,   &p->status,       "status");
    P(GOMC_HAL_OUT, float, &p->current_min,  "current_min");
    P(GOMC_HAL_OUT, float, &p->current_max,  "current_max");
    P(GOMC_HAL_OUT, float, &p->pressure_min, "pressure_min");
    P(GOMC_HAL_OUT, float, &p->pressure_max, "pressure_max");
    P(GOMC_HAL_OUT, float, &p->arc_time,     "arcTime");
#undef P
    return 0;
}

/* ========================================================================== */
/* cmod lifecycle                                                              */
/* ========================================================================== */

static int pmx_Init(cmod_t *self) { (void)self; return 0; }

static int pmx_Start(cmod_t *self)
{
    pmx_inst_t *inst = (pmx_inst_t *)self->priv;
    inst->running = true;
    if (pthread_create(&inst->thread, NULL, pmx_thread, inst) != 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "thread creation failed");
        return -1;
    }
    return 0;
}

static void pmx_Stop(cmod_t *self)
{
    pmx_inst_t *inst = (pmx_inst_t *)self->priv;
    inst->running = false;
    pthread_join(inst->thread, NULL);
}

static void pmx_Destroy(cmod_t *self)
{
    pmx_inst_t *inst = (pmx_inst_t *)self->priv;
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

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)name;

    pmx_inst_t *inst = calloc(1, sizeof(pmx_inst_t));
    if (!inst) return -1;

    inst->env = env;
    inst->log = env->log;
    inst->serial_fd = -1;
    inst->exit_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    /* First positional arg is port */
    if (argc > 0)
        strncpy(inst->port, argv[0], sizeof(inst->port) - 1);
    else
        strncpy(inst->port, "/dev/ttyUSB0", sizeof(inst->port) - 1);

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

    hal->ready(hal->ctx, inst->hal_id);

    cmod_t *mod = calloc(1, sizeof(cmod_t));
    if (!mod) {
        hal->exit(hal->ctx, inst->hal_id);
        free(inst);
        return -1;
    }
    mod->Init    = pmx_Init;
    mod->Start   = pmx_Start;
    mod->Stop    = pmx_Stop;
    mod->Destroy = pmx_Destroy;
    mod->priv    = inst;
    *out = mod;
    return 0;
}
