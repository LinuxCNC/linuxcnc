/******************************************************************************
 *
 * Copyright (C) 2005 Peter G. Vavaroutsos <pete AT vavaroutsos DOT com>
 *
 * $RCSfile$
 * $Author$
 * $Locker$
 * $Revision$
 * $State$
 * $Date$
 *
 * Hardware register defines for the PLX 9030 PCI target chip.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU General
 * Public License as published by the Free Software Foundation.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA
 *
 * THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
 * ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
 * TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
 * harming persons must have provisions for completely removing power
 * from all motors, etc, before persons enter any danger area.  All
 * machinery must be designed to comply with local and national safety
 * codes, and the authors of this software can not, and do not, take
 * any responsibility for such compliance.
 *
 * This code was written as part of the EMC HAL project.  For more
 * information, go to www.linuxcnc.org.
 *
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2005/07/12 18:26:41  petev
 * Initial revision.
 *
 *
 ******************************************************************************/

#ifndef _PLX9030_H_
#define _PLX9030_H_


#include "hal.h"


// Vendor and device ID.
#define PLX9030_VENDOR_ID		0x10B5	// PLX.
#define PLX9030_DEVICE_ID		0x9030	// 9030 SMARTarget I/O Accelerator.


typedef struct {
    hal_u32_t				pcidr;
    hal_u16_t				pcicr;
    hal_u16_t				pcisr;
    hal_u8_t				pcirev;
    hal_u8_t				pciccr[3];
    hal_u8_t				pciclsr;
    hal_u8_t				pciltr;
    hal_u8_t				pcihtr;
    hal_u8_t				pcibistr;
    hal_u32_t				pcibar0;
    hal_u32_t				pcibar1;
    hal_u32_t				pcibar2;
    hal_u32_t				pcibar3;
    hal_u32_t				pcibar4;
    hal_u32_t				pcibar5;
    hal_u32_t				pcicis;
    hal_u16_t				pcisvid;
    hal_u16_t				pcisid;
    hal_u32_t				pcierbar;
    hal_u8_t				capptr;
    hal_u8_t				reserved1[3];
    hal_u32_t				reserved2;
    hal_u8_t				pciilr;
    hal_u8_t				pciipr;
    hal_u8_t				pcimgr;
    hal_u8_t				pcimlr;
    hal_u8_t				pmccapid;
    hal_u8_t				pmnext;
    hal_u16_t				pmc;
    hal_u16_t				pmcsr;
    hal_u8_t				pmdata;
    hal_u8_t				hscntl;
    hal_u8_t				hsnext;
    hal_u16_t				hscsr;
    hal_u8_t				pvpdcntl;
    hal_u8_t				pvpdnext;
    hal_u16_t				pvpdad;
    hal_u32_t				pvpdata;
} volatile Plx9030PciCfgRegMap;


typedef struct {
    hal_u32_t				las0rr;
    hal_u32_t				las1rr;
    hal_u32_t				las2rr;
    hal_u32_t				las3rr;
    hal_u32_t				eromrr;
    hal_u32_t				las0ba;
    hal_u32_t				las1ba;
    hal_u32_t				las2ba;
    hal_u32_t				las3ba;
    hal_u32_t				eromba;
    hal_u32_t				las0brd;
    hal_u32_t				las1brd;
    hal_u32_t				las2brd;
    hal_u32_t				las3brd;
    hal_u32_t				erombrd;
    hal_u32_t				cs0base;
    hal_u32_t				cs1base;
    hal_u32_t				cs2base;
    hal_u32_t				cs3base;
    hal_u16_t				intcsr;
    hal_u8_t				protarea;
    hal_u8_t				reserved1;
    hal_u32_t				cntrl;
    hal_u32_t				gpioc;
    hal_u32_t				pmdatasel;
    hal_u32_t				pmdatascale;
} volatile Plx9030LocalRegMap;


#endif
