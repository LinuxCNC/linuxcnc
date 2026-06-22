/*
   XHC-WHB04B-6 Wireless MPG pendant LinuxCNC HAL module

   Copyright (C) 2018 Raoul Rubien (github.com/rubienr)
   Updated for LinuxCNC 2020 by alkabal_free.fr
   Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port

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
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.

   Cmod port: hand-written cmod for XHC-WHB04B-6 wireless MPG pendant.
 */

#include "gomc_env.h"
#include "gomc_user.h"


#include <stdlib.h>
#include <sys/eventfd.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <libusb-1.0/libusb.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/time.h>

/* ========================================================================== */
/* Constants                                                                   */
/* ========================================================================== */

#define USB_VENDOR_ID   0x10ce
#define USB_PRODUCT_ID  0xeb93
#define COMP_NAME       "xhc-whb04b-6"
#define PIN_PREFIX      "whb"

/* Number of axes */
#define N_AXIS 6

/* Axis indices */
enum { AXIS_X = 0, AXIS_Y, AXIS_Z, AXIS_A, AXIS_B, AXIS_C };

/* Handwheel step modes */
enum { MODE_CON = 0, MODE_STEP = 1, MODE_MPG = 2 };

/* Counter indices (LEAD after the 6 axes, UNDEFINED = sentinel) */
enum { CNT_X = 0, CNT_Y, CNT_Z, CNT_A, CNT_B, CNT_C, CNT_LEAD, CNT_UNDEFINED };

/* Display step mode flags */
enum { DISP_CON = 0, DISP_STEP = 1, DISP_MPG = 2, DISP_PERCENT = 3 };

/* Max number of meta-buttons (including terminator) */
#define NB_META_BUTTONS 32

/* Mode request timing (ms) */
#define MODE_HOLD_MS        10
#define MODE_SPACE_MS       10
#define MODE_CHECK_LOOPS    60
#define MODE_CHECK_TIMEOUT  5

/* ========================================================================== */
/* Key codes — must match USB protocol                                        */
/* ========================================================================== */

/* Push buttons */
#define BTN_RESET         0x01
#define BTN_STOP          0x02
#define BTN_START         0x03
#define BTN_FEED_PLUS     0x04
#define BTN_FEED_MINUS    0x05
#define BTN_SPINDLE_PLUS  0x06
#define BTN_SPINDLE_MINUS 0x07
#define BTN_M_HOME        0x08
#define BTN_SAFE_Z        0x09
#define BTN_W_HOME        0x0a
#define BTN_S_ON_OFF      0x0b
#define BTN_FN            0x0c
#define BTN_PROBE_Z       0x0d
#define BTN_CONTINUOUS    0x0e
#define BTN_STEP          0x0f
#define BTN_MACRO10       0x10
#define BTN_UNDEFINED     0x00

/* Axis rotary button */
#define AXIS_OFF_CODE     0x06
#define AXIS_X_CODE       0x11
#define AXIS_Y_CODE       0x12
#define AXIS_Z_CODE       0x13
#define AXIS_A_CODE       0x14
#define AXIS_B_CODE       0x15
#define AXIS_C_CODE       0x16

/* Feed rotary button */
#define FEED_2_CODE       0x0d
#define FEED_5_CODE       0x0e
#define FEED_10_CODE      0x0f
#define FEED_30_CODE      0x10
#define FEED_60_CODE      0x1a
#define FEED_100_CODE     0x1b
#define FEED_LEAD_CODE    0x1c
#define FEED_LEAD_ALT     0x9b  /* alternate code for xhc-whb06-4 */

/* ========================================================================== */
/* Meta-button table: (key_code, modifier_code, pin_name)                     */
/* ========================================================================== */

typedef struct {
    uint8_t     key;
    uint8_t     modifier;
    const char *pin_name;
} meta_button_t;

static const meta_button_t meta_buttons[NB_META_BUTTONS] = {
    { BTN_RESET,         BTN_UNDEFINED, "reset" },
    { BTN_RESET,         BTN_FN,        "macro-11" },
    { BTN_STOP,          BTN_UNDEFINED, "stop" },
    { BTN_STOP,          BTN_FN,        "macro-12" },
    { BTN_START,         BTN_UNDEFINED, "start-pause" },
    { BTN_START,         BTN_FN,        "macro-13" },
    { BTN_FEED_PLUS,     BTN_UNDEFINED, "feed-plus" },
    { BTN_FEED_PLUS,     BTN_FN,        "macro-1" },
    { BTN_FEED_MINUS,    BTN_UNDEFINED, "feed-minus" },
    { BTN_FEED_MINUS,    BTN_FN,        "macro-2" },
    { BTN_SPINDLE_PLUS,  BTN_UNDEFINED, "spindle-plus" },
    { BTN_SPINDLE_PLUS,  BTN_FN,        "macro-3" },
    { BTN_SPINDLE_MINUS, BTN_UNDEFINED, "spindle-minus" },
    { BTN_SPINDLE_MINUS, BTN_FN,        "macro-4" },
    { BTN_M_HOME,        BTN_UNDEFINED, "m-home" },
    { BTN_M_HOME,        BTN_FN,        "macro-5" },
    { BTN_SAFE_Z,        BTN_UNDEFINED, "safe-z" },
    { BTN_SAFE_Z,        BTN_FN,        "macro-6" },
    { BTN_W_HOME,        BTN_UNDEFINED, "w-home" },
    { BTN_W_HOME,        BTN_FN,        "macro-7" },
    { BTN_S_ON_OFF,      BTN_UNDEFINED, "s-on-off" },
    { BTN_S_ON_OFF,      BTN_FN,        "macro-8" },
    { BTN_FN,            BTN_UNDEFINED, "fn" },
    { BTN_PROBE_Z,       BTN_UNDEFINED, "probe-z" },
    { BTN_PROBE_Z,       BTN_FN,        "macro-9" },
    { BTN_MACRO10,       BTN_UNDEFINED, "macro-10" },
    { BTN_MACRO10,       BTN_FN,        "macro-14" },
    { BTN_CONTINUOUS,    BTN_UNDEFINED, "mode-continuous" },
    { BTN_CONTINUOUS,    BTN_FN,        "macro-15" },
    { BTN_STEP,          BTN_UNDEFINED, "mode-step" },
    { BTN_STEP,          BTN_FN,        "macro-16" },
    { BTN_UNDEFINED,     BTN_UNDEFINED, NULL },  /* terminator */
};

/* ========================================================================== */
/* Step size tables                                                            */
/* ========================================================================== */

/* Feed rotary position index (matches USB code order) */
enum {
    FPOS_2 = 0, FPOS_5, FPOS_10, FPOS_30, FPOS_60, FPOS_100, FPOS_LEAD, FPOS_UNDEF,
    FPOS_COUNT = 8
};

