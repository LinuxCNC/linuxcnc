
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


#define HM2_5I20_VERSION "0.4"

#define HM2_LLIO_NAME "hm2_5i20"


#define HM2_5I20_MAX_BOARDS  8




//
// the LAS?BRD registers are in the PLX 9030
//
// The HostMot2 firmware needs the #READY bit (0x2) set in order to work,
// but the EEPROMs on some older 5i20 cards dont set them right.
// The driver detects this problem and fixes it up.
//

#define LAS0BRD_OFFSET 0x28
#define LAS1BRD_OFFSET 0x2C
#define LAS2BRD_OFFSET 0x30
#define LAS3BRD_OFFSET 0x34

#define LASxBRD_READY 0x2




/* I/O registers */
#define CTRL_STAT_OFFSET	0x0054	/* 9030 GPIO register (region 1) */

 /* bit number in 9030 GPIO register */
#define GPIO_3_MASK		(1<<11)	/* GPIO 3 */
#define DONE_MASK		(1<<11)	/* GPIO 3 */
#define _INIT_MASK		(1<<14)	/* GPIO 4 */
#define _LED_MASK		(1<<17)	/* GPIO 5 */
#define GPIO_6_MASK		(1<<20)	/* GPIO 6 */
#define _WRITE_MASK		(1<<23)	/* GPIO 7 */
#define _PROGRAM_MASK		(1<<26)	/* GPIO 8 */




typedef struct {
    struct pci_dev *dev;
    void __iomem *base;
    int len;
    unsigned long ctrl_base_addr;
    unsigned long data_base_addr;
    hm2_lowlevel_io_t llio;
} hm2_5i20_t;

