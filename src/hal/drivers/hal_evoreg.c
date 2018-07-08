/********************************************************************
* Description:  hal_evoreg.c
*               This file, 'hal_evoreg.c', is a HAL component that 
*               provides a driver for the Siemens EVOREG motion 
*               control board.
*
* Author: Martin Kuhnle, John Kasunich
* License: GPL Version 2
*    
* Copyright (c) 2003 All rights reserved.
*
* Last change: 
********************************************************************/

/** This file, 'hal_evoreg.c', is a HAL component that provides a
    driver for the Siemens EVOREG motion control board.
    This board privides three 16bit DAC's, three encoder inputs,
    46 digital inputs and 21 digital outputs

    Fixme: error messages are not proper up to now
    ToDo: better error messages
          make inverted bits available
          posibility to read back outputs
          check dac values for limits
          scale for every dacs
          scale for every encoder
          check if the dacs are updated all at the same time
          enable Interrupts
          check for wire break
          watchdog



 Copyright (C) 2003 Martin Kuhnle
                       <mkuhnle AT users DOT sourceforge DOT net>
                John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>
                       
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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

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

#include "rtapi_ctype.h"	/* isspace() */
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */

/* If FASTIO is defined, uses outb() and inb() from <asm.io>,
   instead of rtapi_outb() and rtapi_inb() - the <asm.io> ones
   are inlined, and save a microsecond or two (on my 233MHz box)
*/
#define FASTIO

#ifdef FASTIO
#define rtapi_inb inb
#define rtapi_outb outb
#include <asm/io.h>
#endif

/* module information */
MODULE_AUTHOR("Martin Kuhnle");
MODULE_DESCRIPTION("SIEMENS-EVOREG Driver for EMC HAL");
MODULE_LICENSE("GPL");
/* static char *cfg = 0; */
/* config string
RTAPI_MP_STRING(cfg, "config string"); */

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the runtime data needed by the
   driver for a single port/channel
*/

typedef struct {
	void *io_base;
        hal_float_t *dac_out[3];  /* ptrs for dac output */
	hal_float_t *position[3];	   /* ptrs for encoder input */
	hal_bit_t *digital_in[47];    /* ptrs for digital input pins 0 - 45 */
        hal_bit_t *digital_out[25];    /* ptrs for digital output pins 0 - 20 */
        __u16 raw_counts_old[3];
        __s32 counts[3];
        hal_float_t pos_scale;         /*! \todo scale for position command FIXME schould be one per axis */
} evoreg_t;

/* pointer to array of evoreg_t structs in shared memory, 1 per port */
static evoreg_t *port_data_array;

/* other globals */
static int comp_id;		/* component ID */
static int num_ports;		/* number of ports configured */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
/* These is the functions that actually do the I/O
   everything else is just init code
*/
static void update_port(void *arg, long period);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

#define MAX_PORTS 8
#define MAX_DAC 3               /* number of DACs per card */
#define MAX_ENC 3               /* number of Encoders per card */

#define MAX_TOK ((MAX_PORTS*2)+3)

