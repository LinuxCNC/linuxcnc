
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


#define HM2_SOCFPGA_VERSION "0.9"

#define HM2_LLIO_NAME "hm2_soc_ol"

#define HM2_SOC_MAX_BOARDS  2

// AFAICT  hm2_soc_pins_t struct 
// is not used, its members are 
// not referenced anywhere
// and the driver does not create
// pins of these names

typedef struct {
    hal_u32_t *irq_count;
    hal_u32_t *irq_missed;
    hal_u32_t *read_errors;
    hal_u32_t *write_errors;
} hm2_soc_pins_t;

typedef struct {
    int fpga_state;
    int uio_fd;
    int firmware_given;
    const char *name;
    char *config;
    char *descriptor;
    const char *uio_dev;
    // local copy of calling args
    int argc;
    char* const *argv;
    void __iomem *base;
    int len;
    char *firmware;
    hm2_lowlevel_io_t llio;
    hm2_soc_pins_t *pins;
    int no_init_llio;
    int num;
    int debug;
    int already_programmed;
} hm2_soc_t;

