
//
//    Copyright (C) 2013 Sebastian Kuzminsky
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
#include "hal/drivers/epp.h"

#include "hal/drivers/mesa-hostmot2/bitfile.h"
#include "hal/drivers/mesa-hostmot2/hostmot2-lowlevel.h"
#include "hal/drivers/mesa-hostmot2/hm2_7i43.h"


static int comp_id;

#ifdef MODULE_INFO
MODULE_INFO(linuxcnc, "component:hm2_7i43:LinuxCNC HAL driver for the Mesa Electronics 7i43 EPP Anything IO board with HostMot2 firmware.");
MODULE_INFO(linuxcnc, "license:GPL");
#endif // MODULE_INFO

MODULE_LICENSE("GPL");

static int ioaddr[HM2_7I43_MAX_BOARDS] = { 0x378, 0x3f8, [2 ... (HM2_7I43_MAX_BOARDS-1)] = 0 };
static int num_ioaddrs = HM2_7I43_MAX_BOARDS;
module_param_array(ioaddr, int, &num_ioaddrs, S_IRUGO);
MODULE_PARM_DESC(ioaddr, "base address of the parallel port(s) (see hm2_7i43(9) manpage)");

static int ioaddr_hi[HM2_7I43_MAX_BOARDS] = { [0 ... (HM2_7I43_MAX_BOARDS-1)] = 0 };
static int num_ioaddr_his = HM2_7I43_MAX_BOARDS;
module_param_array(ioaddr_hi, int, &num_ioaddr_his, S_IRUGO);
MODULE_PARM_DESC(ioaddr_hi, "secondary address of the parallel port(s) (see hm2_7i43(9) manpage)");

static int epp_wide[HM2_7I43_MAX_BOARDS] = { [0 ... (HM2_7I43_MAX_BOARDS-1)] = 1 };
static int num_epp_wides = HM2_7I43_MAX_BOARDS;
module_param_array(epp_wide, int, &num_epp_wides, S_IRUGO);
MODULE_PARM_DESC(epp_wide, "set to 0 to disable wide EPP mode (see (hm2_7i43(9) manpage)");

int debug_epp = 0;
RTAPI_MP_INT(debug_epp, "Developer/debug use only!  Enable debug logging of most EPP\ntransfers.");

static char *config[HM2_7I43_MAX_BOARDS];
static int num_config_strings = HM2_7I43_MAX_BOARDS;
module_param_array(config, charp, &num_config_strings, S_IRUGO);
MODULE_PARM_DESC(config, "config string(s) for the 7i43 board(s) (see hostmot2(9) manpage)");




//
// this data structure keeps track of all the 7i43 boards found
//

static hm2_7i43_t board[HM2_7I43_MAX_BOARDS];
static int num_boards;




//
// misc generic helper functions
//

// FIXME: this is bogus
static void hm2_7i43_nanosleep(unsigned long int nanoseconds) {
    long int max_ns_delay;

    max_ns_delay = rtapi_delay_max();

    while (nanoseconds > max_ns_delay) {
        rtapi_delay(max_ns_delay);
        nanoseconds -= max_ns_delay;
    }

    rtapi_delay(nanoseconds);
}




// 
// these are the low-level i/o functions exported to the hostmot2 driver
//

int hm2_7i43_read(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    int bytes_remaining = size;
    hm2_7i43_t *board = this->private;

    epp_addr16(addr | HM2_7I43_ADDR_AUTOINCREMENT, &board->epp_port);

    for (; bytes_remaining > 3; bytes_remaining -= 4) {
        *((u32*)buffer) = epp_read32(&board->epp_port);
        buffer += 4;
    }

    for ( ; bytes_remaining > 0; bytes_remaining --) {
        *((u8*)buffer) = epp_read(&board->epp_port);
        buffer ++;
    }

    if (epp_check_for_timeout(&board->epp_port)) {
        THIS_PRINT("EPP timeout on data cycle of read(addr=0x%04x, size=%d)\n", addr, size);
        (*this->io_error) = 1;
        this->needs_reset = 1;
        epp_clear_timeout(&board->epp_port);
        return 0;  // fail
    }

    return 1;  // success
}




