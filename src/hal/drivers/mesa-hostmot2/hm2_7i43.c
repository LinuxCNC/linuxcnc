
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


#include <asm/io.h>

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_math.h"
#include "rtapi_string.h"

#include "hal.h"

#include "hal/drivers/mesa-hostmot2/hostmot2-lowlevel.h"
#include "hal/drivers/mesa-hostmot2/hm2_7i43.h"


static int comp_id;

#ifdef MODULE_INFO
MODULE_INFO(emc2, "component:hm2_7i43:EMC HAL driver for the Mesa Electronics 7i43 EPP Anything IO board with HostMot2 firmware.");
MODULE_INFO(emc2, "license:GPL");
#endif // MODULE_INFO

MODULE_LICENSE("GPL");

// FIXME: support multiple boards
int ioaddr = 0x378;
RTAPI_MP_INT(ioaddr, "The base address of the parallel port.");

int ioaddr_hi = 0;
RTAPI_MP_INT(ioaddr_hi, "The secondary address of the parallel port, used to set EPP mode.\n0 means to use ioaddr + 0x400.");

int epp_wide = 1;
RTAPI_MP_INT(epp_wide, "Set to zero to disable the \"wide EPP mode\".  \"Wide\" mode allows a 16-\nand 32-bit EPP transfers, which can reduce the time spent in the read\nand write functions.  However, this may not work on all EPP parallel\nports.");

int debug_epp = 0;
RTAPI_MP_INT(debug_epp, "Developer/debug use only!  Enable debug logging of most EPP\ntransfers.");


static char *config[HM2_7I43_MAX_BOARDS];
static int num_config_strings = HM2_7I43_MAX_BOARDS;
module_param_array(config, charp, &num_config_strings, S_IRUGO);
MODULE_PARM_DESC(config, "config string for 7i43 boards (see hostmot2(9) manpage)");




//
// this data structure keeps track of all the 7i43 boards found
//

static hm2_7i43_t board[HM2_7I43_MAX_BOARDS];
static int num_boards;




// 
// EPP I/O code
// 

static inline void hm2_7i43_epp_addr8(u8 addr, hm2_7i43_t *board) {
    outb(addr, board->ioaddr + HM2_7I43_EPP_ADDRESS_OFFSET);
    LL_DEBUG(debug_epp, "selected address 0x%02X\n", addr);
}

static inline void hm2_7i43_epp_addr16(u16 addr, hm2_7i43_t *board) {
    outb((addr & 0x00FF), board->ioaddr + HM2_7I43_EPP_ADDRESS_OFFSET);
    outb((addr >> 8),     board->ioaddr + HM2_7I43_EPP_ADDRESS_OFFSET);
    LL_DEBUG(debug_epp, "selected address 0x%04X\n", addr);
}

static inline void hm2_7i43_epp_write(int w, hm2_7i43_t *board) {
    outb(w, board->ioaddr + HM2_7I43_EPP_DATA_OFFSET);
    LL_DEBUG(debug_epp, "wrote data 0x%02X\n", w);
}

static inline int hm2_7i43_epp_read(hm2_7i43_t *board) {
    int val;
    val = inb(board->ioaddr + HM2_7I43_EPP_DATA_OFFSET);
    LL_DEBUG(debug_epp, "read data 0x%02X\n", val);
    return val;
}

static inline u32 hm2_7i43_epp_read32(hm2_7i43_t *board) {
    uint32_t data;

    if (epp_wide) {
	data = inl(board->ioaddr + HM2_7I43_EPP_DATA_OFFSET);
        LL_DEBUG(debug_epp, "read data 0x%08X\n", data);
    } else {
        uint8_t a, b, c, d;
        a = hm2_7i43_epp_read(board);
        b = hm2_7i43_epp_read(board);
        c = hm2_7i43_epp_read(board);
        d = hm2_7i43_epp_read(board);
        data = a + (b<<8) + (c<<16) + (d<<24);
    }

    return data;
}

static inline void hm2_7i43_epp_write32(uint32_t w, hm2_7i43_t *board) {
    if (epp_wide) {
	outl(w, board->ioaddr + HM2_7I43_EPP_DATA_OFFSET);
        LL_DEBUG(debug_epp, "wrote data 0x%08X\n", w);
    } else {
        hm2_7i43_epp_write((w) & 0xFF, board);
        hm2_7i43_epp_write((w >>  8) & 0xFF, board);
        hm2_7i43_epp_write((w >> 16) & 0xFF, board);
        hm2_7i43_epp_write((w >> 24) & 0xFF, board);
    }
}

