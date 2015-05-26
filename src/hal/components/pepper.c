/********************************************************************
* Description:  pepper.c
*               This file, 'pepper.c', is a real-time HAL component
*               that  is used to configure the PEPPER stepper driver
*               module used with the BeBoPr boards.
*
* Author: Bas Laarhoven
* License: GPL Version 2
*    
* Copyright (c) 2014 All rights reserved.
*
* Last change:  16-04-2014 sjl - created.
********************************************************************/

/** This file, 'pepper.c', is a HAL component that takes care
    of configuring the Pepper 5-axes stepper driver board when
    connected to a BeBoPr++.
    Settings like micro step factor, decays settings and current
    limit are transfered via a one-way serial protocol from the
    BeBoPr to the Pepper.

    The pepper components exports the 'update' function that needs
    to run at a high update rate, as do the components that process
    the generated signals. The pepper component is a real-time
    component does not use floating point functions.

    This component should be the last link the io_enable chain
    to the BeBopr. It is used to enter configuration mode on
    the Pepper board.

    The Pepper board has four different enable states controlled
    by two enable lines. Currently only two different states are
    used: Idle (00) and Active (11). The other two states are
    programmed with the same settings as the Active state.

    Programmable settings (parameters)

    idle-current
    active-current
    idle-decay
    active-decay
    no-store

    Decay settings:
      eSlowDecay  = 0, eFastDecay  = 1, eMixedDecay = 2
    Current settings:
      use 100mA units, e.g. 8 sets 0.8 A peak current.
    Volatile settings:
      setting no-store to 1 does not store settings in EEPROM

*/

/** Copyright (C) 2014 Bas Laarhoven <sjml AT xs4all DOT nl>
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

#include <float.h>
#include "rtapi_math.h"
#include <rtapi_string.h>
#include <stdint.h>

#include "rtapi.h"              /* RTAPI realtime OS API */
#include "rtapi_app.h"          /* RTAPI realtime module decls */
#include "hal.h"                /* HAL public API decls */

/* module information */
MODULE_AUTHOR("Bas Laarhoven");
MODULE_DESCRIPTION("Pepper Board Configuration Component for EMC HAL");
MODULE_LICENSE("GPL");


#if !defined( BUILD_SYS_USER_DSO)
#error "This driver is for usermode threads only"
#endif

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/** This structure contains the runtime data for the component.
*/

#define NR_AXES (5)             /* axes per PEPPER board */

static int count = 1;           /* number of PEPPER boards */
RTAPI_MP_INT( count, "number of PEPPER boards attached");

static int debug = 0;
RTAPI_MP_INT( debug, "debug level");


typedef enum {
    eSlowDecay = 0,
    eFastDecay = 1,
    eMixedDecay = 2
} decay_type;

typedef struct {
    hal_s32_t micro_step;       /* parameter: input */
    hal_s32_t active_current;   /* parameter: input */
    hal_s32_t active_decay;     /* parameter: input */
    hal_s32_t idle_current;     /* parameter: input */
    hal_s32_t idle_decay;       /* parameter: input */
} axis_config_t;

typedef struct {
    hal_bit_t* io_ena_in;               /* pin: input */
    hal_bit_t* io_ena_out;              /* pin: output */
    hal_bit_t* stepper_ena_in[ NR_AXES];/* pin: input */
    hal_bit_t* enable_sck;              /* pin: output */
    hal_bit_t* spindle_mosi;            /* pin: output */

    hal_bit_t no_store;                 /* parameter: input */
    hal_s32_t cycle_time;               /* parameter: input */
    hal_s32_t num_axes;                 /* parameter: output */

    axis_config_t (*axis_config)[];
} hal_pepper_t;

#define NR_ITEMS( x) (sizeof((x)) / sizeof(*(x)))

/* pointer to hal_pepper_t data in shared memory */
static hal_pepper_t* pepper_data;

