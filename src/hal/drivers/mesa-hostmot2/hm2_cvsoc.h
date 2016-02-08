
//
//    Copyright (C) 2007-2008 Sebastian Kuzminsky
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
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//


#define HM2_SOCFPGA_VERSION "0.8"

#define HM2_LLIO_NAME "hm2_cvsoc"


#define HM2_SOC_MAX_BOARDS  1


//
// Programming variables
//
// Note: just for the nano / atlas board initially
//


/* how long should we wait for DONE when programming socfpga systems */
#define DONE_WAIT_CVSOCFPGA			2000 // :-)

// the pci dev would corrospond to the uio dev in the soc system.
// the only unique thing the uio device provides is easy access to the hardware memory
// through /dev/uio0 device.
// A design practice this is is dscuraged for future designs by Denx who provides u-boot
// among others.   
// Therefore there is a solid reason for creating an pci --> uio struct containing whats left from there.
// And adding a dts_device_id for all devices that can benefit from utilising device-tree info.
// To have a forward way towards partial fpga reconfiguration data structures relating to device tree overlays.
// could come in handy. Once the soc / embedded systems migrate to 4.x+ kernels.


typedef struct {
//    struct dts_dev *dev;
    void __iomem *base;
    int len;
    unsigned long ctrl_base_addr;
    unsigned long data_base_addr;
    hm2_lowlevel_io_t llio;
} hm2_cvsoc_t;

struct compatible {
        char vendor[32];
        char name[32];
};

struct dts_device_id {
    unsigned short address_width;           //address width /
    unsigned short clocks;                  // clocks /
    struct compatible compatible;
    unsigned short data_width;       /* (class,subclass,prog-if) triplet */
    char name[32];
    unsigned long reg[3];
};


/*
typedef struct {
    union {
        u8 tp8[64 * 1024];
        u32 tp32[16 * 1024];
    } test_pattern;

    hm2_lowlevel_io_t llio;
} hm2_cvsoc_t;
*/
volatile void *virtual_base;
