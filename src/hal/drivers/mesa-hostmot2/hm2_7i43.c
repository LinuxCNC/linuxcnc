static const void *hm2_log;

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





#include "gomc_env.h"
#include "hm2_core_api.h"
#include "hal/drivers/mesa-hostmot2/bitfile.h"
#include "hal/drivers/mesa-hostmot2/hostmot2-lowlevel.h"
#include "hal/drivers/mesa-hostmot2/hm2_7i43.h"
#include "hal/drivers/mesa-hostmot2/hostmot2.h"


typedef struct hm2_7i43_inst {
    cmod_t cmod;
    const cmod_env_t *env;
    const hm2_core_callbacks_t *core;
    int comp_id;
    int ioaddr[HM2_7I43_MAX_BOARDS];
    int ioaddr_hi[HM2_7I43_MAX_BOARDS];
    int epp_wide[HM2_7I43_MAX_BOARDS];
    int debug_epp;
    char *config[HM2_7I43_MAX_BOARDS];
    char cfg_bufs[HM2_7I43_MAX_BOARDS][256];
    hm2_7i43_t board[HM2_7I43_MAX_BOARDS];
    int num_boards;
} hm2_7i43_inst_t;




// 
// EPP I/O code
// 

static inline void hm2_7i43_epp_addr8(uint8_t addr, hm2_7i43_t *board) {
    outb(addr, board->port.base + HM2_7I43_EPP_ADDRESS_OFFSET);
    LL_PRINT_IF(board->inst->debug_epp, "selected address 0x%02X\n", addr);
}

static inline void hm2_7i43_epp_addr16(uint16_t addr, hm2_7i43_t *board) {
    outb((addr & 0x00FF), board->port.base + HM2_7I43_EPP_ADDRESS_OFFSET);
    outb((addr >> 8),     board->port.base + HM2_7I43_EPP_ADDRESS_OFFSET);
    LL_PRINT_IF(board->inst->debug_epp, "selected address 0x%04X\n", addr);
}

static inline void hm2_7i43_epp_write(int w, hm2_7i43_t *board) {
    outb(w, board->port.base + HM2_7I43_EPP_DATA_OFFSET);
    LL_PRINT_IF(board->inst->debug_epp, "wrote data 0x%02X\n", w);
}

static inline int hm2_7i43_epp_read(hm2_7i43_t *board) {
    int val;
    val = inb(board->port.base + HM2_7I43_EPP_DATA_OFFSET);
    LL_PRINT_IF(board->inst->debug_epp, "read data 0x%02X\n", val);
    return val;
}

static inline uint32_t hm2_7i43_epp_read32(hm2_7i43_t *board) {
    uint32_t data;

    if (board->epp_wide) {
	data = inl(board->port.base + HM2_7I43_EPP_DATA_OFFSET);
        LL_PRINT_IF(board->inst->debug_epp, "read data 0x%08X\n", data);
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
    if (board->epp_wide) {
	outl(w, board->port.base + HM2_7I43_EPP_DATA_OFFSET);
        LL_PRINT_IF(board->inst->debug_epp, "wrote data 0x%08X\n", w);
    } else {
        hm2_7i43_epp_write((w) & 0xFF, board);
        hm2_7i43_epp_write((w >>  8) & 0xFF, board);
        hm2_7i43_epp_write((w >> 16) & 0xFF, board);
        hm2_7i43_epp_write((w >> 24) & 0xFF, board);
    }
}

static inline uint8_t hm2_7i43_epp_read_status(hm2_7i43_t *board) {
    uint8_t val;
    val = inb(board->port.base + HM2_7I43_EPP_STATUS_OFFSET);
    LL_PRINT_IF(board->inst->debug_epp, "read status 0x%02X\n", val);
    return val;
}

static inline void hm2_7i43_epp_write_status(uint8_t status_byte, hm2_7i43_t *board) {
    outb(status_byte, board->port.base + HM2_7I43_EPP_STATUS_OFFSET);
    LL_PRINT_IF(board->inst->debug_epp, "wrote status 0x%02X\n", status_byte);
}

static inline void hm2_7i43_epp_write_control(uint8_t control_byte, hm2_7i43_t *board) {
    outb(control_byte, board->port.base + HM2_7I43_EPP_CONTROL_OFFSET);
    LL_PRINT_IF(board->inst->debug_epp, "wrote control 0x%02X\n", control_byte);
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
        LL_PRINT("failed to clear EPP Timeout!\n");
        return 0;  // fail
    }
    return 1;  // success
}


//
// misc generic helper functions
//