/* other globals */
static int comp_id;             /* component ID */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int  pepper_export( hal_pepper_t* addr, const char* prefix);
static void pepper_update( void *arg, long period);
static int  pepper_spi_prepare( hal_pepper_t* data, int board_nr);
static int  pepper_spi_next_bit( int* spi_data);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/


int rtapi_app_main( void)
{
    const char prefix[] = "pepper";

    if (debug) {
        rtapi_print_msg( RTAPI_MSG_ERR, "PEPPER: INFO: number of boards: %d\n", count);
    }

    /* validate number of boards */
    if (count < 1 || count > 2) {
        rtapi_print_msg( RTAPI_MSG_ERR,
            "PEPPER: ERROR: invalid board count: %d\n", count);
        return -1;
    }

    /* connect to the HAL */
    comp_id = hal_init( "pepper");
    if (comp_id < 0) {
        rtapi_print_msg( RTAPI_MSG_ERR, "PEPPER: ERROR: hal_init() failed\n");
        return -1;
    }

    /* allocate shared memory for pepper data */
    pepper_data = hal_malloc( sizeof( hal_pepper_t));
    if (pepper_data == 0) {
        rtapi_print_msg( RTAPI_MSG_ERR, "PEPPER: ERROR: hal_malloc(1) failed\n");
        hal_exit( comp_id);
        return -1;
    }

    pepper_data->num_axes = count * NR_AXES;

    /* allocate shared memory for pepper data */
    pepper_data->axis_config = hal_malloc( pepper_data->num_axes * sizeof( axis_config_t));
    if (pepper_data->axis_config == 0) {
        rtapi_print_msg( RTAPI_MSG_ERR, "PEPPER: ERROR: hal_malloc(2) failed\n");
        hal_exit( comp_id);
        return -1;
    }

    /* export variables and functions */
    int retval = pepper_export( pepper_data, prefix);

    if (retval != 0) {
        rtapi_print_msg( RTAPI_MSG_ERR, "PEPPER: ERROR: pepper var export failed\n");
        hal_exit( comp_id);
        return -1;
    }
    rtapi_print_msg( RTAPI_MSG_INFO, "PEPPER: installed PEPPER configuration module\n");
    hal_ready( comp_id);
    return 0;
}

void rtapi_app_exit( void)
{
    hal_exit( comp_id);
}

/***********************************************************************
*                  REALTIME HANDLING OF I/O SIGNALS                    *
************************************************************************/

static int bit_cnt;

