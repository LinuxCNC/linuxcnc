/********************************************************************
* Description:  hal_ppmc.c
*               HAL driver for the the Pico Systems family of
*               parallel port motion control boards, including
*               the PPMC board set, the USC, and the UPC. 
*
* Usage:  halcmd loadrt hal_ppmc port_addr=<addr1>[,addr2[,addr3]]
*               where 'addr1', 'addr2', and 'addr3' are the addresses
*               of up to three parallel ports.
*
* Author: John Kasunich, Jon Elson, Stephen Wille Padnos
* License: GPL Version 2
*    
* Copyright (c) 2005 All rights reserved.
*
* Last change: 
# $Revision$
* $Author$
* $Date$
********************************************************************/

/** The driver searches the entire address space of the enhanced
    parallel port (EPP) at 'port_addr', looking for any board(s)
    in the PPMC family.  It then exports HAL pins for whatever it
    finds, as well as a pair of functions, one that reads all 
    inputs, and one that writes all outputs.
*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2.1 of the GNU General
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

#ifndef RTAPI
#error This is a realtime component only!
#endif

#include <linux/slab.h>		/* kmalloc() */
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */

#define MAX_BUS 3	/* max number of parports (EPP busses) */

#ifdef MODULE
/* module information */
MODULE_AUTHOR("John Kasunich");
MODULE_DESCRIPTION("HAL driver for Universal PWM Controller");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
int port_addr[MAX_BUS] = { 0x0378, 0, 0 };  /* default, 1 bus at 0x0378 */
MODULE_PARM(port_addr, "1-3i");
MODULE_PARM_DESC(port_addr, "port address(es) for EPP bus(es)");
#endif /* MODULE */

/***********************************************************************
*                DEFINES (MOSTLY REGISTER ADDRESSES)                   *
************************************************************************/

#define SPPDATA(addr) addr
#define STATUSPORT(addr) addr+1
#define CONTROLPORT(addr) addr+2
#define ADDRPORT(addr) addr+3
#define DATAPORT(addr) addr+4

#define NUM_SLOTS      16
#define SLOT_SIZE      16
#define SLOT_ID_OFFSET 15

#define ENCCNT0    0x00          /*  EPP address to read first byte of encoder counter */
#define ENCCNT1    0x03
#define ENCCNT2    0x06
#define ENCCNT3    0x09

#define ENCCTRL    0x03          /* EPP address of encoder control register */
#define ENCRATE    0x04          /* interrupt rate register, only on master encoder */
#define ENCISR     0x0C		//index sense register
#define ENCLOAD    0x00          /* EPP address to write into first byte of preset */
				/* register for channels 0 - 3 */

#define DAC_0      0x00          /* EPP address of low byte of DAC chan 0 */
#define DAC_1      0x02          /* EPP address of low byte of DAC chan 1 */
#define DAC_2      0x04          /* EPP address of low byte of DAC chan 2 */
#define DAC_3      0x06          /* EPP address of low byte of DAC chan 3 */

#define DAC_MODE_0 0x4C
#define DAC_WRITE_0 0x48

#define DAC_MODE_1 0x5C
#define DAC_WRITE_1 0x58

#define RATE_GEN_0      0x10       /* EPP addr of low byte of rate generator 0 LSB */
//      RATE_GEN_0a     0x11       /* middle byte */
//      RATE_GEN_0b     0x12       /* MSB */
#define RATE_GEN_1      0x13
#define RATE_GEN_2      0x16
#define RATE_GEN_3      0x19
#define RATE_CTRL_0     0x1C
#define RATE_SETUP_0    0x1D
#define RATE_WIDTH_0    0x1E

#define UxC_DINA        0x0D       /* EPP address of digital inputs on univstep */
#define UxC_DINB        0x0E
#define UxC_ESTOP_IN    0x0E
/* The "estop" input will always report OFF to the software, regardless
   of the actual state of the physical input, unless the "estop" output
   has being turned on by the software. */

#define UxC_DOUTA       0x1F          /* EPP address of digital outputs */
#define UxC_ESTOP_OUT   0x1F
/* The physical output associated with the "estop" output will not come on
   unless the physical "estop" input is also on.  All physical outputs
   will not come on unless the physical "estop" output is on. */
   
/* These strange interactions between inputs and outputs are just a little
   crazy in my humble opinion */

#define DIO_DINA        0x00          /* EPP address of digital inputs on DIO */
#define DIO_DINB        0x01
#define DIO_ESTOP_IN    0x02

#define DIO_DOUTA       0x00          /* EPP address of digital outputs */
#define DIO_ESTOP_OUT   0x01

/***********************************************************************
*                       STRUCTURE DEFINITIONS                          *
************************************************************************/

/* this structure contains the runtime data for a digital output */
typedef struct {
    hal_bit_t *data;		/* output pin value */
    hal_bit_t invert;		/* parameter to invert output pin */
} dout_t;

/* this structure contains the runtime data for a digital input */
typedef struct {
    hal_bit_t *data;		/* input pin value */
    hal_bit_t *data_not;	/* inverted input pin value */
} din_t;

/* this structure contains the runtime data for a step pulse generator */
typedef struct {
    hal_float_t *vel;		/* velocity command */
    hal_float_t vel_scale;	/* parameter: scaling for vel to Hz */
    hal_float_t max_vel;	/* velocity limit */
    hal_float_t freq;		/* parameter: velocity cmd scaled to Hz */
} stepgen_t;