// FIXME: this is bogus
static void hm2_7i43_nanosleep(const gomc_rtapi_t *rtapi, unsigned long int nanoseconds) {
    long int max_ns_delay;

    max_ns_delay = rtapi->delay_max(rtapi->ctx);

    while (nanoseconds > (unsigned long int)max_ns_delay) {
        rtapi->delay(rtapi->ctx, max_ns_delay);
        nanoseconds -= max_ns_delay;
    }

    rtapi->delay(rtapi->ctx, nanoseconds);
}




// 
// these are the low-level i/o functions exported to the hostmot2 driver
//

int hm2_7i43_read(hm2_lowlevel_io_t *this, uint32_t addr, void *buffer, int size) {
    int bytes_remaining = size;
    hm2_7i43_t *board = this->private;

    hm2_7i43_epp_addr16(addr | HM2_7I43_ADDR_AUTOINCREMENT, board);

    for (; bytes_remaining > 3; bytes_remaining -= 4) {
        *((uint32_t*)buffer) = hm2_7i43_epp_read32(board);
        buffer += 4;
    }

    for ( ; bytes_remaining > 0; bytes_remaining --) {
        *((uint8_t*)buffer) = hm2_7i43_epp_read(board);
        buffer ++;
    }

    if (hm2_7i43_epp_check_for_timeout(board)) {
        THIS_PRINT("EPP timeout on data cycle of read(addr=0x%04x, size=%d)\n", addr, size);
        (*this->io_error) = 1;
        this->needs_reset = 1;
        hm2_7i43_epp_clear_timeout(board);
        return 0;  // fail
    }

    return 1;  // success
}




int hm2_7i43_write(hm2_lowlevel_io_t *this, uint32_t addr, const void *buffer, int size) {
    int bytes_remaining = size;
    hm2_7i43_t *board = this->private;

    hm2_7i43_epp_addr16(addr | HM2_7I43_ADDR_AUTOINCREMENT, board);

    for (; bytes_remaining > 3; bytes_remaining -= 4) {
        hm2_7i43_epp_write32(*((uint32_t*)buffer), board);
        buffer += 4;
    }

    for ( ; bytes_remaining > 0; bytes_remaining --) {
        hm2_7i43_epp_write(*((uint8_t*)buffer), board);
        buffer ++;
    }

    if (hm2_7i43_epp_check_for_timeout(board)) {
        THIS_PRINT("EPP timeout on data cycle of write(addr=0x%04x, size=%d)\n", addr, size);
        (*this->io_error) = 1;
        this->needs_reset = 1;
        hm2_7i43_epp_clear_timeout(board);
        return 0;
    }

    return 1;
}




