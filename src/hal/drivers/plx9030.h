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
 * modify it under the terms of version 2 of the GNU General
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
 ******************************************************************************/

#ifndef _PLX9030_H_
#define _PLX9030_H_


#include "hal.h"


// Vendor and device ID.
#define PLX9030_VENDOR_ID		0x10B5	// PLX.
#define PLX9030_DEVICE_ID		0x9030	// 9030 SMARTarget I/O Accelerator.


typedef struct {
    volatile __u32			pcidr;
    volatile __u16			pcicr;
    volatile __u16			pcisr;
    volatile __u8			pcirev;
    volatile __u8			pciccr[3];
    volatile __u8			pciclsr;
    volatile __u8			pciltr;
    volatile __u8			pcihtr;
    volatile __u8			pcibistr;
    volatile __u32			pcibar0;
    volatile __u32			pcibar1;
    volatile __u32			pcibar2;
    volatile __u32			pcibar3;
    volatile __u32			pcibar4;
    volatile __u32			pcibar5;
    volatile __u32			pcicis;
    volatile __u16			pcisvid;
    volatile __u16			pcisid;
    volatile __u32			pcierbar;
    volatile __u8			capptr;
    volatile __u8			reserved1[3];
    volatile __u32			reserved2;
    volatile __u8			pciilr;
    volatile __u8			pciipr;
    volatile __u8			pcimgr;
    volatile __u8			pcimlr;
    volatile __u8			pmccapid;
    volatile __u8			pmnext;
    volatile __u16			pmc;
    volatile __u16			pmcsr;
    volatile __u8			pmdata;
    volatile __u8			hscntl;
    volatile __u8			hsnext;
    volatile __u16			hscsr;
    volatile __u8			pvpdcntl;
    volatile __u8			pvpdnext;
    volatile __u16			pvpdad;
    volatile __u32			pvpdata;
} volatile Plx9030PciCfgRegMap;


typedef struct {
    volatile __u32			las0rr;
    volatile __u32			las1rr;
    volatile __u32			las2rr;
    volatile __u32			las3rr;
    volatile __u32			eromrr;
    volatile __u32			las0ba;
    volatile __u32			las1ba;
    volatile __u32			las2ba;
    volatile __u32			las3ba;
    volatile __u32			eromba;
    volatile __u32			las0brd;
    volatile __u32			las1brd;
    volatile __u32			las2brd;
    volatile __u32			las3brd;
    volatile __u32			erombrd;
    volatile __u32			cs0base;
    volatile __u32			cs1base;
    volatile __u32			cs2base;
    volatile __u32			cs3base;
    volatile __u16			intcsr;
    volatile __u8			protarea;
    volatile __u8			reserved1;
    volatile __u32			cntrl;
    volatile __u32			gpioc;
    volatile __u32			pmdatasel;
    volatile __u32			pmdatascale;
} volatile Plx9030LocalRegMap;


#endif