/* runtime data for a set of 4 step pulse generators */
typedef struct {
    stepgen_t sg[4];		/* per generator data */
    hal_u8_t setup_time;	/* setup time in 100nS increments */
    hal_u8_t pulse_width;	/* pulse width in 100nS increments */
} stepgens_t;


/* runtime data for a single encoder */
typedef struct {
    hal_float_t *position;      /* output: scaled position pointer */
    hal_s32_t *count;           /* output: unscaled encoder counts */
    hal_float_t scale;          /* parameter: scale factor */
    hal_bit_t *index;           /* output: index flag */
    signed long oldreading;     /* used to detect overflow / underflow of the counter */
} encoder_t;

/* this structure contains the runtime data for a single EPP bus slot */
/* A single slot can contain a wide variety of "stuff", ranging 
   from PWM or stepper or DAC outputs, to encoder inputs, to digital
   I/O.  at runtime, the proper function(s) need to be invoked to 
   handle it.  We do that by having an array of functions that are
   called in order.  The entries are filled in when the init code
   scans the bus and determines what is in each slot */

#define MAX_FUNCT 10

struct slot_data_s {};

typedef void (slot_funct_t)(struct slot_data_s *slot);

typedef struct slot_data_s {
    unsigned char id;		/* slot id code */
    unsigned char ver;		/* slot version code */
    unsigned int slot_base;	/* base addr of this 16 byte slot */
    unsigned int port_addr;	/* addr of parport */
    unsigned char first_rd;	/* first epp address needed by read_all */
    unsigned char last_rd;	/* last epp address needed by read_all */
    unsigned char num_rd_functs;/* number of read functions */
    unsigned char rd_buf[32];	/* cached data read from epp bus */
    slot_funct_t *rd_functs[MAX_FUNCT];	/* array of read functions */
    unsigned char first_wr;	/* first epp address needed by write_all */
    unsigned char last_wr;	/* last epp address needed by write_all */
    unsigned char num_wr_functs;/* number of write functions */
    unsigned char wr_buf[32];	/* cached data to be written to epp bus */
    slot_funct_t *wr_functs[MAX_FUNCT];	/* array of write functions */
    dout_t *digout;		/* ptr to shmem data for digital outputs */
    din_t *digin;		/* ptr to shmem data for digital inputs */
    stepgens_t *stepgen;	/* ptr to shmem data for step generators */
    encoder_t *encoder;         /* ptr to shmem data for encoders */
} slot_data_t;

/* this structure contains the runtime data for a complete EPP bus */

typedef struct {
//    unsigned int port_addr;	/* addr of parport to talk to board */
    int busnum;			/* bus number */
    unsigned char have_master;	/* true if a master has been configured */
    unsigned int last_digout;	/* used for numbering digital outputs */
    unsigned int last_digin;	/* used for numbering digital outputs */
    unsigned int last_stepgen;	/* used for numbering step generators */
    unsigned int last_encoder;	/* used for numbering encoders */
    char slot_valid[NUM_SLOTS];	/* tags for slots that are used */
    slot_data_t slot_data[NUM_SLOTS];  /* data for slots on EPP bus */
} bus_data_t;


/***********************************************************************
*                          GLOBAL VARIABLES                            *
************************************************************************/

static bus_data_t *bus_array[MAX_BUS];
static int comp_id;		/* component ID */

/***********************************************************************
*                    REALTIME FUNCTION DECLARATIONS                    *
************************************************************************/

static void read_all(void *arg, long period);
static void write_all(void *arg, long period);

static void read_digins(slot_data_t *slot);
static void write_digouts(slot_data_t *slot);
static void write_stepgens(slot_data_t *slot);

static void read_encoders(slot_data_t *slot);

/***********************************************************************
*                  REALTIME I/O FUNCTION DECLARATIONS                  *
************************************************************************/

static void BusReset(unsigned int port_addr);
static int ClrTimeout(unsigned int port_addr);
static unsigned short SelRead(unsigned char epp_addr, unsigned int port_addr);
static unsigned short ReadMore(unsigned int port_addr);
static void SelWrt(unsigned char byte, unsigned char epp_addr, unsigned int port_addr);
static void WrtMore(unsigned char byte, unsigned int port_addr);


/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int add_rd_funct(slot_funct_t *funct, slot_data_t *slot, int min_addr, int max_addr );
static int add_wr_funct(slot_funct_t *funct, slot_data_t *slot, int min_addr, int max_addr );

static int export_UxC_digin(slot_data_t *slot, bus_data_t *bus);
static int export_UxC_digout(slot_data_t *slot, bus_data_t *bus);
static int export_USC_stepgen(slot_data_t *slot, bus_data_t *bus);
static int export_UxC_encoders(slot_data_t *slot, bus_data_t *bus);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    int msg, rv, rv1, busnum, slotnum, n, boards;
    int idcode, id, ver;
    bus_data_t *bus;
    slot_data_t *slot;
    char buf[HAL_NAME_LEN + 2];

    rtapi_print_msg(RTAPI_MSG_INFO, "PPMC: installing driver\n");
    /* This function exports a lot of stuff, which results in a lot of
       logging if msg_level is at INFO or ALL. So we save the current value
       of msg_level and restore it later.  If you actually need to log this
       function's actions, change the second line below */
    msg = rtapi_get_msg_level();