int hm2_7i43_program_fpga(hm2_lowlevel_io_t *this, const bitfile_t *bitfile) {
    hm2_7i43_t *board = this->private;
    hm2_7i43_inst_t *inst = board->inst;
    int orig_debug_epp = inst->debug_epp;  // we turn off EPP debugging for this part...
    int64_t start_time, end_time;
    int i;
    const uint8_t *firmware = bitfile->e.data;


    //
    // send the firmware
    //

    inst->debug_epp = 0;
    start_time = inst->env->rtapi->get_time(inst->env->rtapi->ctx);

    // select the CPLD's data address
    hm2_7i43_epp_addr8(0, board);

    for (i = 0; i < bitfile->e.size; i ++, firmware ++) {
        hm2_7i43_epp_write(bitfile_reverse_bits(*firmware), board);
    }

    end_time = inst->env->rtapi->get_time(inst->env->rtapi->ctx);
    inst->debug_epp = orig_debug_epp;


    // see if it worked
    if (hm2_7i43_epp_check_for_timeout(board)) {
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

    if(board->epp_wide)
    {
        hm2_7i43_epp_clear_timeout(board);
        uint32_t cookie;
        hm2_7i43_read(this, HM2_ADDR_IOCOOKIE, &cookie, sizeof(cookie));
        if(cookie != HM2_IOCOOKIE) {
            THIS_ERR("Reading cookie with epp_wide failed. (read 0x%08x) Falling back to byte transfers\n", cookie);
            board->epp_wide = 0;
            hm2_7i43_epp_clear_timeout(board);
            hm2_7i43_read(this, HM2_ADDR_IOCOOKIE, &cookie, sizeof(cookie));
            if(cookie == HM2_IOCOOKIE) {
                THIS_ERR("Successfully read cookie after selecting byte transfers\n");
            } else {
                THIS_ERR("Reading cookie still failed without epp_wide. (read 0x%08x)\n", cookie);
            }
        }
    }

    return 0;
}




// return 0 if the board has been reset, -errno if not
int hm2_7i43_reset(hm2_lowlevel_io_t *this) {
    hm2_7i43_t *board = this->private;
    hm2_7i43_inst_t *inst = board->inst;
    uint8_t byte;


    //
    // this resets the FPGA *only* if it's currently configured with the
    // HostMot2 or GPIO firmware
    //

    hm2_7i43_epp_addr16(0x7F7F, board);
    hm2_7i43_epp_write(0x5A, board);
    hm2_7i43_epp_addr16(0x7F7F, board);
    hm2_7i43_epp_write(0x5A, board);


    // 
    // this code resets the FPGA *only* if the CPLD is in charge of the
    // parallel port
    //

    // select the control register
    hm2_7i43_epp_addr8(1, board);

    // bring the Spartan3's PROG_B line low for 1 us (the specs require 300-500 ns or longer)
    hm2_7i43_epp_write(0x00, board);
    hm2_7i43_nanosleep(inst->env->rtapi, 1000);

    // bring the Spartan3's PROG_B line high and wait for 2 ms before sending firmware (required by spec)
    hm2_7i43_epp_write(0x01, board);
    hm2_7i43_nanosleep(inst->env->rtapi, 2 * 1000 * 1000);

    // make sure the FPGA is not asserting its /DONE bit
    byte = hm2_7i43_epp_read(board);
    if ((byte & 0x01) != 0) {
        LL_PRINT("/DONE is not low after CPLD reset!\n");
        return -EIO;
    }

    return 0;
}




//
// setup and cleanup code
//


static void hm2_7i43_cleanup(hm2_7i43_inst_t *inst) {
    int i;

    // NOTE: hm2->llio->hal->malloc(hm2->llio->hal->ctx, ) doesn't have a matching free

    for (i = 0; i < inst->num_boards; i ++) {
        hm2_lowlevel_io_t *this = &inst->board[i].llio;
        THIS_PRINT("releasing board\n");
        inst->core->unregister_board(inst->core->ctx, this);
        rtapi_parport_release(&inst->board[i].port);
    }
}


static int hm2_7i43_setup(hm2_7i43_inst_t *inst) {
    int i;

    LL_PRINT("loading HostMot2 Mesa 7i43 driver version %s\n", HM2_7I43_VERSION);

    // zero the board structs
    memset(inst->board, 0, HM2_7I43_MAX_BOARDS * sizeof(hm2_7i43_t));
    inst->num_boards = 0;

    for (i = 0; i < HM2_7I43_MAX_BOARDS; i ++) {
        if (inst->ioaddr[i] < 0) break;

        hm2_lowlevel_io_t *this;
        int r;

        inst->board[i].epp_wide = inst->epp_wide[i];
        inst->board[i].inst = inst;

        //
        // claim the I/O regions for the parport
        // 

        r = rtapi_parport_get(inst->board[i].llio.name, &inst->board[i].port,
                inst->ioaddr[i], inst->ioaddr_hi[i], PARPORT_MODE_EPP);
        if(r < 0)
            return r;

        // set up the parport for EPP
	if(inst->board[i].port.base_hi) {
	    outb(0x94, inst->board[i].port.base_hi + HM2_7I43_ECP_CONTROL_HIGH_OFFSET); // select EPP mode in ECR
        }

        //
        // when we get here, the parallel port is in epp mode and ready to go
        //

        // select the device and tell it to make itself ready for io
        hm2_7i43_epp_write_control(0x04, &inst->board[i]);  // set control lines and input mode
        hm2_7i43_epp_clear_timeout(&inst->board[i]);

        snprintf(inst->board[i].llio.name, sizeof(inst->board[i].llio.name), "%s.%d", HM2_LLIO_NAME, i);
        inst->board[i].llio.comp_id = inst->comp_id;

        inst->board[i].llio.read = hm2_7i43_read;
        inst->board[i].llio.write = hm2_7i43_write;
        inst->board[i].llio.program_fpga = hm2_7i43_program_fpga;
        inst->board[i].llio.reset = hm2_7i43_reset;

        inst->board[i].llio.num_ioport_connectors = 2;
        inst->board[i].llio.pins_per_connector = 24;
        inst->board[i].llio.ioport_connector_name[0] = "P4";
        inst->board[i].llio.ioport_connector_name[1] = "P3";
        inst->board[i].llio.num_leds = 8;
        inst->board[i].llio.private = &inst->board[i];

        this = &inst->board[i].llio;


        //
        // now we want to detect if this 7i43 has the big FPGA or the small one
        // 3s200tq144 for the small board
        // 3s400tq144 for the big
        //

        // make sure the CPLD is in charge of the parallel port
        hm2_7i43_reset(&inst->board[i].llio);

        //  select CPLD data register
        hm2_7i43_epp_addr8(0, &inst->board[i]);

        if (hm2_7i43_epp_read(&inst->board[i]) & 0x01) {
            inst->board[i].llio.fpga_part_number = "3s400tq144";
        } else {
            inst->board[i].llio.fpga_part_number = "3s200tq144";
        }
        THIS_DBG("detected FPGA '%s'\n", inst->board[i].llio.fpga_part_number);


        r = inst->core->register_board(inst->core->ctx, &inst->board[i].llio, inst->config[i]);
        if (r != 0) {
            rtapi_parport_release(&inst->board[i].port);
            THIS_ERR(
                "board at (ioaddr=0x%04X, ioaddr_hi=0x%04X, epp_wide %s) not found!\n",
                inst->board[i].port.base,
                inst->board[i].port.base_hi,
                (inst->board[i].epp_wide ? "ON" : "OFF"));
            return r;
        }

        THIS_PRINT(
            "board at (ioaddr=0x%04X, ioaddr_hi=0x%04X, epp_wide %s) found\n",
            inst->board[i].port.base,
            inst->board[i].port.base_hi,
            (inst->board[i].epp_wide ? "ON" : "OFF")
        );

        inst->num_boards ++;
    }

    return 0;
}


static void hm2_7i43_destroy(cmod_t *self);

static void hm2_7i43_parse_argv(hm2_7i43_inst_t *inst, int argc, const char **argv) {
    int cfg_idx = 0, io_idx = 0, iohi_idx = 0, ew_idx = 0;

    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "config=", 7) == 0 && cfg_idx < HM2_7I43_MAX_BOARDS) {
            strncpy(inst->cfg_bufs[cfg_idx], argv[i] + 7, sizeof(inst->cfg_bufs[0]) - 1);
            inst->config[cfg_idx] = inst->cfg_bufs[cfg_idx];
            cfg_idx++;
        } else if (strncmp(argv[i], "ioaddr=", 7) == 0 && io_idx < HM2_7I43_MAX_BOARDS) {
            inst->ioaddr[io_idx] = strtol(argv[i] + 7, NULL, 0);
            io_idx++;
        } else if (strncmp(argv[i], "ioaddr_hi=", 10) == 0 && iohi_idx < HM2_7I43_MAX_BOARDS) {
            inst->ioaddr_hi[iohi_idx] = strtol(argv[i] + 10, NULL, 0);
            iohi_idx++;
        } else if (strncmp(argv[i], "epp_wide=", 9) == 0 && ew_idx < HM2_7I43_MAX_BOARDS) {
            inst->epp_wide[ew_idx] = strtol(argv[i] + 9, NULL, 0);
            ew_idx++;
        } else if (strncmp(argv[i], "debug_epp=", 10) == 0) {
            inst->debug_epp = strtol(argv[i] + 10, NULL, 0);
        }
    }
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    const gomc_hal_t *hal = env->hal;
    hm2_log = env->log;
    int r = 0;

    hm2_7i43_inst_t *inst = env->rtapi->calloc(env->rtapi->ctx, sizeof(*inst));
    if (!inst) return -ENOMEM;
    inst->env = env;

    // Set non-zero defaults (calloc zeroed everything)
    inst->ioaddr[0] = 0;
    for (int i = 1; i < HM2_7I43_MAX_BOARDS; i++)
        inst->ioaddr[i] = -1;
    for (int i = 0; i < HM2_7I43_MAX_BOARDS; i++)
        inst->epp_wide[i] = 1;

    hm2_7i43_parse_argv(inst, argc, argv);

    inst->core = hm2_core_api_get(env->api, "hostmot2");
    if (!inst->core) {
        gomc_log_errorf(env->log, name, "hm2_7i43: hostmot2 core API not found (is hostmot2 loaded?)\n");
        inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
        return -1;
    }

    r = hal->init(hal->ctx, HM2_LLIO_NAME, env->dl_handle, GOMC_HAL_COMP_REALTIME);
    if (r < 0) {
        inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
        return r;
    }
    inst->comp_id = r;

    r = hm2_7i43_setup(inst);
    if (r) {
        hm2_7i43_cleanup(inst);
        hal->exit(hal->ctx, inst->comp_id);
        inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
    } else {
        hal->ready(hal->ctx, inst->comp_id);
    }

    if (r) return r;

    inst->cmod.Destroy = hm2_7i43_destroy;
    inst->cmod.priv = inst;
    *out = &inst->cmod;
    return 0;
}


static void hm2_7i43_destroy(cmod_t *self) {
    hm2_7i43_inst_t *inst = self->priv;
    const gomc_hal_t *hal = inst->env->hal;
    hm2_7i43_cleanup(inst);
    hal->exit(hal->ctx, inst->comp_id);
    LL_PRINT("driver unloaded\n");
    inst->env->rtapi->free(inst->env->rtapi->ctx, inst);
}