static const float step_sizes[FPOS_COUNT]  = { 0.001f, 0.01f, 0.1f, 1.0f, 5.0f, 10.0f, 0.0f, 0.0f };
static const float mpg_sizes[FPOS_COUNT]   = { 2.0f, 5.0f, 10.0f, 30.0f, 60.0f, 100.0f, 0.0f, 0.0f };
static const float con_sizes[FPOS_COUNT]   = { 2.0f, 5.0f, 10.0f, 30.0f, 60.0f, 100.0f, 0.0f, 0.0f };
/* Map feed USB code to position index */
static int feed_code_to_pos(uint8_t code)
{
    switch (code) {
    case FEED_2_CODE:   return FPOS_2;
    case FEED_5_CODE:   return FPOS_5;
    case FEED_10_CODE:  return FPOS_10;
    case FEED_30_CODE:  return FPOS_30;
    case FEED_60_CODE:  return FPOS_60;
    case FEED_100_CODE: return FPOS_100;
    case FEED_LEAD_CODE:
    case FEED_LEAD_ALT: return FPOS_LEAD;
    default:            return FPOS_UNDEF;
    }
}

/* Map axis USB code to axis index, -1 for off/undefined */
static int axis_code_to_idx(uint8_t code)
{
    switch (code) {
    case AXIS_X_CODE: return AXIS_X;
    case AXIS_Y_CODE: return AXIS_Y;
    case AXIS_Z_CODE: return AXIS_Z;
    case AXIS_A_CODE: return AXIS_A;
    case AXIS_B_CODE: return AXIS_B;
    case AXIS_C_CODE: return AXIS_C;
    default:          return -1;
    }
}

/* ========================================================================== */
/* USB packet structures                                                       */
/* ========================================================================== */

typedef struct {
    uint8_t header;
    uint8_t random_byte;
    uint8_t button1;
    uint8_t button2;
    uint8_t feed_code;
    uint8_t axis_code;
    int8_t  step_count;
    uint8_t crc;
} __attribute__((packed)) usb_in_packet_t;

typedef struct {
    uint16_t integer;
    uint16_t fraction :15;
    uint16_t sign     :1;
} __attribute__((packed)) display_coord_t;

typedef struct {
    uint16_t       header;       /* 0xfdfe */
    uint8_t        seed;         /* 0xfe */
    uint8_t        flags;        /* display mode flags */
    display_coord_t row1;
    display_coord_t row2;
    display_coord_t row3;
    uint16_t       feed_rate;
    uint16_t       spindle_rate;
    uint8_t        _pad;
} __attribute__((packed)) usb_out_data_t;

/* Display flag bits */
#define DISP_FLAG_STEP_MASK  0x03
#define DISP_FLAG_RESET      0x40
#define DISP_FLAG_RELATIVE   0x80

/* ========================================================================== */
/* HAL pins structure                                                          */
/* ========================================================================== */

typedef struct {
    /* Button output pins (one per meta-button) */
    gomc_hal_bit_t  *button_pin[NB_META_BUTTONS];

    /* Flood/mist/lube */
    gomc_hal_bit_t  *flood_is_on;      /* IN */
    gomc_hal_bit_t  *flood_on;         /* OUT */
    gomc_hal_bit_t  *flood_off;        /* OUT */
    gomc_hal_bit_t  *mist_is_on;       /* IN */
    gomc_hal_bit_t  *mist_on;          /* OUT */
    gomc_hal_bit_t  *mist_off;         /* OUT */
    gomc_hal_bit_t  *lube_is_on;       /* IN */
    gomc_hal_bit_t  *lube_on;          /* OUT */
    gomc_hal_bit_t  *lube_off;         /* OUT */

    /* Per-axis pins */
    gomc_hal_s32_t  *jog_counts[N_AXIS];     /* OUT */
    gomc_hal_bit_t  *jog_enable[N_AXIS];     /* OUT */
    gomc_hal_float_t *jog_scale[N_AXIS];     /* OUT */
    gomc_hal_bit_t  *jog_vel_mode[N_AXIS];   /* OUT */
    gomc_hal_bit_t  *axis_select[N_AXIS];    /* OUT */

    /* Axis position feedback */
    gomc_hal_float_t *axis_pos[N_AXIS];          /* IN */
    gomc_hal_float_t *axis_pos_rel[N_AXIS];      /* IN */

    /* Joint homed */
    gomc_hal_bit_t  *joint_homed[N_AXIS];    /* IN */

    /* Feed */
    gomc_hal_bit_t  *feed_sel_2;       /* OUT */
    gomc_hal_bit_t  *feed_sel_5;       /* OUT */
    gomc_hal_bit_t  *feed_sel_10;      /* OUT */
    gomc_hal_bit_t  *feed_sel_30;      /* OUT */
    gomc_hal_bit_t  *feed_sel_60;      /* OUT */
    gomc_hal_bit_t  *feed_sel_100;     /* OUT */
    gomc_hal_bit_t  *feed_sel_lead;    /* OUT */
    gomc_hal_bit_t  *feed_sel_mpg;     /* OUT */
    gomc_hal_bit_t  *feed_sel_con;     /* OUT */
    gomc_hal_bit_t  *feed_sel_step;    /* OUT */
    gomc_hal_float_t *feed_ovr_scale;  /* OUT */
    gomc_hal_bit_t  *feed_ovr_dec;     /* OUT */
    gomc_hal_bit_t  *feed_ovr_inc;     /* OUT */
    gomc_hal_float_t *feed_ovr_max_vel;/* IN */
    gomc_hal_float_t *feed_ovr_value;  /* IN */

    /* Spindle */
    gomc_hal_float_t *spindle_ovr_scale; /* OUT */
    gomc_hal_bit_t  *spindle_ovr_dec;    /* OUT */
    gomc_hal_bit_t  *spindle_ovr_inc;    /* OUT */
    gomc_hal_bit_t  *spindle_start;      /* OUT */
    gomc_hal_bit_t  *spindle_stop;       /* OUT */
    gomc_hal_bit_t  *spindle_fwd;        /* OUT */
    gomc_hal_bit_t  *spindle_rev;        /* OUT */
    gomc_hal_bit_t  *spindle_inc;        /* OUT */
    gomc_hal_bit_t  *spindle_dec;        /* OUT */
    gomc_hal_bit_t  *spindle_is_on;      /* IN */
    gomc_hal_float_t *spindle_ovr_value; /* IN */
    gomc_hal_float_t *spindle_speed_cmd; /* IN */

    /* Machine */
    gomc_hal_bit_t  *machine_is_on;      /* IN */
    gomc_hal_bit_t  *machine_on;         /* OUT */
    gomc_hal_bit_t  *machine_off;        /* OUT */

    /* Program */
    gomc_hal_bit_t  *prog_is_idle;       /* IN */
    gomc_hal_bit_t  *prog_is_paused;     /* IN */
    gomc_hal_bit_t  *prog_is_running;    /* IN */
    gomc_hal_bit_t  *prog_run;           /* OUT */
    gomc_hal_bit_t  *prog_pause;         /* OUT */
    gomc_hal_bit_t  *prog_resume;        /* OUT */
    gomc_hal_bit_t  *prog_stop;          /* OUT */

    /* Mode */
    gomc_hal_bit_t  *mode_is_auto;       /* IN */
    gomc_hal_bit_t  *mode_is_joint;      /* IN */
    gomc_hal_bit_t  *mode_is_manual;     /* IN */
    gomc_hal_bit_t  *mode_is_mdi;        /* IN */
    gomc_hal_bit_t  *mode_is_teleop;     /* IN */
    gomc_hal_bit_t  *mode_auto;          /* OUT */
    gomc_hal_bit_t  *mode_joint;         /* OUT */
    gomc_hal_bit_t  *mode_manual;        /* OUT */
    gomc_hal_bit_t  *mode_mdi;           /* OUT */
    gomc_hal_bit_t  *mode_teleop;        /* OUT */

    /* Pendant state */
    gomc_hal_bit_t  *pendant_sleeping;   /* OUT */
    gomc_hal_bit_t  *pendant_connected;  /* OUT */
} hal_pins_t;

