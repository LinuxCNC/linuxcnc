//    Copyright 2005-2008, various authors
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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