static void pepper_update( void *arg, long period)
{
    static int state = 0;
    static int init_done = 0;
    static int io_ena = 0;
    static int enable_sck = 0;
    static int spindle_mosi = 0;
    static long delay = 0;
    static int board_nr = 0;

    if (delay > 0 && period > 0 && delay > period) {
        delay -= period;
        return;
    }
    delay = 0;

    hal_pepper_t* pepper = arg;

    switch (state) {
    case 0:
    /**
     *  Normal operation with io_ena negated
     *    Disable all enables.
     */
        if (*(pepper->io_ena_in) != 0) {
            if (init_done == 0) {
    /**
     *  Assertion of io_ena input by system:
     *    If needed, start a SPI transfer cycle.
     */
                state = 5;
                board_nr = 0;
                break;
            } else {
                state = 1;
            }
        }
        io_ena = 0;
        enable_sck = 0;
        spindle_mosi = 0;
        break;

    case 1:
    /**
     *  Normal operation with io_ena asserted
     *    Generate the two enable outputs from the 5 inputs.
     *    Simple, two state implementation (00 & 11).
     */
        if (*(pepper->io_ena_in) == 0) {
            spindle_mosi = 
            enable_sck = 0;
            state = 0;
        } else {
            spindle_mosi = 
            enable_sck =
                *(pepper->stepper_ena_in[ 0]) ||
                *(pepper->stepper_ena_in[ 1]) ||
                *(pepper->stepper_ena_in[ 2]) ||
                *(pepper->stepper_ena_in[ 3]) ||
                *(pepper->stepper_ena_in[ 4]);
        }
        break;

    case 5:
    /**
     *  Start of SPI transfer cycle:
     *    Keep io_ena asserted for a short while
     *    to be able to create a falling edge.
     */
        rtapi_print_msg( RTAPI_MSG_INFO, "PEPPER: configuring Pepper stepper drivers for board %d\n", board_nr);
        pepper_spi_prepare( pepper, board_nr);
        io_ena = 1;
        delay = 50000000UL;     // long time active, like BeBoPr sw
        ++state;
        break;

    case 6:
    /**
     *  Start SPI transfer cycle:
     *    Negate io_ena output. This creates a
     *    falling edge. SPI transfer can start.
     *    Prepare data to transmit.
     */
        io_ena = 0;
        delay = 500000UL;
        ++state;
        break;

    case 7:
    /**
     *  Start SPI transfer cycle:
     *    Assert io_ena output. This creates a
     *    rising edge. SPI transfer can start.
     *    Prepare data to transmit.
     */
        io_ena = 1;
        enable_sck = 0;
        spindle_mosi = 0;
        delay = 600000UL;
        state = 10;
        break;

    case 10:
    /**
     *  SPI transfer, setup data:
     *    Clock is low. Get next bit to send.
     *    Generate data setup period.
     */
        if (pepper_spi_next_bit( &spindle_mosi) < 0) {
            state = 20;
            break;
        }
        delay = 50000UL;        // data setup time wrt rising edge
        ++state;
        break;

    case 11:
    /**
     *  SPI transfer, generate rising clock edge:
     *    Data is stable, assert clock.
     *    Generate clock active period.
     */
        enable_sck = 1;
        delay = 50000UL;        // data setup time wrt rising edge
        ++state;
        break;

    case 12:
    /**
     *  SPI transfer, generate falling clock edge:
     *    Keep data, negate clock.
     *    Generate data hold period.
     */
        enable_sck = 0;
        delay = 50000UL;        // data setup time wrt rising edge
        state = (bit_cnt == 0) ? 6 : 10;
        break;

    case 20:
    /**
     *  SPI transfer complete:
     *    Assert io_ena output, clear enables (sck & mosi)
     */
        io_ena = 1;
        enable_sck = 0;
        spindle_mosi = 0;
        delay = 250000UL;

        if (++board_nr * NR_AXES < pepper->num_axes) {
            state = 5;
        } else {
/* disabling allows one to experiment with settings */
#if 0
            init_done = 1;
#endif
            state = 1;
        }
        break;

    default:
    /**
     *  Catch all, try to recover
     */
        io_ena = 0;
        init_done = 0;
        enable_sck = 0;
        spindle_mosi = 0;
        delay = 250000UL;
        state = 0;
        break;
    }
    *(pepper->io_ena_out) = io_ena;
    *(pepper->enable_sck) = enable_sck;
    *(pepper->spindle_mosi) = spindle_mosi;
}