static inline uint8_t hm2_7i43_epp_read_status(hm2_7i43_t *board) {
    uint8_t val;
    val = inb(board->ioaddr + HM2_7I43_EPP_STATUS_OFFSET);
    LL_DEBUG(debug_epp, "read status 0x%02X\n", val);
    return val;
}

static inline void hm2_7i43_epp_write_status(uint8_t status_byte, hm2_7i43_t *board) {
    outb(status_byte, board->ioaddr + HM2_7I43_EPP_STATUS_OFFSET);
    LL_DEBUG(debug_epp, "wrote status 0x%02X\n", status_byte);
}

static inline void hm2_7i43_epp_write_control(uint8_t control_byte, hm2_7i43_t *board) {
    outb(control_byte, board->ioaddr + HM2_7I43_EPP_CONTROL_OFFSET);
    LL_DEBUG(debug_epp, "wrote control 0x%02X\n", control_byte);
}

// returns TRUE if there's a timeout
static inline int hm2_7i43_epp_check_for_timeout(hm2_7i43_t *board) {
    return (hm2_7i43_epp_read_status(board) & 0x01);
}

static int hm2_7i43_epp_clear_timeout(hm2_7i43_t *board) {
    uint8_t status;

    if (!hm2_7i43_epp_check_for_timeout(board)) {
        return 1;
    }

    /* To clear timeout some chips require double read */
    (void)hm2_7i43_epp_read_status(board);

    // read in the actual status register
    status = hm2_7i43_epp_read_status(board);

    hm2_7i43_epp_write_status(status | 0x01, board);  // Some reset by writing 1
    hm2_7i43_epp_write_status(status & 0xFE, board);  // Others by writing 0

    if (hm2_7i43_epp_check_for_timeout(board)) {
        LL_WARN("failed to clear EPP Timeout!\n");
        return 0;  // fail
    }
    return 1;  // success
}




// 
// these are the low-level i/o functions exported to the hostmot2 driver
//

int hm2_7i43_read(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    int bytes_remaining = size;
    hm2_7i43_t *board = this->private;

    hm2_7i43_epp_addr16(addr | HM2_7I43_ADDR_AUTOINCREMENT, board);

    for (; bytes_remaining > 3; bytes_remaining -= 4) {
        *((u32*)buffer) = hm2_7i43_epp_read32(board);
        buffer += 4;
    }

    for ( ; bytes_remaining > 0; bytes_remaining --) {
        *((u8*)buffer) = hm2_7i43_epp_read(board);
        buffer ++;
    }

    if (hm2_7i43_epp_check_for_timeout(board)) {
        THIS_WARN("EPP timeout on data cycle of read(addr=0x%04x, size=%d)\n", addr, size);
        (*this->io_error) = 1;
        this->needs_reset = 1;
        hm2_7i43_epp_clear_timeout(board);
        return 0;  // fail
    }

    return 1;  // success
}


int hm2_7i43_write(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    int bytes_remaining = size;
    hm2_7i43_t *board = this->private;

    hm2_7i43_epp_addr16(addr | HM2_7I43_ADDR_AUTOINCREMENT, board);

    for (; bytes_remaining > 3; bytes_remaining -= 4) {
        hm2_7i43_epp_write32(*((u32*)buffer), board);
        buffer += 4;
    }

    for ( ; bytes_remaining > 0; bytes_remaining --) {
        hm2_7i43_epp_write(*((u8*)buffer), board);
        buffer ++;
    }

    if (hm2_7i43_epp_check_for_timeout(board)) {
        THIS_WARN("EPP timeout on data cycle of write(addr=0x%04x, size=%d)\n", addr, size);
        (*this->io_error) = 1;
        this->needs_reset = 1;
        hm2_7i43_epp_clear_timeout(board);
        return 0;
    }

    return 1;
}




//
// setup and cleanup code
//