//    rtapi_set_msg_level(RTAPI_MSG_WARN);

    /* validate port addresses */
    n = 0;
    /* if an error occurs, bailing out in the middle of init can leave
       a mess of partially allocated stuff.  So we don't normally bail
       out, instead we set rv non-zero to indicate failure, and carry
       on.  Later, we see that rv is set, and clean up anything that
       might have been allocated before we return. */
    rv = 0;
    for ( busnum = 0 ; busnum < MAX_BUS ; busnum++ ) {
	/* init pointer to bus data */
	bus_array[busnum] = NULL;
	/* check to see if a port address was specified */
	if ( port_addr[busnum] == 0 ) {
	    /* nope, skip it */
	    continue;
	}
	/* is it legal? */
	if ( port_addr[busnum] > 65535 ) {
	    /* nope, complain and skip it */
	    rtapi_print_msg(RTAPI_MSG_ERR, 
		"PPMC: ERROR: invalid port_addr: %0X\n", port_addr[busnum]);
	    rv = -1;
	    continue;
	}
	/* got a good one */
	n++;
    }
    if ( rv != 0 ) {
	/* one or more invalid addresses, already printed msg */
	return -1;
    }
    if ( n == 0 ) {
	rtapi_print_msg(RTAPI_MSG_ERR, 
	    "PPMC: ERROR: no ports specified\n");
	return -1;
    }
    /* have valid config info, connect to the HAL */
    comp_id = hal_init("hal_ppmc");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "PPMC: ERROR: hal_init() failed\n");
	return -1;
    }
    /* begin init - loop thru all busses */
    for ( busnum = 0 ; busnum < MAX_BUS ; busnum++ ) {
	/* check to see if a port address was specified */
	if ( port_addr[busnum] == 0 ) {
	    /* nope, skip to next bus */
	    continue;
	}
	rtapi_print_msg(RTAPI_MSG_INFO,
	    "PPMC: checking EPP bus %d at port %04X\n",
	    busnum, port_addr[busnum]);
	boards = 0;
	/* allocate memory for bus data - this is not shared memory */
	bus = kmalloc(sizeof(bus_data_t), GFP_KERNEL);
	if (bus == 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PPMC: ERROR: kmalloc() failed\n");
	    rv = -1;
	    /* skip to next bus */
	    continue;
	}
	/* clear the bus data structure */
	bus->busnum = busnum;
	bus->have_master = 0;
	bus->last_digout = 0;
	bus->last_digin = 0;
	bus->last_stepgen = 0;
	bus->last_encoder = 0;
	/* clear the slot data structures (part of the bus struct) */
	for ( slotnum = 0 ; slotnum < NUM_SLOTS ; slotnum ++ ) {
	    /* clear slot valid flag in bus struct */
	    bus->slot_valid[slotnum] = 0;
	    /* point to slot struct */
	    slot = &(bus->slot_data[slotnum]);
	    /* clear stuff */
	    slot->id = 0;
	    slot->ver = 0;
	    slot->slot_base = slotnum * SLOT_SIZE;
	    slot->port_addr = port_addr[busnum];
	    slot->first_rd = 31;
	    slot->last_rd = 0;
	    slot->first_wr = 31;
	    slot->last_wr = 0;
	    /* clear EPP read and write caches */
	    for ( n = 0 ; n < 32 ; n++ ) {
		slot->rd_buf[n] = 0;
		slot->wr_buf[n] = 0;
	    }
	    /* clear function pointers */
	    slot->num_rd_functs = 0;
	    slot->num_wr_functs = 0;
	    for ( n = 0 ; n < MAX_FUNCT ; n++ ) {
		slot->rd_functs[n] = NULL;
		slot->wr_functs[n] = NULL;
	    }
	    slot->digout = NULL;
            slot->digin = NULL;
            slot->stepgen = NULL;
            slot->encoder = NULL;
	}    
	/* scan the bus */
	for ( slotnum = 0 ; slotnum < NUM_SLOTS ; slotnum ++ ) {
	    /* point to slot struct */
	    slot = &(bus->slot_data[slotnum]);
	    /* rv1 is used to flag errors that fail one bus */
	    rv1 = 0;
	    rtapi_print_msg(RTAPI_MSG_INFO, "PPMC: slot %d: ", slotnum);
	
	    /* check slot */
	    idcode = SelRead(slot->slot_base+SLOT_ID_OFFSET, slot->port_addr);
	    if ( ( idcode == 0 ) || ( idcode == 0xFF ) ) {
		slot->id = 0;
		slot->ver = 0;
		rtapi_print_msg(RTAPI_MSG_INFO, "nothing detected\n");
		ClrTimeout(slot->port_addr);
		/* skip to next slot */
		continue;
	    }
	    rtapi_print_msg(RTAPI_MSG_INFO, "ID code: %02X ", idcode);
	    slot->id = id = idcode & 0xF0;
	    slot->ver = ver = idcode & 0x0F;
	    /* mark slot as occupied */
	    bus->slot_valid[slotnum] = 1;
	    
	    /* do board specific init and configuration */
	    switch ( id ) {
	    case 0x10:
		boards++;
		rtapi_print_msg(RTAPI_MSG_INFO, "PPMC encoder card\n");
		break;
	    case 0x20:
		boards++;
		rtapi_print_msg(RTAPI_MSG_INFO, "PPMC DAC card\n");
		break;
	    case 0x30:
		boards++;
		rtapi_print_msg(RTAPI_MSG_INFO, "PPMC Digital I/O card\n");
		break;
	    case 0x40:
		boards++;
		rtapi_print_msg(RTAPI_MSG_INFO, "Univ. Stepper Controller\n");
		rv1 += export_UxC_digin(slot, bus);
		rv1 += export_UxC_digout(slot, bus);
		rv1 += export_USC_stepgen(slot, bus);
                rv1 += export_UxC_encoders(slot, bus);
		/* the USC occupies two slots, so skip the second one */
		slotnum++;
		break;
	    case 0x50:
		boards++;
		rtapi_print_msg(RTAPI_MSG_INFO, "Univ. PWM Controller\n");
		rv1 += export_UxC_digin(slot, bus);
		rv1 += export_UxC_digout(slot, bus);
                rv1 += export_UxC_encoders(slot, bus);
                /* the UPC occupies two slots, so skip the second one */
		slotnum++;
		break;
	    default:
		rtapi_print_msg(RTAPI_MSG_ERR, "Unknown! (ERROR)\n");
		/* mark slot as empty */
		bus->slot_valid[slotnum] = 0;
		/* mark bus failed */
		rv1 = -1;
		break;
            }
	} /* end of slot loop */
	if ( rv1 != 0 ) {
	    /* error during slot scan, already printed */
	    rv = -1;
	    /* skip to next bus */
	    continue;
	}
	if ( boards == 0 ) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PPMC: ERROR: no boards found on bus %d, port %04X\n",
		busnum, port_addr[busnum] );
	    rv = -1;
	    /* skip to next bus */
	    continue;
	}
	/* export functions */
	rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.read", busnum);
	rv1 = hal_export_funct(buf, read_all, &(bus_array[busnum]),
	    1, 0, comp_id);
	if (rv1 != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PPMC: ERROR: read funct export failed\n");
	    rv = -1;
	    /* skip to next bus */
	    continue;
	}
	rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.write", busnum);
	rv1 = hal_export_funct(buf, write_all, &(bus_array[busnum]),
	    1, 0, comp_id);
	if (rv1 != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"PPMC: ERROR: write funct export failed\n");
	    rv = -1;
	    /* skip to next bus */
	    continue;
	}
	/* save pointer to bus data */
	bus_array[busnum] = bus;
	rtapi_print_msg(RTAPI_MSG_INFO, "PPMC: bus %d complete\n", busnum);
    }
    /* restore saved message level */
    rtapi_set_msg_level(msg);
    /* final check for errors */
    if ( rv != 0 ) {
	/* something went wrong, cleanup and exit */
	rtapi_print_msg(RTAPI_MSG_ERR, "PPMC: shutting down\n");
	for ( busnum = 0 ; busnum < MAX_BUS ; busnum++ ) {
	    /* check to see if memory was allocated for bus */
	    if ( bus_array[busnum] != NULL ) {
		/* save ptr to memory block */
		bus = bus_array[busnum];
		/* mark it invalid so RT code won't access */
		bus_array[busnum] = NULL;
		/* and free the block */
		kfree(bus);
	    }
	}
	/* disconnect from HAL */
	hal_exit(comp_id);
	return -1;
    }    
    rtapi_print_msg(RTAPI_MSG_INFO, "PPMC: driver installed\n");
    return 0;
}