int hm2_7i43_write(hm2_lowlevel_io_t *this, u32 addr, void *buffer, int size) {
    int bytes_remaining = size;
    hm2_7i43_t *board = this->private;

    epp_addr16(addr | HM2_7I43_ADDR_AUTOINCREMENT, &board->epp_port);

    for (; bytes_remaining > 3; bytes_remaining -= 4) {
        epp_write32(*((u32*)buffer), &board->epp_port);
        buffer += 4;
    }

    for ( ; bytes_remaining > 0; bytes_remaining --) {
        epp_write(*((u8*)buffer), &board->epp_port);
        buffer ++;
    }

    if (epp_check_for_timeout(&board->epp_port)) {
        THIS_PRINT("EPP timeout on data cycle of write(addr=0x%04x, size=%d)\n", addr, size);
        (*this->io_error) = 1;
        this->needs_reset = 1;
        epp_clear_timeout(&board->epp_port);
        return 0;
    }

    return 1;
}




int hm2_7i43_program_fpga(hm2_lowlevel_io_t *this, const bitfile_t *bitfile) {
    int orig_debug_epp = debug_epp;  // we turn off EPP debugging for this part...
    hm2_7i43_t *board = this->private;
    int64_t start_time, end_time;
    int i;
    const u8 *firmware = bitfile->e.data;


    //
    // send the firmware
    //

    debug_epp = 0;
    start_time = rtapi_get_time();

    // select the CPLD's data address
    epp_addr8(0, &board->epp_port);

    for (i = 0; i < bitfile->e.size; i ++, firmware ++) {
        epp_write(bitfile_reverse_bits(*firmware), &board->epp_port);
    }

    end_time = rtapi_get_time();
    debug_epp = orig_debug_epp;


    // see if it worked
    if (epp_check_for_timeout(&board->epp_port)) {
        THIS_PRINT("EPP Timeout while sending firmware!\n");
        return -EIO;
    }


    //
    // brag about how fast it was
    //

    {
        uint32_t duration_ns;

        duration_ns = (uint32_t)(end_time - start_time);

        if (duration_ns != 0) {
            THIS_INFO(
                "%d bytes of firmware sent (%u KB/s)\n",
                bitfile->e.size,
                (uint32_t)(((double)bitfile->e.size / ((double)duration_ns / (double)(1000 * 1000 * 1000))) / 1024)
            );
        }
    }


    return 0;
}




// return 0 if the board has been reset, -errno if not
int hm2_7i43_reset(hm2_lowlevel_io_t *this) {
    hm2_7i43_t *board = this->private;
    uint8_t byte;


    //
    // this resets the FPGA *only* if it's currently configured with the
    // HostMot2 or GPIO firmware
    //

    epp_addr16(0x7F7F, &board->epp_port);
    epp_write(0x5A, &board->epp_port);
    epp_addr16(0x7F7F, &board->epp_port);
    epp_write(0x5A, &board->epp_port);


    // 
    // this code resets the FPGA *only* if the CPLD is in charge of the
    // parallel port
    //

    // select the control register
    epp_addr8(1, &board->epp_port);

    // bring the Spartan3's PROG_B line low for 1 us (the specs require 300-500 ns or longer)
    epp_write(0x00, &board->epp_port);
    hm2_7i43_nanosleep(1000);

    // bring the Spartan3's PROG_B line high and wait for 2 ms before sending firmware (required by spec)
    epp_write(0x01, &board->epp_port);
    hm2_7i43_nanosleep(2 * 1000 * 1000);

    // make sure the FPGA is not asserting its /DONE bit
    byte = epp_read(&board->epp_port);
    if ((byte & 0x01) != 0) {
        LL_PRINT("/DONE is not low after CPLD reset!\n");
        return -EIO;
    }

    return 0;
}