/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int pepper_export( hal_pepper_t* addr, const char* prefix)
{
    int retval;
    int i;
       
    retval = hal_pin_bit_newf( HAL_IN, &(addr->io_ena_in), comp_id, "%s.io-ena.in", prefix);
    if (retval != 0) {
        return retval;
    }

    retval = hal_pin_bit_newf( HAL_OUT, &(addr->io_ena_out), comp_id, "%s.io-ena.out", prefix);
    if (retval != 0) {
        return retval;
    }

    for (i = 0 ; i < NR_ITEMS( addr->stepper_ena_in) ; ++i) {
        retval = hal_pin_bit_newf( HAL_IN, &(addr->stepper_ena_in[ i]), comp_id, "%s.stepper-ena.%d.in", prefix, i);
        if (retval != 0) {
            return retval;
        }
    }

    retval = hal_pin_bit_newf( HAL_OUT, &(addr->enable_sck), comp_id, "%s.enable-sck.out", prefix);
    if (retval != 0) {
        return retval;
    }

    retval = hal_pin_bit_newf( HAL_OUT, &(addr->spindle_mosi), comp_id, "%s.spindle-mosi.out", prefix);
    if (retval != 0) {
        return retval;
    }

    retval = hal_param_bit_newf( HAL_RW, &(addr->no_store), comp_id, "%s.no-store", prefix);
    if (retval != 0) {
        return retval;
    }
    addr->no_store = 1;

    retval = hal_param_s32_newf( HAL_RW, &(addr->cycle_time), comp_id, "%s.cycle-time", prefix);
    if (retval != 0) {
        return retval;
    }
    addr->cycle_time = 0;       // unknown value

    retval = hal_param_s32_newf( HAL_RO, &(addr->num_axes), comp_id, "%s.num_axes", prefix);
    if (retval != 0) {
        return retval;
    }

    for (i = 0 ; i < addr->num_axes ; ++i) {

        axis_config_t* aci = &(*(addr->axis_config))[ i];
        char buf[ HAL_NAME_LEN + 1];

        rtapi_snprintf( buf, sizeof( buf), "%s.axis.%d", prefix, i);

        retval = hal_param_s32_newf( HAL_RW, &(aci->micro_step),
                                comp_id, "%s.micro-step", buf);
        if (retval != 0) {
            return retval;
        }

        retval = hal_param_s32_newf( HAL_RW, &(aci->active_current),
                                comp_id, "%s.active-current", buf);
        if (retval != 0) {
            return retval;
        }

        retval = hal_param_s32_newf( HAL_RW, &(aci->active_decay),
                                comp_id, "%s.active-decay", buf);
        if (retval != 0) {
            return retval;
        }

        retval = hal_param_s32_newf( HAL_RW, &(aci->idle_current),
                                comp_id, "%s.idle-current", buf);
        if (retval != 0) {
            return retval;
        }

        retval = hal_param_s32_newf( HAL_RW, &(aci->idle_decay),
                                comp_id, "%s.idle-decay", buf);
        if (retval != 0) {
            return retval;
        }

        aci->micro_step     = 8;        // 1:8 microstep
        aci->active_current = 6;        // 0.6 A peak current
        aci->active_decay   = 1;        // fast decay
        aci->idle_current   = 3;        // 0.3 A hold current
        aci->idle_decay     = 1;        // fast decay
    }

    /* export processing function */
    char buf[ HAL_NAME_LEN + 1];
    rtapi_snprintf( buf, sizeof( buf), "%s.update", prefix);
    retval = hal_export_funct( buf, pepper_update, pepper_data, 0, 0, comp_id);
    if (retval != 0) {
        rtapi_print_msg( RTAPI_MSG_ERR, "PEPPER: ERROR: update function export failed\n");
        hal_exit( comp_id);
        return -1;
    }
    return 0;
}


//////////////////////////// pepper.c code from BeBoPr /////////////////////////////


static uint16_t crc_xmodem_update( uint16_t crc, uint8_t data)
{
    crc = crc ^ ((uint16_t)data << 8);
    int i;
    for (i = 0 ; i < 8 ; ++i) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ 0x1021;
        } else {
            crc = (crc << 1);
        }
    }
    return crc;
}

struct __attribute__ ((__packed__)) ms_struct {
    uint8_t x      : 3;
    uint8_t y      : 3;
    uint8_t z      : 3;
    uint8_t a      : 3;
    uint8_t b      : 3;
};

struct ena_reg_struct {
    uint8_t pwm    : 5;
    uint8_t decay  : 2;
    uint8_t ena    : 1;
};

struct ena_struct {
    struct ena_reg_struct x, y, z, a, b;
};

/*
 *  SPI data header definition
 */
struct spi_header_struct {
    uint16_t    sync;
    uint8_t     version    : 4;
    uint8_t     revision   : 4;
    uint8_t     command;
};

static union pepper_config_union {
    struct __attribute__ ((__packed__)) {
        struct spi_header_struct header;
        uint8_t no_store   : 1;
        uint8_t address    : 7;
        struct ms_struct ms;
        struct ena_struct ena[ 4];
        uint16_t check;
    };
    uint8_t bytes[ 0];
} cfg;