void rtapi_app_exit(void)
{
    int busnum;
    bus_data_t *bus;

    rtapi_print_msg(RTAPI_MSG_ERR, "PPMC: shutting down\n");
    for ( busnum = 0 ; busnum < MAX_BUS ; busnum++ ) {
	/* check to see if memory was allocated for bus */
	if ( bus_array[busnum] != NULL ) {
	    /* save ptr to memory block */
	    bus = bus_array[busnum];
	    /* mark it invalid so RT code won't access */
	    bus_array[busnum] = NULL;
	    /* FIXME should we have code here that turns
	       off all outputs, etc?  If so, it won't be
	       pretty, because just like the regular code
	       it will be different for every board. */
	    /* and free the block */
	    kfree(bus);
	}
    }
    /* disconnect from HAL */
    hal_exit(comp_id);
}

/***********************************************************************
*                         REALTIME FUNCTIONS                           *
************************************************************************/

static void read_all(void *arg, long period)
{
    bus_data_t *bus;
    slot_data_t *slot;
    int slotnum, functnum;
    unsigned char n, eppaddr;

    /* get pointer to bus data structure */
    bus = *(bus_data_t **)(arg);
    /* test to make sure it hasn't been freed */
    if ( bus == NULL ) {
	return;
    }
    /* loop thru all slots */
    for ( slotnum = 0 ; slotnum < NUM_SLOTS ; slotnum++ ) {
	/* check for anthing in slot */
	if ( bus->slot_valid[slotnum] ) {
	    /* point at slot data */
	    slot = &(bus->slot_data[slotnum]);
	    /* FIXME - code to latch encoders probably needs to go here */
	    SelWrt(0x20, slot->slot_base + ENCRATE, slot->port_addr);    /* software generated encoder latch */
	    SelWrt(0x20, slot->slot_base + ENCRATE, slot->port_addr);    /* a little extra delay*/
//	    SelWrt(0x20, slot->slot_base + ENCRATE, slot->port_addr);
	    SelWrt(0x00, slot->slot_base + ENCRATE, slot->port_addr);    /* and return to normal */
	       /* fetch data from EPP to cache */
	    if ( slot->first_rd <= slot->last_rd ) {
		/* need to read some data */
		n = slot->first_rd;
		eppaddr = slot->slot_base + slot->first_rd;
		/* read first byte */
		slot->rd_buf[n++] = SelRead(eppaddr, slot->port_addr);
		/* read the rest */
		while ( n <= slot->last_rd ) {
		    slot->rd_buf[n++] = ReadMore(slot->port_addr);
		}
	    }
	    /* loop thru all functions associated with slot */
	    for ( functnum = 0 ; functnum < slot->num_rd_functs ; functnum++ ) {
		/* call function */
		(slot->rd_functs[functnum])(slot);
	    }
	}
    }
}