int rtapi_app_main(void)
{
    char name[HAL_NAME_LEN + 1];
    int n,i , retval, num_dac, num_enc;

    unsigned int base=0x300;

    /* only one port at the moment */
    num_ports = 1;
    n = 0;

    #define ISA_BASE    0xC9000
    #define ISA_MAX    0x100000  /* allgemeiner Speicherzugriff */

    
    /* STEP 1: initialise the driver */
    comp_id = hal_init("hal_evoreg");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "EVOREG: ERROR: hal_init() failed\n");
	return -1;
    }

    /* STEP 2: allocate shared memory for EVOREG data */
    port_data_array = hal_malloc(num_ports * sizeof(evoreg_t));
    if (port_data_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "EVOREG: ERROR: hal_malloc() failed\n");
	hal_exit(comp_id);
	return -1;
    }

    /*! \todo FIXME: Test memory area and setup the card */
    port_data_array->io_base = ioremap(ISA_BASE, ISA_MAX - ISA_BASE);
    rtapi_print_msg(RTAPI_MSG_ERR,"EVOREG: io_base: %p \n", port_data_array->io_base);
    outw(0x82c9,base); /* set indexregister */

    /* Set all outputs to zero */
    writew(0, port_data_array->io_base + 0x20); /* digital out 0-15  */
    writew(0, port_data_array->io_base + 0x40); /* digital out 16-23 */
    writew(0, port_data_array->io_base + 0x60); /* DAC 1 */
    writew(0, port_data_array->io_base + 0x80); /* DAC 2 */
    writew(0, port_data_array->io_base + 0xa0); /* DAC 3 */
    /* Reset Encoder's */
    writew(0, port_data_array->io_base + 0x02); /* ENCODER 1 */
    writew(0, port_data_array->io_base + 0x0a); /* ENCODER 2 */
    writew(0, port_data_array->io_base + 0x12); /* ENCODER 3 */
    
    /* STEP 3: export the pin(s) */

    /* Export DAC pin's */
    for ( num_dac=1; num_dac<=MAX_DAC; num_dac++) {
      retval = hal_pin_float_newf(HAL_IN, &(port_data_array->dac_out[num_dac-1]),
				  comp_id, "evoreg.%d.dac-%02d-out", 1, num_dac);
      if (retval < 0) {
	  rtapi_print_msg(RTAPI_MSG_ERR,
	    "EVOREG: ERROR: port %d var export failed with err=%i\n", n + 1,
	    retval);
	hal_exit(comp_id);
	return -1;
      }
    }

    /* Export Encoder pin's */
    for ( num_enc=1; num_enc<=MAX_ENC; num_enc++) {
      retval = hal_pin_float_newf(HAL_OUT, &(port_data_array->position[num_enc - 1]),
				  comp_id, "evoreg.%d.position-%02d-in", 1, num_enc);
      if (retval < 0) {
	  rtapi_print_msg(RTAPI_MSG_ERR,
	      "EVOREG: ERROR: port %d var export failed with err=%i\n", n + 1,
	      retval);
  	  hal_exit(comp_id);
  	  return -1;
      }
    }

    /* Export IO pin's */

    /* export write only HAL pin's for the input bit */
    for ( i=0; i<=45;i++) {
      retval += hal_pin_bit_newf(HAL_OUT, &(port_data_array->digital_in[i]),
				 comp_id, "evoreg.%d.pin-%02d-in", 1, i);

      /* export another write only HAL pin for the same bit inverted */
      /*
      retval += hal_pin_bit_newf(HAL_OUT, &(port_data_array->digital_in[(2*i)+1]),
				 comp_id, "evoreg.%d.pin-%02d-in-not", 1, i); */
      if (retval < 0) {
	  rtapi_print_msg(RTAPI_MSG_ERR,
	      "EVOREG: ERROR: port %d var export failed with err=%i\n", n + 1,
	      retval);
  	  hal_exit(comp_id);
  	  return -1;
      }
    }

    /* export read only HAL pin's for the output bit */
    for ( i=0; i<=23;i++) {
      retval += hal_pin_bit_newf(HAL_IN, &(port_data_array->digital_out[i]),
				 comp_id, "evoreg.%d.pin-%02d-out", 1, i);

      /* export another read only HAL pin for the same bit inverted */
      /*
      retval += hal_pin_bit_newf(HAL_IN, &(port_data_array->digital_out[(2*i)+1]),
				 comp_id, "evoreg.%d.pin-%02d-out-not", 1, i));  */
      if (retval < 0) {
	  rtapi_print_msg(RTAPI_MSG_ERR,
	      "EVOREG: ERROR: port %d var export failed with err=%i\n", n + 1,
	      retval);
  	  hal_exit(comp_id);
  	  return -1;
      }
    }

    /* export parameter for scaling */
    retval = hal_param_float_newf(HAL_RW, &(port_data_array->pos_scale),
				  comp_id, "evoreg.%d.position-scale", 1);
    if (retval != 0) {
	return retval;
    }


    /* STEP 4: export function */
    rtapi_snprintf(name, sizeof(name), "evoreg.%d.update", n + 1);
    retval = hal_export_funct(name, update_port, &(port_data_array[n]), 1, 0,
	comp_id);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "EVOREG: ERROR: port %d write funct export failed\n", n + 1);
	hal_exit(comp_id);
	return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
	"EVOREG: installed driver for %d card(s)\n", num_ports);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    outw(0x0,0x300);
    hal_exit(comp_id);
}

/**************************************************************
* REALTIME PORT WRITE FUNCTION                                *
**************************************************************/

static void update_port(void *arg, long period)
{
    evoreg_t *port;
    int pin;
    unsigned char tmp, mask;
    __u16 raw_counts[3];

    port = arg;

/* write DAC's */
    writew((*(port->dac_out[0])/10 * 0x7fff), port->io_base + 0x60);
    writew((*(port->dac_out[1])/10 * 0x7fff), port->io_base + 0x80);
    writew((*(port->dac_out[2])/10 * 0x7fff), port->io_base + 0xa0);

/* Read Encoders, improve the 16bit hardware counters to 32bit and scale the values */
    raw_counts[0] = (__u16) readw(port->io_base);
    raw_counts[1] = (__u16) readw(port->io_base + 0x08 );
    raw_counts[2] = (__u16) readw(port->io_base + 0x10 );

    port->counts[0] += (__s16) (raw_counts[0] - port->raw_counts_old[0]);
    port->raw_counts_old[0] = raw_counts[0];

    port->counts[1] += (__s16) (raw_counts[1] - port->raw_counts_old[1]);
    port->raw_counts_old[1] = raw_counts[1];

    port->counts[2] += (__s16) (raw_counts[2] - port->raw_counts_old[2]);
    port->raw_counts_old[2] = raw_counts[2];

    *port->position[0] = port->counts[0] * port->pos_scale;
    *port->position[1] = port->counts[1] * port->pos_scale;
    *port->position[2] = port->counts[2] * port->pos_scale;


/* read digital inputs */
     tmp = readw(port->io_base + 0x20);       /* digital input 0-15 */
      mask = 0x01;
	for (pin=0 ; pin < 16 ; pin++) {
	*port->digital_in[pin] = (tmp & mask) ? 1:0 ;
	mask <<= 1;
	}
     tmp = readw(port->io_base + 0x40);       /* digital input 16-31 */
      mask = 0x01;
	for (pin=16 ; pin < 32 ; pin++) {
	*port->digital_in[pin] = (tmp & mask) ? 1:0 ;
	mask <<= 1;
	}

     tmp = readw(port->io_base + 0x60);       /* digital input 32-45 */
      mask = 0x01;
	for (pin=32 ; pin < 46 ; pin++) {
	*port->digital_in[pin] = (tmp & mask) ? 1:0 ;
	mask <<= 1;
	}


/* write digital outputs */
     tmp = 0x0;
     mask = 0x01;
     for (pin=0; pin < 16; pin++) {
        if (port->digital_out[pin]) {
        tmp |= mask;
        mask <<= 1;
        }
     }
     writew( tmp, port->io_base + 0x20);  /* digital output 0-15 */


     tmp = 0x0;
     mask = 0x01;
     for (pin=16; pin < 24; pin++) {
        if (port->digital_out[pin]) {
        tmp |= mask;
        mask <<= 1;
        }
     }
     writew( tmp, port->io_base + 0x40);  /* digital output 16-23 */

}