//
// setup and cleanup code
//


static void hm2_7i43_cleanup(void) {
    int i;

    // NOTE: hal_malloc() doesnt have a matching free

    for (i = 0; i < num_boards; i ++) {
        hm2_lowlevel_io_t *this = &board[i].llio;
        THIS_PRINT("releasing board\n");
        hm2_unregister(this);
        epp_release(&board[i].epp_port);
    }
}


static int hm2_7i43_setup(void) {
    int i;

    LL_PRINT("loading HostMot2 Mesa 7i43 driver version %s\n", HM2_7I43_VERSION);

    // zero the board structs
    memset(board, 0, HM2_7I43_MAX_BOARDS * sizeof(hm2_7i43_t));
    num_boards = 0;

    for (i = 0; i < num_config_strings; i ++) {
        hm2_lowlevel_io_t *this;
        int r;

        r = epp_get(&board[i].epp_port, ioaddr[i], ioaddr_hi[i], epp_wide[i]);
        if (r != 0) {
            LL_ERR(
                "failed to get parport at (ioaddr=0x%04X, ioaddr_hi=0x%04X, epp_wide %s)!\n",
                ioaddr[i],
                ioaddr_hi[i],
                (epp_wide[i] ? "ON" : "OFF")
            );
            return r;
        }

        // select the device and tell it to make itself ready for io
        epp_write_control(0x04, &board[i].epp_port);  // set control lines and input mode
        epp_clear_timeout(&board[i].epp_port);

        rtapi_snprintf(board[i].llio.name, sizeof(board[i].llio.name), "%s.%d", HM2_LLIO_NAME, i);
        board[i].llio.comp_id = comp_id;

        board[i].llio.read = hm2_7i43_read;
        board[i].llio.write = hm2_7i43_write;
        board[i].llio.program_fpga = hm2_7i43_program_fpga;
        board[i].llio.reset = hm2_7i43_reset;

        board[i].llio.num_ioport_connectors = 2;
        board[i].llio.pins_per_connector = 24;
        board[i].llio.ioport_connector_name[0] = "P4";
        board[i].llio.ioport_connector_name[1] = "P3";
        board[i].llio.num_leds = 8;
        board[i].llio.private = &board[i];

        this = &board[i].llio;


        //
        // now we want to detect if this 7i43 has the big FPGA or the small one
        // 3s200tq144 for the small board
        // 3s400tq144 for the big
        //

        // make sure the CPLD is in charge of the parallel port
        hm2_7i43_reset(&board[i].llio);

        //  select CPLD data register
        epp_addr8(0, &board[i].epp_port);

        if (epp_read(&board[i].epp_port) & 0x01) {
            board[i].llio.fpga_part_number = "3s400tq144";
        } else {
            board[i].llio.fpga_part_number = "3s200tq144";
        }
        THIS_DBG("detected FPGA '%s'\n", board[i].llio.fpga_part_number);


        r = hm2_register(&board[i].llio, config[i]);
        if (r != 0) {
            THIS_ERR(
                "board at (ioaddr=0x%04X, ioaddr_hi=0x%04X, epp_wide %s) not found!\n",
                ioaddr[i],
                ioaddr_hi[i],
                (epp_wide[i] ? "ON" : "OFF")
            );

            epp_release(&board[i].epp_port);
            return r;
        }

        THIS_PRINT(
            "board at (ioaddr=0x%04X, ioaddr_hi=0x%04X, epp_wide %s) found\n",
            ioaddr[i],
            ioaddr_hi[i],
            (epp_wide[i] ? "ON" : "OFF")
        );

        num_boards ++;
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
    hal_exit(comp_id);
    LL_PRINT("driver unloaded\n");
}