static void write_all(void *arg, long period)
{
    bus_data_t *bus;
    slot_data_t *slot;
    int slotnum, functnum;
    unsigned char n, eppaddr;

    /* get pointer to bus data structure */
    bus = *(bus_data_t **)(arg);
    /* test to make sure it hasn't been freed */
    if ( bus == NULL ) {
	return;
    }
    /* loop thru all slots */
    for ( slotnum = 0 ; slotnum < NUM_SLOTS ; slotnum++ ) {
	/* check for anthing in slot */
	if ( bus->slot_valid[slotnum] ) {
	    /* point at slot data */
	    slot = &(bus->slot_data[slotnum]);
	    /* loop thru all functions associated with slot */
	    for ( functnum = 0 ; functnum < slot->num_wr_functs ; functnum++ ) {
		/* call function */
		(slot->wr_functs[functnum])(slot);
	    }
	    /* write data from cache to EPP */
	    if ( slot->first_wr <= slot->last_wr ) {
		/* need to write some data */
		n = slot->first_wr;
		eppaddr = slot->slot_base + slot->first_wr;
		/* write first byte */
		SelWrt(slot->wr_buf[n++], eppaddr, slot->port_addr);
		/* write the rest */
		while ( n <= slot->last_wr ) {
		    WrtMore(slot->wr_buf[n++], slot->port_addr);
		}
	    }
	    /* FIXME - do we need something here to strobe data? */
	}
    }
}

static void read_digins(slot_data_t *slot)
{
    int b;
    unsigned char indata, mask;

    /* read the first 8 inputs */
    indata = slot->rd_buf[UxC_DINA];
    /* split the bits into 16 variables (8 regular, 8 inverted) */
    b = 0;
    mask = 0x01;
    while ( b < 8 ) {
	*(slot->digin[b].data) = indata & mask;
	*(slot->digin[b].data_not) = !(indata & mask);
	mask <<= 1;
	b++;
    }
    /* read the next 8 inputs */
    indata = slot->rd_buf[UxC_DINB];
    /* and split them too */
    mask = 0x01;
    while ( b < 16 ) {
	*(slot->digin[b].data) = indata & mask;
	*(slot->digin[b].data_not) = !(indata & mask);
	mask <<= 1;
	b++;
    }
}

static void write_digouts(slot_data_t *slot)
{
    int b;
    unsigned char outdata, mask;

    outdata = 0x00;
    mask = 0x01;
    /* assemble output byte from 8 source variables */
    for (b = 0; b < 8; b++) {
	/* get the data, add to output byte */
	if ((*(slot->digout[b].data)) && (!slot->digout[b].invert)) {
	    outdata |= mask;
	}
	if ((!*(slot->digout[b].data)) && (slot->digout[b].invert)) {
	    outdata |= mask;
	}
	mask <<= 1;
    }
    /* write it to the hardware (cache) */
    slot->wr_buf[UxC_DOUTA] = outdata;
}

static void read_encoders(slot_data_t *slot)
{
    int i, byteindex;
    hal_u8_t mask = 0x01, indextemp;

    union pos_tag {
        signed long l;
        struct byte_tag {
            signed char b0;
            signed char b1;
            signed char b2;
            signed char b3;
        } byte;
    } pos, oldpos;
    
    indextemp = (hal_u8_t)(slot->rd_buf[ENCISR]);
    byteindex = ENCCNT0;        /* first encoder count register */
    for (i = 0; i < 4; i++) {
        oldpos.l = slot->encoder[i].oldreading;
	pos.byte.b0 = (signed char)slot->rd_buf[byteindex++];
	pos.byte.b1 = (signed char)slot->rd_buf[byteindex++];
	pos.byte.b2 = (signed char)slot->rd_buf[byteindex++];
        pos.byte.b3 = oldpos.byte.b3;
        /* check for - to + transition */
        if ((oldpos.byte.b2 < 0) && (pos.byte.b2 >= 0))
            pos.byte.b3++;
        else 
            if ((oldpos.byte.b2 >= 0) && (pos.byte.b2 < 0))
                pos.byte.b3--;
	slot->encoder[i].oldreading = pos.l;
	*(slot->encoder[i].count) = pos.l;
	*(slot->encoder[i].position) = pos.l / slot->encoder[i].scale;
	*(slot->encoder[i].index) = (((indextemp & mask) == mask) ? 1 : 0);
	mask <<= 1;
    }
}