static void hm2_7i43_cleanup(void) {
    int i;

    // NOTE: hal_malloc() doesnt have a matching free

    for (i = 0; i < HM2_7I43_MAX_BOARDS; i ++) {
        hm2_lowlevel_io_t *this = &board[i].llio;
        // if we've initialized the board, reset it now
        if (board[i].io_region1) {
            THIS_INFO("releasing board\n");
            hm2_unregister(this);
            rtapi_release_region(board[i].ioaddr, 8);
            rtapi_release_region(board[i].ioaddr_hi, 4);
        }
    }
}


static int hm2_7i43_setup(void) {
    int i;

    LL_INFO("loading HostMot2 Mesa 7i43 driver version %s\n", HM2_7I43_VERSION);

    // zero the board structs
    memset(board, 0, HM2_7I43_MAX_BOARDS * sizeof(hm2_7i43_t));
    num_boards = 0;

    for (i = 0; i < 1; i ++) {
        hm2_lowlevel_io_t *this;
        int r;

        // FIXME: support multiple boards
        board[i].ioaddr = ioaddr;
        board[i].ioaddr_hi = ioaddr_hi;
        if (board[i].ioaddr_hi == 0) {
            board[i].ioaddr_hi = board[i].ioaddr + 0x400;
        }
        board[i].epp_wide = epp_wide;


        //
        // claim the I/O regions for the parport
        // 

        board[i].io_region1 = rtapi_request_region(board[i].ioaddr, 8, "hm2_7i43");
        if (!board[i].io_region1) {
            LL_ERR("request_region(%x) failed\n", board[i].ioaddr);
            LL_ERR("make sure the kernel module 'parport' is unloaded)\n");
            return -EBUSY;
        }

        board[i].io_region2 = rtapi_request_region(board[i].ioaddr_hi, 4, "hm2_7i43");
        if (!board[i].io_region2) {
            rtapi_release_region(board[i].ioaddr, 8);
            board[i].io_region1 = NULL;
            LL_ERR("request_region(%x) failed\n", board[i].ioaddr);
            LL_ERR("make sure the kernel module 'parport' is unloaded)\n");
            return -EBUSY;
        }


        // set up the parport for EPP
        outb(0x94, board[i].ioaddr_hi + HM2_7I43_ECP_CONTROL_HIGH_OFFSET); // select EPP mode in ECR
        hm2_7i43_epp_write_control(0x04, &board[i]);  // set control lines and input mode
        hm2_7i43_epp_clear_timeout(&board[i]);


        snprintf(board[i].llio.name, HAL_NAME_LEN, "%s.%d", HM2_LLIO_NAME, num_boards);
        board[i].llio.comp_id = comp_id;
        board[i].llio.read = hm2_7i43_read;
        board[i].llio.write = hm2_7i43_write;
        board[i].llio.private = &board[i];
        board[i].llio.num_ioport_connectors = 2;
        board[i].llio.ioport_connector_name[0] = "P4";
        board[i].llio.ioport_connector_name[1] = "P3";
        this = &board[i].llio;

        r = hm2_register(&board[i].llio, config[i]);
        if (r != 0) {
            THIS_ERR(
                "board at (ioaddr=0x%04X, ioaddr_hi=0x%04X, epp_wide %s) not found!\n",
                board[i].ioaddr,
                board[i].ioaddr_hi,
                (board[i].epp_wide ? "ON" : "OFF")
            );
            rtapi_release_region(board[i].ioaddr, 8);
            rtapi_release_region(board[i].ioaddr_hi, 4);
            board[i].io_region1 = NULL;
            return r;
        } else {
            THIS_INFO(
                "board at (ioaddr=0x%04X, ioaddr_hi=0x%04X, epp_wide %s) found\n",
                board[i].ioaddr,
                board[i].ioaddr_hi,
                (board[i].epp_wide ? "ON" : "OFF")
            );

        }
    }

    return 0;
}


int rtapi_app_main(void) {
    int r = 0;

    comp_id = hal_init(HM2_LLIO_NAME);
    if (comp_id < 0) return comp_id;

    r = hm2_7i43_setup();
    if (r) {
        hm2_7i43_cleanup();
        hal_exit(comp_id);
    } else {
        hal_ready(comp_id);
    }

    return r;
}


void rtapi_app_exit(void) {
    hm2_7i43_cleanup();
    LL_INFO("unloaded driver\n");
    hal_exit(comp_id);
}

