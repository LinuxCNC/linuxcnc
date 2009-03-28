/*************************************************************************

Header for opto22 pci AC5  board driver

Copyright (C) 2008 Chris Morley

*************************************************************************/

#include "hal.h"

/* code assumes that PINS_PER_PORT is <= 24 */
//there are 2 ports on a opto22 pci AC5 card
//number of i/o per board = 48 + 4 LEDS
#define OPTO_NUM_DIGITAL	52

/*************************************************************************
                         Data Structures
*************************************************************************/
typedef struct  DigitalPinsParams {
    // Pins.
    hal_bit_t				*pValue;
    hal_bit_t				*pValueNot;
    // Parameters.
    hal_bit_t				invert;
} DigitalPinsParams;



typedef struct port_t {
	 __u32 				mask;
	struct DigitalPinsParams	io[OPTO_NUM_DIGITAL/2];
} port_t;




/* master board structure */
typedef struct board_data_t {
    struct pci_dev 		*pci_dev;			/* PCI bus info */
    int 			slot;				/* PCI slot number */
    int 			num;				/* HAL board number */
    void 			__iomem *base;			/* base address */
    int 			len;				/* length of address space*/ 
   struct port_t 		port[2];
   

} board_data_t; 

/*************************************************************************
                                Globals
*************************************************************************/

//extern int comp_id;	/* HAL component ID */


/*************************************************************************
                                Functions
*************************************************************************/