/** Convert requested micro stepping factor to hardware bit pattern.
 *  All unavailable factors result in no micro-stepping at all.
 */
static unsigned int get_mode_bits( unsigned int micro_step)
{
    switch (micro_step) {
    case 2:  return 1;
    case 4:  return 2;
    case 8:  return 3;
    case 16: return 4;
    case 32: return 5;
    }
    return 0;
}

static uint16_t check;
static int byte_ix = 0;
static int bit_cnt = 0;
static int spi_busy = 0;

static int pepper_spi_prepare( hal_pepper_t* data, int board_nr)
{
    // set header and version info, clear checksum field
    cfg.header.sync     = 0xba51;
    cfg.header.version  = 3;
    cfg.header.revision = 0;
    cfg.header.command  = 0xc0;
    cfg.check    = 0;
    cfg.no_store = data->no_store;
    cfg.address  = board_nr + 1;
    // set stepper driver mode select fields
    cfg.ms.x = get_mode_bits( (*data->axis_config)[ board_nr * NR_AXES + 0].micro_step);
    cfg.ms.y = get_mode_bits( (*data->axis_config)[ board_nr * NR_AXES + 1].micro_step);
    cfg.ms.z = get_mode_bits( (*data->axis_config)[ board_nr * NR_AXES + 2].micro_step);
    cfg.ms.a = get_mode_bits( (*data->axis_config)[ board_nr * NR_AXES + 3].micro_step);
    cfg.ms.b = get_mode_bits( (*data->axis_config)[ board_nr * NR_AXES + 4].micro_step);
    // set all enable state registers
    int i, j;
    for (j = 0 ; j < 4 ; ++j) { // all four enable states
        struct ena_struct* es = &cfg.ena[ j];

        for (i = 0 ; i < 5 ; ++i) {     // all five axes
            axis_config_t* daci = &(*data->axis_config)[ board_nr * NR_AXES + i];
            struct ena_reg_struct* ers = NULL;

            switch (i) {
            case 0: ers = &es->x; break;
            case 1: ers = &es->y; break;
            case 2: ers = &es->z; break;
            case 3: ers = &es->a; break;
            case 4: ers = &es->b; break;
            }
            if (j == 0) {       // enable mode 0
                unsigned int hold_current = daci->idle_current;
                if (hold_current > 0) {
                    ers->pwm = hold_current;
                    ers->ena = 1;
                } else {
                    ers->pwm = daci->active_current;
                    ers->ena = 0;
                }
                ers->decay = daci->idle_decay;
            } else {            // enable modes 1,2&3
                ers->pwm = daci->active_current;
                ers->ena = 1;
                ers->decay = daci->active_decay;
            }
        }
    }
    check = 0;
    byte_ix = 0;
    bit_cnt = 0;
    spi_busy = 1;
    return 0;
}

static int pepper_spi_next_bit( int* spi_data)
{
    static uint8_t data;

#define BYTE_SWAP( x) ( (((x) & 255) << 8) | (((x) >> 8) & 255) )

    if (!spi_busy) {
        return -1;
    }
    if (bit_cnt == 0) {
        if (byte_ix < sizeof( cfg)) {
            if (byte_ix < sizeof( cfg) - sizeof( cfg.check)) {
                check = crc_xmodem_update( check, cfg.bytes[ byte_ix]);
            } else if (byte_ix == sizeof( cfg) - sizeof( cfg.check)) {
                cfg.check = BYTE_SWAP( check);
            }
            data = cfg.bytes[ byte_ix];
            ++byte_ix;
            bit_cnt = 7;
        } else {
            rtapi_print_msg( RTAPI_MSG_INFO,
                            "PEPPER: configuration sent %d bytes, crc16 is $%04X\n",
                            sizeof( cfg), cfg.check);
            spi_busy = 0;
            return -1;
        }
    } else {
        data <<= 1;
        --bit_cnt;
    }
    *spi_data = (data & 0x80) ? 1 : 0;
    return 0;
}