/* ========================================================================== */
/* Instance structure                                                          */
/* ========================================================================== */

typedef struct {
    /* cmod env */
    const cmod_env_t *env;
    const gomc_log_t *log;
    int               exit_fd;

    /* HAL */
    int               hal_id;
    hal_pins_t       *pins;

    /* USB */
    libusb_context       *usb_ctx;
    libusb_device_handle *usb_dev;
    struct libusb_transfer *in_transfer;
    uint8_t               in_buf[8];

    /* State */
    int32_t  counters[CNT_UNDEFINED + 1]; /* per-axis + lead + unused sentinel */
    int      active_counter;    /* CNT_X..CNT_C or CNT_UNDEFINED */
    bool     lead_active;       /* lead counter override */
    int      step_mode;         /* MODE_CON / MODE_STEP / MODE_MPG */
    int      feed_pos;          /* FPOS_2..FPOS_UNDEF */
    int      active_axis;       /* AXIS_X..AXIS_C or -1 */
    bool     spindle_fwd;       /* spindle direction */
    int      cur_meta;          /* index into meta_buttons[], or -1 */

    /* Display */
    usb_out_data_t  display;
    bool            disp_absolute;  /* absolute vs relative */
    int             disp_group;     /* 0=XYZ, 1=ABC */

    /* Options */
    bool     lead_mode_spindle;
    bool     lead_mode_feed;
    bool     step_5_10;
    int      wait_timeout;

    /* Thread */
    pthread_t usb_thread;
    volatile bool running;
} whb_inst_t;

/* ========================================================================== */
/* Forward declarations                                                        */
/* ========================================================================== */

static void process_input(whb_inst_t *inst, const usb_in_packet_t *pkt);
static void update_display(whb_inst_t *inst);
static void send_display(whb_inst_t *inst);

/* ========================================================================== */
/* HAL helpers                                                                 */
/* ========================================================================== */