static void write_stepgens(slot_data_t *slot)
{
    int n, reverse, run;
    unsigned int divisor;
    stepgen_t *sg;
    double bd_max_freq, ch_max_freq, abs_scale, freq;
    unsigned char control_byte;

    /* pulse width cannot be zero (or one, HW bug) */
    if ( slot->stepgen->pulse_width < 2 ) {
	slot->stepgen->pulse_width = 2;
    }
    /* write it to the cache, inverted */
    slot->wr_buf[RATE_WIDTH_0] = 256 - slot->stepgen->pulse_width;
    /* calculate the max frequency, varies with pulse width */
    bd_max_freq = 5000000.0 / slot->stepgen->pulse_width;
    /* setup time cannot be zero or one, see above */
    if ( slot->stepgen->setup_time < 2 ) {
	slot->stepgen->setup_time = 2;
    }
    /* write it to the cache */
    slot->wr_buf[RATE_SETUP_0] = 256 - slot->stepgen->setup_time;
    /* now do the four individual stepgens */
    control_byte = 0;
    for ( n = 0 ; n < 4 ; n++ ) {
	/* point to the specific stepgen */
	sg = &(slot->stepgen->sg[n]);
	/* validate the scale value */
	if ( sg->vel_scale < 0.0 ) {
	    if ( sg->vel_scale > -1e-20 ) {
		/* too small, divide by zero is bad */
		sg->vel_scale = -1.0;
	    }
	    abs_scale = -sg->vel_scale;
	} else {
	    if ( sg->vel_scale < 1e-20 ) {
		sg->vel_scale = 1.0;
	    }
	    abs_scale = sg->vel_scale;
	}
	ch_max_freq = bd_max_freq;
	/* check for user specified max velocity */
	if (sg->max_vel <= 0.0) {
	    /* set to zero if negative, and ignore if zero */
	    sg->max_vel = 0.0;
	} else {
	    /* parameter is non-zero and positive, compare to max_freq */
	    if ( (sg->max_vel * abs_scale) > ch_max_freq) {
		/* parameter is too high, lower it */
		sg->max_vel = ch_max_freq / abs_scale;
	    } else {
		/* lower max_freq to match parameter */
		ch_max_freq = sg->max_vel * abs_scale;
	    }
	}
	/* SWP - changed the sign for "reverse", now, when step output is fed back to
		encoder input, the signs match */
	/* calculate desired frequency */
	freq = *(sg->vel) * sg->vel_scale;
	reverse = 1;
	run = 1;
	/* deal with negative */
	if ( freq < 0.0 ) {
	    /* negative */
	    freq = -freq;
	    reverse = 0;
	}
	/* apply limits */
	if ( freq > ch_max_freq ) {
	    freq = ch_max_freq;
	    divisor = 10000000.0 / freq;
	} else if ( freq < (10000000.0/16777214.0) ) {
	    /* frequency would result in a divisor greater than 2^24-2 */
	    freq = 0.0;
	    divisor = 16777215;
	    /* only way to get zero is to turn it off */
	    run = 0;
	} else {
	    /* calculate divisor, round to nearest instead of truncating */
	    divisor = ( 10000000.0 / freq ) + 0.5;
	    /* calculate actual frequency (due to divisor roundoff) */
	    freq = 10000000.0 / divisor;
	}
	/* save the frequency */
	if ( reverse == 0 ) {	    
	    sg->freq = freq;
	} else {
	    sg->freq = -freq;
	}
	/* set run and dir bits in the control byte */
	control_byte >>= 2;
	if ( run ) {
	    control_byte |= 0x80;
	}
	if ( reverse ) {
	    control_byte |= 0x40;
	}
	/* write divisor to the cache */
	slot->wr_buf[RATE_GEN_0+(n*3)] = divisor & 0xff;
	divisor >>= 8;
	slot->wr_buf[RATE_GEN_0+(n*3)+1] = divisor & 0xff;
	divisor >>= 8;
	slot->wr_buf[RATE_GEN_0+(n*3)+2] = divisor & 0xff;
    }
    /* write control byte to cache */
    slot->wr_buf[RATE_CTRL_0] = control_byte;
}

/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

/* these functions are used to register a runtime function to be called
   by either read_all or write_all.  'min_addr' and 'max_addr' define
   the range of EPP addresses that the function needs.  All addresses
   needed by all functions associated with the slot woll be sequentially
   be read into the rd_buf cache (or written from the wr_buf cache) 
   by read_all or write_all respectively, to minimize the number of 
   slow inb and outb operations needed.
*/

static int add_rd_funct(slot_funct_t *funct, slot_data_t *slot,
			int min_addr, int max_addr )
{
    if ( slot->num_rd_functs >= MAX_FUNCT ) {
	rtapi_print_msg(RTAPI_MSG_ERR, 
	    "PPMC: ERROR: too many read functions\n");
	return -1;
    }
    slot->rd_functs[slot->num_rd_functs++] = funct;
    if ( slot->first_rd > min_addr ) {
	slot->first_rd = min_addr;
    }
    if ( slot->last_rd < max_addr ) {
	slot->last_rd = max_addr;
    }
    return 0;
}

static int add_wr_funct(slot_funct_t *funct, slot_data_t *slot,
			int min_addr, int max_addr )
{
    if ( slot->num_wr_functs >= MAX_FUNCT ) {
	rtapi_print_msg(RTAPI_MSG_ERR, 
	    "PPMC: ERROR: too many write functions\n");
	return -1;
    }
    slot->wr_functs[slot->num_wr_functs++] = funct;
    if ( slot->first_wr > min_addr ) {
	slot->first_wr = min_addr;
    }
    if ( slot->last_wr < max_addr ) {
	slot->last_wr = max_addr;
    }
    return 0;
}