#define NEW_BIT_PIN(dir, ptr, fmt, ...) \
    do { if (gomc_hal_pin_bit_newf(hal, dir, ptr, comp_id, fmt, ##__VA_ARGS__) < 0) return -1; } while(0)
#define NEW_S32_PIN(dir, ptr, fmt, ...) \
    do { if (gomc_hal_pin_s32_newf(hal, dir, ptr, comp_id, fmt, ##__VA_ARGS__) < 0) return -1; } while(0)
#define NEW_FLOAT_PIN(dir, ptr, fmt, ...) \
    do { if (gomc_hal_pin_float_newf(hal, dir, ptr, comp_id, fmt, ##__VA_ARGS__) < 0) return -1; } while(0)

static int create_hal_pins(whb_inst_t *inst)
{
    const gomc_hal_t *hal = inst->env->hal;
    int comp_id = inst->hal_id;
    hal_pins_t *p = inst->pins;
    static const char *axis_name[] = { "x", "y", "z", "a", "b", "c" };

    /* Button pins */
    for (int i = 0; meta_buttons[i].pin_name != NULL; i++) {
        NEW_BIT_PIN(GOMC_HAL_OUT, &p->button_pin[i], "%s.button.%s", PIN_PREFIX, meta_buttons[i].pin_name);
    }

    /* Flood/mist/lube */
    NEW_BIT_PIN(GOMC_HAL_IN,  &p->flood_is_on, "%s.halui.flood.is-on", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->flood_off,   "%s.halui.flood.off", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->flood_on,    "%s.halui.flood.on", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_IN,  &p->mist_is_on,  "%s.halui.mist.is-on", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->mist_off,    "%s.halui.mist.off", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->mist_on,     "%s.halui.mist.on", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_IN,  &p->lube_is_on,  "%s.halui.lube.is-on", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->lube_off,    "%s.halui.lube.off", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->lube_on,     "%s.halui.lube.on", PIN_PREFIX);

    /* Per-axis pins */
    for (int i = 0; i < N_AXIS; i++) {
        NEW_S32_PIN(GOMC_HAL_OUT,   &p->jog_counts[i],  "%s.axis.%s.jog-counts", PIN_PREFIX, axis_name[i]);
        NEW_BIT_PIN(GOMC_HAL_OUT,   &p->jog_enable[i],  "%s.axis.%s.jog-enable", PIN_PREFIX, axis_name[i]);
        NEW_FLOAT_PIN(GOMC_HAL_OUT, &p->jog_scale[i],   "%s.axis.%s.jog-scale", PIN_PREFIX, axis_name[i]);
        NEW_BIT_PIN(GOMC_HAL_OUT,   &p->jog_vel_mode[i],"%s.axis.%s.jog-vel-mode", PIN_PREFIX, axis_name[i]);
    }

    /* Pendant state */
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->pendant_sleeping,  "%s.pendant.is-sleeping", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->pendant_connected, "%s.pendant.is-connected", PIN_PREFIX);

    /* Axis position feedback */
    for (int i = 0; i < N_AXIS; i++) {
        NEW_FLOAT_PIN(GOMC_HAL_IN, &p->axis_pos[i],     "%s.halui.axis.%s.pos-feedback", PIN_PREFIX, axis_name[i]);
        NEW_FLOAT_PIN(GOMC_HAL_IN, &p->axis_pos_rel[i], "%s.halui.axis.%s.pos-relative", PIN_PREFIX, axis_name[i]);
    }

    /* Feed selection */
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->feed_sel_2,    "%s.halui.feed.selected-2", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->feed_sel_5,    "%s.halui.feed.selected-5", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->feed_sel_10,   "%s.halui.feed.selected-10", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->feed_sel_30,   "%s.halui.feed.selected-30", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->feed_sel_60,   "%s.halui.feed.selected-60", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->feed_sel_100,  "%s.halui.feed.selected-100", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->feed_sel_lead, "%s.halui.feed.selected-lead", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->feed_sel_mpg,  "%s.halui.feed.selected-mpg-feed", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->feed_sel_con,  "%s.halui.feed.selected-continuous", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->feed_sel_step, "%s.halui.feed.selected-step", PIN_PREFIX);

    /* Feed override */
    NEW_FLOAT_PIN(GOMC_HAL_OUT, &p->feed_ovr_scale, "%s.halui.feed-override.scale", PIN_PREFIX);
    NEW_FLOAT_PIN(GOMC_HAL_IN,  &p->feed_ovr_max_vel, "%s.halui.max-velocity.value", PIN_PREFIX);
    NEW_FLOAT_PIN(GOMC_HAL_IN,  &p->feed_ovr_value,   "%s.halui.feed-override.value", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT,   &p->feed_ovr_dec,     "%s.halui.feed-override.decrease", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT,   &p->feed_ovr_inc,     "%s.halui.feed-override.increase", PIN_PREFIX);

    /* Spindle */
    NEW_FLOAT_PIN(GOMC_HAL_IN,  &p->spindle_speed_cmd, "%s.halui.spindle-speed-cmd", PIN_PREFIX);
    NEW_FLOAT_PIN(GOMC_HAL_IN,  &p->spindle_ovr_value, "%s.halui.spindle-override.value", PIN_PREFIX);
    NEW_FLOAT_PIN(GOMC_HAL_OUT, &p->spindle_ovr_scale, "%s.halui.spindle-override.scale", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT,   &p->spindle_inc,       "%s.halui.spindle.increase", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT,   &p->spindle_dec,       "%s.halui.spindle.decrease", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT,   &p->spindle_ovr_inc,   "%s.halui.spindle-override.increase", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT,   &p->spindle_ovr_dec,   "%s.halui.spindle-override.decrease", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT,   &p->spindle_start,     "%s.halui.spindle.start", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_IN,    &p->spindle_is_on,     "%s.halui.spindle.is-on", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT,   &p->spindle_stop,      "%s.halui.spindle.stop", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT,   &p->spindle_fwd,       "%s.halui.spindle.forward", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT,   &p->spindle_rev,       "%s.halui.spindle.reverse", PIN_PREFIX);

    /* Machine */
    NEW_BIT_PIN(GOMC_HAL_IN,  &p->machine_is_on, "%s.halui.machine.is-on", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->machine_on,    "%s.halui.machine.on", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->machine_off,   "%s.halui.machine.off", PIN_PREFIX);

    /* Program */
    NEW_BIT_PIN(GOMC_HAL_IN,  &p->prog_is_idle,    "%s.halui.program.is-idle", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_IN,  &p->prog_is_paused,  "%s.halui.program.is-paused", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_IN,  &p->prog_is_running, "%s.halui.program.is-running", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->prog_resume,     "%s.halui.program.resume", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->prog_pause,      "%s.halui.program.pause", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->prog_run,        "%s.halui.program.run", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->prog_stop,       "%s.halui.program.stop", PIN_PREFIX);

    /* Mode */
    NEW_BIT_PIN(GOMC_HAL_IN,  &p->mode_is_auto,    "%s.halui.mode.is-auto", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_IN,  &p->mode_is_joint,   "%s.halui.mode.is-joint", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_IN,  &p->mode_is_manual,  "%s.halui.mode.is-manual", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_IN,  &p->mode_is_mdi,     "%s.halui.mode.is-mdi", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_IN,  &p->mode_is_teleop,  "%s.halui.mode.is-teleop", PIN_PREFIX);

    /* Joint homed */
    for (int i = 0; i < N_AXIS; i++) {
        NEW_BIT_PIN(GOMC_HAL_IN, &p->joint_homed[i], "%s.halui.joint.%s.is-homed", PIN_PREFIX, axis_name[i]);
    }

    NEW_BIT_PIN(GOMC_HAL_OUT, &p->mode_auto,       "%s.halui.mode.auto", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->mode_joint,      "%s.halui.mode.joint", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->mode_manual,     "%s.halui.mode.manual", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->mode_mdi,        "%s.halui.mode.mdi", PIN_PREFIX);
    NEW_BIT_PIN(GOMC_HAL_OUT, &p->mode_teleop,     "%s.halui.mode.teleop", PIN_PREFIX);

    /* Axis select */
    for (int i = 0; i < N_AXIS; i++) {
        NEW_BIT_PIN(GOMC_HAL_OUT, &p->axis_select[i], "%s.halui.axis.%s.select", PIN_PREFIX, axis_name[i]);
    }

    return 0;
}

/* ========================================================================== */
/* Mode request helpers                                                        */
/* ========================================================================== */

static bool wait_for_mode(volatile gomc_hal_bit_t *condition)
{
    for (int i = 0; i < MODE_CHECK_LOOPS; i++) {
        if (*condition)
            return true;
        usleep(MODE_CHECK_TIMEOUT * 1000);
    }
    return false;
}

static bool request_mode(whb_inst_t *inst, bool rising, gomc_hal_bit_t *req_pin, gomc_hal_bit_t *fb_pin)
{
    if (rising) {
        if (*fb_pin)
            return true;
        *req_pin = 1;
        usleep(MODE_HOLD_MS * 1000);
        bool ok = wait_for_mode(fb_pin);
        *req_pin = 0;
        usleep(MODE_SPACE_MS * 1000);
        return ok;
    } else {
        *req_pin = 0;
        return false;
    }
}

static bool request_manual(whb_inst_t *inst, bool rising)
{
    return request_mode(inst, rising, inst->pins->mode_manual, inst->pins->mode_is_manual);
}

static bool request_auto(whb_inst_t *inst, bool rising)
{
    return request_mode(inst, rising, inst->pins->mode_auto, inst->pins->mode_is_auto);
}

static bool request_mdi(whb_inst_t *inst, bool rising)
{
    return request_mode(inst, rising, inst->pins->mode_mdi, inst->pins->mode_is_mdi);
}

static bool request_teleop(whb_inst_t *inst, bool rising)
{
    return request_mode(inst, rising, inst->pins->mode_teleop, inst->pins->mode_is_teleop);
}

static bool request_joint(whb_inst_t *inst, bool rising)
{
    return request_mode(inst, rising, inst->pins->mode_joint, inst->pins->mode_is_joint);
}

/* ========================================================================== */
/* HAL action helpers                                                           */
/* ========================================================================== */

static void clear_start_resume(whb_inst_t *inst)
{
    hal_pins_t *p = inst->pins;
    *p->mode_teleop = 0;
    *p->mode_joint  = 0;
    *p->mode_auto   = 0;
    *p->prog_pause  = 0;
    *p->prog_run    = 0;
    *p->prog_resume = 0;
}

static void check_state(bool state, volatile gomc_hal_bit_t *pin)
{
    for (int i = 0; i < 500; i++) {
        if (*pin != state)
            break;
        usleep(1000);
    }
}

static void toggle_start_resume(whb_inst_t *inst)
{
    hal_pins_t *p = inst->pins;
    if (*p->prog_is_paused) {
        *p->prog_pause  = 0;
        *p->prog_run    = 0;
        *p->prog_resume = 1;
        check_state(true, p->prog_is_paused);
        *p->prog_resume = 0;
    } else if (*p->prog_is_running) {
        *p->prog_pause = 1;
        check_state(false, p->prog_is_paused);
        *p->prog_pause  = 0;
        *p->prog_run    = 0;
        *p->prog_resume = 0;
    } else if (*p->prog_is_idle) {
        *p->prog_pause  = 0;
        *p->prog_run    = 1;
        check_state(false, p->prog_is_running);
        *p->prog_run    = 0;
        *p->prog_resume = 0;
    }
}

static void set_pin(whb_inst_t *inst, int meta_idx, bool enabled)
{
    if (meta_idx >= 0 && meta_idx < NB_META_BUTTONS && inst->pins->button_pin[meta_idx])
        *inst->pins->button_pin[meta_idx] = enabled;
}

/* Find meta-button index by key+modifier pair */
static int find_meta(uint8_t key, uint8_t modifier)
{
    for (int i = 0; meta_buttons[i].pin_name != NULL; i++) {
        if (meta_buttons[i].key == key && meta_buttons[i].modifier == modifier)
            return i;
    }
    return -1;
}

/* ========================================================================== */
/* Step size / jog scale                                                        */
/* ========================================================================== */

static float get_step_size(whb_inst_t *inst)
{
    int pos = inst->feed_pos;
    if (pos < 0 || pos >= FPOS_COUNT)
        return 0.0f;

    switch (inst->step_mode) {
    case MODE_STEP: return step_sizes[pos];
    case MODE_MPG:  return mpg_sizes[pos];
    case MODE_CON:  return con_sizes[pos];
    default:        return 0.0f;
    }
}

static bool feed_is_permitted(whb_inst_t *inst)
{
    int pos = inst->feed_pos;
    if (pos < 0 || pos >= FPOS_COUNT)
        return false;
    if (pos == FPOS_LEAD)
        return true; /* lead always permitted */
    return (get_step_size(inst) > 0.0f);
}

/* ========================================================================== */
/* Set velocity mode on all axes                                               */
/* ========================================================================== */

static void set_all_vel_mode(whb_inst_t *inst, bool vel_mode)
{
    for (int i = 0; i < N_AXIS; i++)
        *inst->pins->jog_vel_mode[i] = vel_mode;
}

/* ========================================================================== */
/* Dispatch feed value to HAL                                                  */
/* ========================================================================== */

static void dispatch_feed_value(whb_inst_t *inst)
{
    hal_pins_t *p = inst->pins;

    if (!feed_is_permitted(inst)) {
        for (int i = 0; i < N_AXIS; i++)
            *p->jog_scale[i] = 0.0;
        return;
    }

    float scale = 0.0f;
    if (inst->step_mode == MODE_STEP) {
        set_all_vel_mode(inst, false);
        *p->feed_sel_step = 1;
        *p->feed_sel_mpg  = 0;
        *p->feed_sel_con  = 0;
        *p->feed_ovr_scale = 0;
        scale = get_step_size(inst);
    } else if (inst->step_mode == MODE_MPG) {
        set_all_vel_mode(inst, false);
        *p->feed_sel_step = 0;
        *p->feed_sel_mpg  = 1;
        *p->feed_sel_con  = 0;
        *p->feed_ovr_scale = 0;
        scale = 0.0f; /* no jog in MPG - used for feed override */
    } else if (inst->step_mode == MODE_CON) {
        set_all_vel_mode(inst, true);
        *p->feed_sel_step = 0;
        *p->feed_sel_mpg  = 0;
        *p->feed_sel_con  = 1;
        *p->feed_ovr_scale = 0;
        scale = get_step_size(inst) * 0.0001f * (float)*p->feed_ovr_max_vel;
    }

    for (int i = 0; i < N_AXIS; i++)
        *p->jog_scale[i] = scale;
}

/* ========================================================================== */
/* Dispatch feed selection pins                                                 */
/* ========================================================================== */

static void dispatch_feed_sel(whb_inst_t *inst, int pos, bool active)
{
    hal_pins_t *p = inst->pins;
    switch (pos) {
    case FPOS_2:    *p->feed_sel_2    = active; break;
    case FPOS_5:    *p->feed_sel_5    = active; break;
    case FPOS_10:   *p->feed_sel_10   = active; break;
    case FPOS_30:   *p->feed_sel_30   = active; break;
    case FPOS_60:   *p->feed_sel_60   = active; break;
    case FPOS_100:  *p->feed_sel_100  = active; break;
    case FPOS_LEAD: *p->feed_sel_lead = active; break;
    default: break;
    }
}

/* ========================================================================== */
/* Axis active/inactive dispatch                                               */
/* ========================================================================== */

static void set_axis_active(whb_inst_t *inst, int axis, bool active)
{
    if (axis < 0 || axis >= N_AXIS)
        return;
    *inst->pins->axis_select[axis] = active;
    *inst->pins->jog_enable[axis]  = active;
}

/* ========================================================================== */
/* Jog counts dispatch                                                         */
/* ========================================================================== */

static void dispatch_jog_counts(whb_inst_t *inst)
{
    hal_pins_t *p = inst->pins;

    /* Request teleop if selected axis is not homed, manual otherwise */
    int sel = inst->active_axis;
    if (sel >= 0 && sel < N_AXIS && !*p->joint_homed[sel]) {
        request_teleop(inst, true);
    }
    request_manual(inst, true);

    for (int i = 0; i < N_AXIS; i++)
        *p->jog_counts[i] = inst->counters[i];

    request_manual(inst, false);
    request_teleop(inst, false);
}

/* ========================================================================== */
/* Button press/release handlers                                               */
/* ========================================================================== */

static void on_button_pressed(whb_inst_t *inst, int meta_idx)
{
    hal_pins_t *p = inst->pins;
    uint8_t key = meta_buttons[meta_idx].key;
    uint8_t mod = meta_buttons[meta_idx].modifier;

    set_pin(inst, meta_idx, true);

    /* Direct button actions (no Fn modifier) */
    if (mod == BTN_UNDEFINED) {
        switch (key) {
        case BTN_RESET:
            clear_start_resume(inst);
            if (*p->machine_is_on)
                *p->machine_off = 1;
            else
                *p->machine_on = 1;
            break;
        case BTN_STOP:
            clear_start_resume(inst);
            *p->prog_stop = 1;
            break;
        case BTN_START:
            if (request_auto(inst, true))
                toggle_start_resume(inst);
            break;
        case BTN_FEED_PLUS:
            *p->feed_ovr_scale = 0.05;
            *p->feed_ovr_inc = 1;
            break;
        case BTN_FEED_MINUS:
            *p->feed_ovr_scale = 0.05;
            *p->feed_ovr_dec = 1;
            break;
        case BTN_SPINDLE_PLUS:
            *p->spindle_ovr_scale = 0.05;
            *p->spindle_ovr_inc = 1;
            break;
        case BTN_SPINDLE_MINUS:
            *p->spindle_ovr_scale = 0.05;
            *p->spindle_ovr_dec = 1;
            break;
        case BTN_FN:
            /* fn alone — just set pin */
            break;
        case BTN_M_HOME:
            if (request_joint(inst, true))
                ; /* pin already set */
            break;
        case BTN_SAFE_Z:
            request_mdi(inst, true);
            break;
        case BTN_W_HOME:
            request_mdi(inst, true);
            break;
        case BTN_S_ON_OFF:
            if (*p->spindle_is_on) {
                *p->spindle_stop = 1;
            } else {
                if (inst->spindle_fwd) {
                    *p->spindle_fwd = 1;
                    *p->spindle_inc = 1;
                } else {
                    *p->spindle_rev = 1;
                    *p->spindle_inc = 1;
                }
                *p->spindle_start = 1;
            }
            break;
        case BTN_PROBE_Z:
            request_mdi(inst, true);
            break;
        case BTN_MACRO10:
            /* Toggle absolute/relative display */
            inst->disp_absolute = !inst->disp_absolute;
            break;
        case BTN_CONTINUOUS:
            inst->step_mode = MODE_CON;
            set_all_vel_mode(inst, true);
            *p->feed_sel_con = 1;
            *p->feed_sel_mpg = 0;
            *p->feed_sel_step = 0;
            dispatch_feed_value(inst);
            break;
        case BTN_STEP:
            inst->step_mode = MODE_STEP;
            set_all_vel_mode(inst, false);
            *p->feed_sel_step = 1;
            *p->feed_sel_mpg  = 0;
            *p->feed_sel_con  = 0;
            dispatch_feed_value(inst);
            break;
        }
    }
    /* Fn-modified button actions */
    else if (mod == BTN_FN) {
        switch (key) {
        case BTN_SPINDLE_PLUS:  /* macro-3: spindle speed increase */
            if (*p->spindle_is_on)
                *p->spindle_inc = 1;
            break;
        case BTN_SPINDLE_MINUS: /* macro-4: spindle speed decrease */
            if (*p->spindle_is_on)
                *p->spindle_dec = 1;
            break;
        case BTN_S_ON_OFF:      /* macro-8: toggle spindle direction */
            inst->spindle_fwd = !inst->spindle_fwd;
            if (*p->spindle_is_on) {
                if (inst->spindle_fwd) {
                    *p->spindle_fwd = 1;
                    *p->spindle_inc = 1;
                } else {
                    *p->spindle_rev = 1;
                    *p->spindle_inc = 1;
                }
            }
            break;
        case BTN_FEED_MINUS:    /* macro-2: toggle lube */
            if (*p->lube_is_on)
                *p->lube_off = 1;
            else
                *p->lube_on = 1;
            break;
        case BTN_CONTINUOUS:    /* macro-15: toggle flood */
            if (*p->flood_is_on)
                *p->flood_off = 1;
            else
                *p->flood_on = 1;
            break;
        case BTN_STEP:          /* macro-16: toggle mist */
            if (*p->mist_is_on)
                *p->mist_off = 1;
            else
                *p->mist_on = 1;
            break;
        default:
            /* Other macros: just pin set (already done above) */
            break;
        }
    }
}

static void on_button_released(whb_inst_t *inst, int meta_idx)
{
    hal_pins_t *p = inst->pins;
    uint8_t key = meta_buttons[meta_idx].key;
    uint8_t mod = meta_buttons[meta_idx].modifier;

    set_pin(inst, meta_idx, false);

    if (mod == BTN_UNDEFINED) {
        switch (key) {
        case BTN_RESET:
            *p->machine_off = 0;
            *p->machine_on  = 0;
            break;
        case BTN_STOP:
            *p->prog_stop = 0;
            break;
        case BTN_START:
            request_auto(inst, false);
            break;
        case BTN_FEED_PLUS:
            *p->feed_ovr_inc = 0;
            break;
        case BTN_FEED_MINUS:
            *p->feed_ovr_dec = 0;
            break;
        case BTN_SPINDLE_PLUS:
            *p->spindle_ovr_inc = 0;
            break;
        case BTN_SPINDLE_MINUS:
            *p->spindle_ovr_dec = 0;
            break;
        case BTN_M_HOME:
            request_joint(inst, false);
            break;
        case BTN_SAFE_Z:
            request_mdi(inst, false);
            break;
        case BTN_W_HOME:
            request_mdi(inst, false);
            break;
        case BTN_S_ON_OFF:
            *p->spindle_stop  = 0;
            *p->spindle_fwd   = 0;
            *p->spindle_rev   = 0;
            *p->spindle_inc   = 0;
            *p->spindle_start = 0;
            break;
        case BTN_PROBE_Z:
            request_mdi(inst, false);
            break;
        default:
            break;
        }
    } else if (mod == BTN_FN) {
        switch (key) {
        case BTN_SPINDLE_PLUS:
            *p->spindle_inc = 0;
            break;
        case BTN_SPINDLE_MINUS:
            *p->spindle_dec = 0;
            break;
        case BTN_S_ON_OFF:
            *p->spindle_fwd = 0;
            *p->spindle_rev = 0;
            *p->spindle_inc = 0;
            break;
        case BTN_FEED_MINUS:
            *p->lube_off = 0;
            *p->lube_on  = 0;
            break;
        case BTN_CONTINUOUS:
            *p->flood_off = 0;
            *p->flood_on  = 0;
            break;
        case BTN_STEP:
            *p->mist_off = 0;
            *p->mist_on  = 0;
            break;
        default:
            break;
        }
    }
}

/* ========================================================================== */
/* Jog dial event                                                              */
/* ========================================================================== */

static void on_jog_dial(whb_inst_t *inst, int8_t delta)
{
    if (!*inst->pins->machine_is_on)
        return;

    int idx = inst->lead_active ? CNT_LEAD : inst->active_counter;
    if (idx == CNT_UNDEFINED)
        return;

    inst->counters[idx] += delta;

    if (delta == 0)
        return;

    if (inst->lead_active && inst->lead_mode_spindle) {
        /* Spindle override via jog wheel in Lead mode */
        if (delta > 0) {
            *inst->pins->spindle_ovr_inc = !*inst->pins->spindle_ovr_inc;
            if (*inst->pins->spindle_ovr_inc)
                *inst->pins->spindle_ovr_scale = 0.01;
        } else {
            *inst->pins->spindle_ovr_dec = !*inst->pins->spindle_ovr_dec;
            if (*inst->pins->spindle_ovr_dec)
                *inst->pins->spindle_ovr_scale = 0.01;
        }
    } else if (!inst->lead_active && inst->lead_mode_feed && inst->step_mode == MODE_MPG) {
        /* Feed override via jog wheel in MPG+lead_mode_feed */
        if (delta > 0) {
            *inst->pins->feed_ovr_inc = !*inst->pins->feed_ovr_inc;
            if (*inst->pins->feed_ovr_inc)
                *inst->pins->feed_ovr_scale = 0.01;
        } else {
            *inst->pins->feed_ovr_dec = !*inst->pins->feed_ovr_dec;
            if (*inst->pins->feed_ovr_dec)
                *inst->pins->feed_ovr_scale = 0.01;
        }
    } else if (!inst->lead_active &&
               (inst->step_mode == MODE_CON || inst->step_mode == MODE_STEP)) {
        /* Normal jog mode */
        dispatch_jog_counts(inst);
    }
}

/* ========================================================================== */
/* Process USB input packet                                                    */
/* ========================================================================== */

static void process_input(whb_inst_t *inst, const usb_in_packet_t *pkt)
{
    uint8_t key_code = pkt->button1;
    uint8_t mod_code = pkt->button2;

    /* Key/modifier normalization (same logic as original) */
    if (key_code == BTN_UNDEFINED) {
        key_code = mod_code;
        mod_code = BTN_UNDEFINED;
    }
    if (key_code == BTN_FN && mod_code != BTN_UNDEFINED) {
        key_code = mod_code;
        mod_code = BTN_FN;
    }
    if (key_code != BTN_UNDEFINED && mod_code != BTN_UNDEFINED && mod_code != BTN_FN) {
        key_code = mod_code;
        mod_code = BTN_UNDEFINED;
    }

    /* Button state change detection */
    int new_meta = find_meta(key_code, mod_code);

    if (new_meta != inst->cur_meta) {
        /* Release previous */
        if (inst->cur_meta >= 0)
            on_button_released(inst, inst->cur_meta);
        /* Press new */
        inst->cur_meta = new_meta;
        if (new_meta >= 0)
            on_button_pressed(inst, new_meta);
    }

    /* Axis rotary button change */
    int new_axis = axis_code_to_idx(pkt->axis_code);
    if (new_axis != inst->active_axis) {
        /* Deactivate old */
        if (inst->active_axis >= 0)
            set_axis_active(inst, inst->active_axis, false);
        /* Update handwheel counter */
        inst->active_counter = (new_axis >= 0) ? new_axis : CNT_UNDEFINED;
        /* Activate new */
        inst->active_axis = new_axis;
        if (new_axis >= 0)
            set_axis_active(inst, new_axis, true);
        /* Update display axis group */
        if (new_axis >= 0 && new_axis <= AXIS_Z)
            inst->disp_group = 0; /* XYZ */
        else if (new_axis >= AXIS_A)
            inst->disp_group = 1; /* ABC */
    }

    /* Feed rotary button change */
    int new_feed = feed_code_to_pos(pkt->feed_code);
    if (new_feed != inst->feed_pos) {
        /* Deactivate old */
        dispatch_feed_sel(inst, inst->feed_pos, false);
        /* Check lead counter */
        bool was_lead = (inst->feed_pos == FPOS_LEAD);
        bool is_lead  = (new_feed == FPOS_LEAD);
        if (was_lead && !is_lead)
            inst->lead_active = false;
        if (is_lead && !was_lead) {
            inst->lead_active = true;
            inst->step_mode = MODE_MPG; /* lead forces MPG */
        }

        inst->feed_pos = new_feed;

        /* Activate new */
        dispatch_feed_sel(inst, new_feed, true);
        dispatch_feed_value(inst);

        /* Update display step mode indicator */
        switch (inst->step_mode) {
        case MODE_STEP: inst->display.flags = (inst->display.flags & ~DISP_FLAG_STEP_MASK) | DISP_STEP; break;
        case MODE_MPG:  inst->display.flags = (inst->display.flags & ~DISP_FLAG_STEP_MASK) | DISP_MPG;  break;
        case MODE_CON:  inst->display.flags = (inst->display.flags & ~DISP_FLAG_STEP_MASK) | DISP_CON;  break;
        }
    }

    /* Jog dial */
    on_jog_dial(inst, pkt->step_count);
}

/* ========================================================================== */
/* Display update                                                              */
/* ========================================================================== */

static void set_coord(display_coord_t *c, float val)
{
    float absval = fabsf(val);
    c->sign = (val < 0) ? 1 : 0;
    uint32_t scaled = (uint32_t)rintf(absval * 10000.0f);
    c->integer  = (uint16_t)(scaled / 10000);
    c->fraction = (uint16_t)(scaled % 10000);
}

static void update_display(whb_inst_t *inst)
{
    hal_pins_t *p = inst->pins;
    usb_out_data_t *d = &inst->display;

    d->header = 0xfdfe;
    d->seed   = 0xfe;

    /* Reset flag */
    if (!*p->machine_is_on)
        d->flags |= DISP_FLAG_RESET;
    else
        d->flags &= ~DISP_FLAG_RESET;

    /* Relative/absolute */
    if (!inst->disp_absolute)
        d->flags |= DISP_FLAG_RELATIVE;
    else
        d->flags &= ~DISP_FLAG_RELATIVE;

    /* Feed/spindle rates */
    uint32_t spindle_inc = (uint32_t)*p->spindle_inc;
    uint32_t spindle_dec = (uint32_t)*p->spindle_dec;
    uint32_t spindle_ovr = (uint32_t)(*p->spindle_ovr_value * 100.0);
    uint32_t spindle_cmd = (uint32_t)*p->spindle_speed_cmd;
    uint32_t feed_rate   = (uint32_t)(*p->feed_ovr_value * 100.0);

    if (spindle_inc || spindle_dec)
        d->spindle_rate = (uint16_t)spindle_cmd;
    else
        d->spindle_rate = (uint16_t)spindle_ovr;
    d->feed_rate = (uint16_t)feed_rate;

    /* Coordinates */
    bool absolute = inst->disp_absolute;
    if (inst->disp_group == 0) {
        set_coord(&d->row1, absolute ? (float)*p->axis_pos[AXIS_X] : (float)*p->axis_pos_rel[AXIS_X]);
        set_coord(&d->row2, absolute ? (float)*p->axis_pos[AXIS_Y] : (float)*p->axis_pos_rel[AXIS_Y]);
        set_coord(&d->row3, absolute ? (float)*p->axis_pos[AXIS_Z] : (float)*p->axis_pos_rel[AXIS_Z]);
    } else {
        set_coord(&d->row1, absolute ? (float)*p->axis_pos[AXIS_A] : (float)*p->axis_pos_rel[AXIS_A]);
        set_coord(&d->row2, absolute ? (float)*p->axis_pos[AXIS_B] : (float)*p->axis_pos_rel[AXIS_B]);
        set_coord(&d->row3, absolute ? (float)*p->axis_pos[AXIS_C] : (float)*p->axis_pos_rel[AXIS_C]);
    }
}

static void clear_display(whb_inst_t *inst)
{
    usb_out_data_t *d = &inst->display;
    memset(d, 0, sizeof(*d));
    d->header = 0xfdfe;
    d->seed = 0xfe;
    d->flags = DISP_FLAG_RESET | DISP_MPG;
}

/* ========================================================================== */
/* USB send display data                                                       */
/* ========================================================================== */

static void send_display(whb_inst_t *inst)
{
    if (!inst->usb_dev)
        return;

    /* The display data is sent as 3 blocks of 8 bytes (report_id + 7 data bytes) */
    const uint8_t *data = (const uint8_t *)&inst->display;
    uint8_t block[8];

    for (int i = 0; i < 3; i++) {
        block[0] = 0x06; /* report ID */
        memcpy(&block[1], data + (i * 7), 7);

        int r = libusb_control_transfer(inst->usb_dev,
            LIBUSB_DT_HID,                        /* bmRequestType 0x21 */
            LIBUSB_REQUEST_SET_CONFIGURATION,     /* bRequest 0x09 */
            0x0306,                               /* wValue */
            0x00,                                 /* wIndex */
            block, sizeof(block), 0);

        if (r < 0) {
            gomc_log_warnf(inst->log, COMP_NAME, "display send failed: %s",
                           libusb_error_name(r));
            return;
        }
    }
}

/* ========================================================================== */
/* USB async transfer callback                                                 */
/* ========================================================================== */

static int usb_open(whb_inst_t *inst);
static void usb_close(whb_inst_t *inst);

static void usb_callback(struct libusb_transfer *transfer)
{
    whb_inst_t *inst = (whb_inst_t *)transfer->user_data;

    switch (transfer->status) {
    case LIBUSB_TRANSFER_COMPLETED:
        if (transfer->actual_length == sizeof(usb_in_packet_t)) {
            const usb_in_packet_t *pkt = (const usb_in_packet_t *)transfer->buffer;

            /* Detect sleep: empty packet (all zeros except header+random+crc) */
            if (pkt->button1 == 0 && pkt->button2 == 0 &&
                pkt->feed_code == 0 && pkt->axis_code == 0 && pkt->step_count == 0) {
                *inst->pins->pendant_sleeping = 1;
            } else {
                if (*inst->pins->pendant_sleeping) {
                    *inst->pins->pendant_sleeping = 0;
                }
                process_input(inst, pkt);
            }
        }
        /* Re-submit */
        if (inst->running)
            libusb_submit_transfer(transfer);
        break;

    case LIBUSB_TRANSFER_TIMED_OUT:
        if (inst->running)
            libusb_submit_transfer(transfer);
        break;

    case LIBUSB_TRANSFER_CANCELLED:
        break;

    default:
        gomc_log_errorf(inst->log, COMP_NAME, "USB transfer error: %d", transfer->status);
        inst->running = false;
        break;
    }
}

/* ========================================================================== */
/* USB thread                                                                  */
/* ========================================================================== */

static void *usb_thread_fn(void *arg)
{
    whb_inst_t *inst = (whb_inst_t *)arg;
    struct timeval tv;

    if (usb_open(inst) < 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "USB open failed, thread exiting");
        return NULL;
    }

    while (inst->running && !gomc_should_exit(inst->exit_fd)) {
        tv.tv_sec  = 0;
        tv.tv_usec = 200000; /* 200ms */
        int r = libusb_handle_events_timeout_completed(inst->usb_ctx, &tv, NULL);
        if (r != 0 && r != LIBUSB_ERROR_TIMEOUT && r != LIBUSB_ERROR_INTERRUPTED) {
            if (r == LIBUSB_ERROR_NO_DEVICE) {
                gomc_log_warnf(inst->log, COMP_NAME, "device disconnected");
                break;
            }
        }
        if (inst->running)  {
            update_display(inst);
            send_display(inst);
        }
    }

    usb_close(inst);
    return NULL;
}

/* ========================================================================== */
/* USB init/cleanup                                                             */
/* ========================================================================== */

static int usb_open(whb_inst_t *inst)
{
    int r = libusb_init(&inst->usb_ctx);
    if (r != 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "libusb_init failed: %s", libusb_error_name(r));
        return -1;
    }

    /* Wait for device with optional timeout */
    int wait = inst->wait_timeout;
    do {
        inst->usb_dev = libusb_open_device_with_vid_pid(inst->usb_ctx, USB_VENDOR_ID, USB_PRODUCT_ID);
        if (inst->usb_dev)
            break;
        if (gomc_should_exit(inst->exit_fd) || !inst->running) {
            libusb_exit(inst->usb_ctx);
            inst->usb_ctx = NULL;
            return -1;
        }
        if (wait > 0) {
            sleep(1);
            wait--;
        } else if (wait == 0) {
            gomc_log_errorf(inst->log, COMP_NAME, "device not found (timeout)");
            libusb_exit(inst->usb_ctx);
            inst->usb_ctx = NULL;
            return -1;
        } else {
            /* wait < 0: wait forever */
            sleep(1);
        }
    } while (!inst->usb_dev);

    if (!inst->usb_dev) {
        libusb_exit(inst->usb_ctx);
        inst->usb_ctx = NULL;
        return -1;
    }

    /* Detach kernel driver if active */
    if (libusb_kernel_driver_active(inst->usb_dev, 0) == 1) {
        libusb_detach_kernel_driver(inst->usb_dev, 0);
    }

    r = libusb_claim_interface(inst->usb_dev, 0);
    if (r != 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "claim interface failed: %s", libusb_error_name(r));
        libusb_close(inst->usb_dev);
        inst->usb_dev = NULL;
        libusb_exit(inst->usb_ctx);
        inst->usb_ctx = NULL;
        return -1;
    }

    /* Setup async bulk transfer */
    inst->in_transfer = libusb_alloc_transfer(0);
    libusb_fill_bulk_transfer(inst->in_transfer, inst->usb_dev,
                              (0x1 | LIBUSB_ENDPOINT_IN),
                              inst->in_buf, sizeof(inst->in_buf),
                              usb_callback, inst, 750);

    r = libusb_submit_transfer(inst->in_transfer);
    if (r != 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "submit transfer failed: %s", libusb_error_name(r));
        libusb_free_transfer(inst->in_transfer);
        inst->in_transfer = NULL;
        libusb_release_interface(inst->usb_dev, 0);
        libusb_close(inst->usb_dev);
        inst->usb_dev = NULL;
        libusb_exit(inst->usb_ctx);
        inst->usb_ctx = NULL;
        return -1;
    }

    gomc_log_infof(inst->log, COMP_NAME, "device opened successfully");
    *inst->pins->pendant_connected = 1;
    return 0;
}

static void usb_close(whb_inst_t *inst)
{
    if (inst->in_transfer) {
        libusb_cancel_transfer(inst->in_transfer);
        /* Handle remaining events so the cancelled transfer completes */
        struct timeval tv = { 1, 0 };
        libusb_handle_events_timeout_completed(inst->usb_ctx, &tv, NULL);
        libusb_free_transfer(inst->in_transfer);
        inst->in_transfer = NULL;
    }
    if (inst->usb_dev) {
        libusb_release_interface(inst->usb_dev, 0);
        libusb_close(inst->usb_dev);
        inst->usb_dev = NULL;
    }
    if (inst->usb_ctx) {
        libusb_exit(inst->usb_ctx);
        inst->usb_ctx = NULL;
    }
    *inst->pins->pendant_connected = 0;
}

/* ========================================================================== */
/* cmod lifecycle                                                               */
/* ========================================================================== */

static int whb_Init(cmod_t *self)
{
    (void)self;
    return 0;
}

static int whb_Start(cmod_t *self)
{
    whb_inst_t *inst = (whb_inst_t *)self->priv;

    inst->running = true;
    clear_display(inst);

    if (pthread_create(&inst->usb_thread, NULL, usb_thread_fn, inst) != 0) {
        gomc_log_errorf(inst->log, COMP_NAME, "failed to create USB thread");
        return -1;
    }

    return 0;
}

static void whb_Stop(cmod_t *self)
{
    whb_inst_t *inst = (whb_inst_t *)self->priv;

    inst->running = false;
    pthread_join(inst->usb_thread, NULL);
}

static void whb_Destroy(cmod_t *self)
{
    whb_inst_t *inst = (whb_inst_t *)self->priv;

    if (inst->hal_id > 0)
        inst->env->hal->exit(inst->env->hal->ctx, inst->hal_id);

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

    whb_inst_t *inst = calloc(1, sizeof(whb_inst_t));
    if (!inst)
        return -1;

    inst->env = env;
    inst->log = env->log;
    inst->exit_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    inst->step_mode = MODE_MPG;
    inst->feed_pos = FPOS_UNDEF;
    inst->active_axis = -1;
    inst->active_counter = CNT_UNDEFINED;
    inst->cur_meta = -1;
    inst->spindle_fwd = true;
    inst->wait_timeout = -1; /* wait indefinitely by default */
    inst->counters[CNT_LEAD] = 100; /* match original */

    /* Parse command-line options */
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0)
            inst->wait_timeout = 3;
        else if (strcmp(argv[i], "-s") == 0)
            inst->lead_mode_spindle = true;
        else if (strcmp(argv[i], "-f") == 0)
            inst->lead_mode_feed = true;
        else if (strcmp(argv[i], "-B") == 0)
            inst->step_5_10 = true;
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
        gomc_log_errorf(inst->log, COMP_NAME, "pin creation failed");
        hal->exit(hal->ctx, inst->hal_id);
        free(inst);
        return -1;
    }

    hal->ready(hal->ctx, inst->hal_id);

    /* Build cmod_t */
    cmod_t *mod = calloc(1, sizeof(cmod_t));
    if (!mod) {
        hal->exit(hal->ctx, inst->hal_id);
        free(inst);
        return -1;
    }
    mod->Init    = whb_Init;
    mod->Start   = whb_Start;
    mod->Stop    = whb_Stop;
    mod->Destroy = whb_Destroy;
    mod->priv    = inst;

    *out = mod;
    gomc_log_infof(inst->log, COMP_NAME, "initialized (lead_spindle=%d lead_feed=%d step_5_10=%d)",
                   inst->lead_mode_spindle, inst->lead_mode_feed, inst->step_5_10);
    return 0;
}