static int export_UxC_digin(slot_data_t *slot, bus_data_t *bus)
{
    int retval, n;
    char buf[HAL_NAME_LEN + 2];

    rtapi_print_msg(RTAPI_MSG_INFO, "PPMC:  exporting digital inputs\n");

    /* do hardware init */

    /* allocate shared memory for the digital input data */
    slot->digin = hal_malloc(16 * sizeof(din_t));
    if (slot->digin == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PPMC: ERROR: hal_malloc() failed\n");
	return -1;
    }
    for ( n = 0 ; n < 16 ; n++ ) {
	/* export pins for input data */
	rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.din.%02d.in",
	    bus->busnum, bus->last_digin);
	retval = hal_pin_bit_new(buf, HAL_WR, 
	    &(slot->digin[n].data), comp_id);
	if (retval != 0) {
	    return retval;
	}
	rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.din.%02d.in-not",
	    bus->busnum, bus->last_digin);
	retval = hal_pin_bit_new(buf, HAL_WR, 
	    &(slot->digin[n].data_not), comp_id);
	if (retval != 0) {
	    return retval;
	}
	/* increment number to prepare for next output */
	bus->last_digin++;
    }
    add_rd_funct(read_digins, slot, UxC_DINA, UxC_DINB);
    return 0;
}

static int export_UxC_digout(slot_data_t *slot, bus_data_t *bus)
{
    int retval, n;
    char buf[HAL_NAME_LEN + 2];

    rtapi_print_msg(RTAPI_MSG_INFO, "PPMC:  exporting digital outputs\n");

    /* do hardware init */
    /* turn off all outputs */
    SelWrt(0, slot->slot_base+UxC_DOUTA, slot->port_addr);

    /* allocate shared memory for the digital output data */
    slot->digout = hal_malloc(8 * sizeof(dout_t));
    if (slot->digout == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PPMC: ERROR: hal_malloc() failed\n");
	return -1;
    }
    for ( n = 0 ; n < 8 ; n++ ) {
	/* export pin for output data */
	rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.dout.%02d.out",
	    bus->busnum, bus->last_digout);
	retval = hal_pin_bit_new(buf, HAL_RD, &(slot->digout[n].data), comp_id);
	if (retval != 0) {
	    return retval;
	}
	/* export parameter for inversion */
	rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.dout.%02d.invert",
	    bus->busnum, bus->last_digout);
	retval = hal_param_bit_new(buf, HAL_WR, &(slot->digout[n].invert), comp_id);
	if (retval != 0) {
	    return retval;
	}
	/* increment number to prepare for next output */
	bus->last_digout++;
    }
    add_wr_funct(write_digouts, slot, UxC_DOUTA, UxC_DOUTA);
    return 0;
}

static int export_USC_stepgen(slot_data_t *slot, bus_data_t *bus)
{
    int retval, n;
    char buf[HAL_NAME_LEN + 2];
    stepgen_t *sg;

    rtapi_print_msg(RTAPI_MSG_INFO, "PPMC:  exporting step generators\n");

    /* do hardware init */

    /* allocate shared memory for the digital output data */
    slot->stepgen = hal_malloc(sizeof(stepgens_t));
    if (slot->stepgen == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "PPMC: ERROR: hal_malloc() failed\n");
	return -1;
    }
    /* export params that apply to all four stepgens */
    rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.stepgen.%02d-%02d.setup-time",
	bus->busnum, bus->last_stepgen, bus->last_stepgen+3);
    retval = hal_param_u8_new(buf, HAL_WR, &(slot->stepgen->setup_time), comp_id);
    if (retval != 0) {
	return retval;
    }
    rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.stepgen.%02d-%02d.pulse-width",
	bus->busnum, bus->last_stepgen, bus->last_stepgen+3);
    retval = hal_param_u8_new(buf, HAL_WR, &(slot->stepgen->pulse_width), comp_id);
    if (retval != 0) {
	return retval;
    }
    /* export per-stepgen pins and params */
    for ( n = 0 ; n < 4 ; n++ ) {
	/* pointer to the stepgen struct */
	sg = &(slot->stepgen->sg[n]);
	/* velocity command pin */
	rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.stepgen.%02d.velocity",
	    bus->busnum, bus->last_stepgen);
	retval = hal_pin_float_new(buf, HAL_RD, &(sg->vel), comp_id);
	if (retval != 0) {
	    return retval;
	}
	/* velocity scaling parameter */
	rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.stepgen.%02d.vel-scale",
	    bus->busnum, bus->last_stepgen);
	retval = hal_param_float_new(buf, HAL_WR, &(sg->vel_scale), comp_id);
	if (retval != 0) {
	    return retval;
	}
	/* maximum velocity parameter */
	rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.stepgen.%02d.max-vel",
	    bus->busnum, bus->last_stepgen);
	retval = hal_param_float_new(buf, HAL_WR, &(sg->max_vel), comp_id);
	if (retval != 0) {
	    return retval;
	}
	/* actual frequency parameter */
	rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.stepgen.%02d.freq",
	    bus->busnum, bus->last_stepgen);
	retval = hal_param_float_new(buf, HAL_RD, &(sg->freq), comp_id);
	if (retval != 0) {
	    return retval;
	}
	/* increment number to prepare for next output */
	bus->last_stepgen++;
    }
    add_wr_funct(write_stepgens, slot, RATE_GEN_0, RATE_WIDTH_0);
    return 0;
}
/* Each of the encoders has the following:
    params:
        ppmc.n.encoder.m.scale      float
    pins:
        ppmc.n.encoder.m.position   float
        ppmc.n.encoder.m.counts     s32
        ppmc.n.encoder.m.index      bit
    
    the output value is position=counts * scale

    Additionally, the encoder registers are zeroed, and the mode is set to latch 
 */

static int export_UxC_encoders(slot_data_t *slot, bus_data_t *bus)
{
    int retval, n;
    char buf[HAL_NAME_LEN+2];
    
    rtapi_print_msg(RTAPI_MSG_INFO, "PPMC: exporting encoder pins / params\n");

    /* do hardware init */
    SelWrt(0x00, slot->slot_base+ENCCTRL, slot->port_addr);	/* clear encoder control register */
    SelWrt(0x00, slot->slot_base+ENCRATE, slot->port_addr);	/* make this a non-master (no INTR) */
    SelWrt(0xF0, slot->slot_base+ENCCTRL, slot->port_addr);	/* we'll reset all counters to 0 */
    SelWrt(0x00, slot->slot_base+ENCLOAD, slot->port_addr);	/* clear encoder count load register */
    WrtMore(0x00, slot->port_addr);
    WrtMore(0x00, slot->port_addr);
    ClrTimeout(slot->port_addr);				/* extra delay, just to be sure */
    ClrTimeout(slot->port_addr);				/* extra delay, just to be sure */
    ClrTimeout(slot->port_addr);				/* extra delay, just to be sure */
    SelWrt(0x00, slot->slot_base+ENCCTRL, slot->port_addr);	/* clear encoder control register */

    /* allocate shared memory for the encoder data */
    slot->encoder = hal_malloc(4 * sizeof(encoder_t));
    if (slot->encoder == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "PPMC: ERROR: hal_malloc() failed\n");
        return -1;
    }
    
    /* export per-encoder pins and params */
    for ( n = 0 ; n < 4 ; n++ ) {
        /* scale input parameter */
        rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.encoder.%02d.scale",
                       bus->busnum, bus->last_encoder);
        retval = hal_param_float_new(buf, HAL_WR, &(slot->encoder[n].scale), comp_id);
        if (retval != 0) {
            return retval;
        }
        /* scaled encoder position */
        rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.encoder.%02d.position",
                       bus->busnum, bus->last_encoder);
        retval = hal_pin_float_new(buf, HAL_WR, &(slot->encoder[n].position), comp_id);
        if (retval != 0) {
            return retval;
        }
	/* raw encoder position */
	rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.encoder.%02d.count",
		       bus->busnum, bus->last_encoder);
	retval = hal_pin_s32_new(buf, HAL_WR, &(slot->encoder[n].count), comp_id);
	if (retval != 0) {
		return retval;
	}
	/* encoder index bit */
	rtapi_snprintf(buf, HAL_NAME_LEN, "ppmc.%d.encoder.%02d.index",
		       bus->busnum, bus->last_encoder);
	retval = hal_pin_bit_new(buf, HAL_WR, &(slot->encoder[n].index), comp_id);
	if (retval != 0) {
		return retval;
	}
	/* increment number to prepare for next output */
	bus->last_encoder++;
    }
    add_rd_funct(read_encoders, slot, ENCCNT0, ENCISR);
    return 0;
}

/* utility functions for EPP bus */

/* reset all boards attached to the EPP bus */
static void BusReset(unsigned int port_addr)
{
  rtapi_outb(0,CONTROLPORT(port_addr));
  rtapi_outb(4,CONTROLPORT(port_addr));
  return;
}

/* tests for an EPP bus timeout, and clears it if so */
static int ClrTimeout(unsigned int port_addr)
{
    unsigned char r;

    r = rtapi_inb(STATUSPORT(port_addr));
    
    if  (!(r & 0x01)) {
	return 0;
    }
/* remove after testing */
rtapi_print("EPP Bus Timeout!\n" );
    /* To clear timeout some chips require double read */
    BusReset(port_addr);
    r = rtapi_inb(STATUSPORT(port_addr));
    rtapi_outb(r | 0x01, STATUSPORT(port_addr)); /* Some reset by writing 1 */
    r = rtapi_inb(STATUSPORT(port_addr));

    return !(r & 0x01);
}

/* sets the EPP address and then reads one byte from that address */
static unsigned short SelRead(unsigned char epp_addr, unsigned int port_addr)
{
    unsigned char b;
    
    ClrTimeout(port_addr);
    /* set port direction to output */
    rtapi_outb(0x04,CONTROLPORT(port_addr));
    /* write epp address to port */
    rtapi_outb(epp_addr,ADDRPORT(port_addr));
    /* set port direction to input */
    rtapi_outb(0x24,CONTROLPORT(port_addr));
    /* read data value */
    b = rtapi_inb(DATAPORT(port_addr));
    return b;

}

/* reads one byte from EPP, use only after SelRead, and only 
   when hardware has auto-increment address cntr */
static unsigned short ReadMore(unsigned int port_addr)
{
    unsigned char b;
    b = rtapi_inb(DATAPORT(port_addr));
    return b;

}

/* sets the EPP address and then writes one byte to that address */
static void SelWrt(unsigned char byte, unsigned char epp_addr, unsigned int port_addr)
{
    ClrTimeout(port_addr);
    /* set port direction to output */
    rtapi_outb(0x04,CONTROLPORT(port_addr));
    /* write epp address to port */
    rtapi_outb(epp_addr,ADDRPORT(port_addr));
    /* write data to port */
    rtapi_outb(byte,DATAPORT(port_addr));
    return;
}

/* writes one byte to EPP, use only after SelWrt, and only 
   when hardware has auto-increment address cntr */
static void WrtMore(unsigned char byte, unsigned int port_addr)
{
    rtapi_outb(byte,DATAPORT(port_addr));
    return;
}

