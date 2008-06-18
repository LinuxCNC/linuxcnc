
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

#include <linux/slab.h>

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "hal.h"

#include "hal/drivers/mesa-hostmot2/hostmot2.h"




MODULE_INFO(emc2, "component:hostmot2:RTAI driver for the HostMot2 firmware from Mesa Electronics.");
MODULE_INFO(emc2, "funct:read:1:Read all registers.");
MODULE_INFO(emc2, "funct:write:1:Write all registers.");
MODULE_INFO(emc2, "funct:pet_watchdog:0:Pet the watchdog to keep it from biting us for a while.");
MODULE_INFO(emc2, "license:GPL");

MODULE_LICENSE("GPL");




int debug_idrom = 0;
RTAPI_MP_INT(debug_idrom, "Developer/debug use only!  Enable debug logging of the HostMot2\nIDROM header.");

int debug_module_descriptors = 0;
RTAPI_MP_INT(debug_module_descriptors, "Developer/debug use only!  Enable debug logging of the HostMot2\nModule Descriptors.");

int debug_modules = 0;
RTAPI_MP_INT(debug_modules, "Developer/debug use only!  Enable debug logging of the HostMot2\nModules used.");




// this keeps track of all the hm2 instances that have been registered by
// the low-level drivers
struct list_head hm2_list;


static int comp_id;




// 
// functions exported to EMC
// 

static void hm2_read(void *void_hm2, long period) {
    hostmot2_t *hm2 = void_hm2;

    // if there are comm problems, wait for the user to fix it
    if ((*hm2->llio->io_error) != 0) return;

    // if the watchdog has bit, wait for the user to reset it
    if ((hm2->watchdog.num_instances == 1) && (*hm2->watchdog.instance[0].hal.pin.has_bit != 0)) return;

    hm2_tram_read(hm2);
    if ((*hm2->llio->io_error) != 0) return;

    hm2_ioport_gpio_process_tram_read(hm2);
    hm2_encoder_process_tram_read(hm2);
    hm2_stepgen_process_tram_read(hm2, period);

    hm2_raw_read(hm2);
}


static void hm2_write(void *void_hm2, long period) {
    hostmot2_t *hm2 = void_hm2;

    // if there are comm problems, wait for the user to fix it
    if ((*hm2->llio->io_error) != 0) return;

    // if the watchdog has bit, wait for the user to reset it
    if ((hm2->watchdog.num_instances == 1) && (*hm2->watchdog.instance[0].hal.pin.has_bit != 0)) return;

    hm2_ioport_gpio_prepare_tram_write(hm2);
    hm2_pwmgen_prepare_tram_write(hm2);
    hm2_stepgen_prepare_tram_write(hm2, period);
    hm2_tram_write(hm2);

    // these usually do nothing
    // they only write to the FPGA if certain pins & params have changed
    hm2_ioport_write(hm2);    // handles gpio.is_output but not gpio.out (that's done in tram_write() above)
    hm2_watchdog_write(hm2);  // in case the user has written to the watchdog.timeout_ns param
    hm2_pwmgen_write(hm2);    // update pwmgen registers if needed 
    hm2_stepgen_write(hm2);   // update stepgen registers if needed 

    hm2_raw_write(hm2);
}




// 
// misc little helper functions
//


// FIXME: the static automatic string makes this function non-reentrant
const char *hm2_hz_to_mhz(u32 freq_hz) {
    static char mhz_str[20];
    int freq_mhz, freq_mhz_fractional;

    freq_mhz = freq_hz / (1000*1000);
    freq_mhz_fractional = (freq_hz / 1000) % 1000;
    sprintf(mhz_str, "%d.%03d", freq_mhz, freq_mhz_fractional);

    return mhz_str;
}


// FIXME: the static automatic string makes this function non-reentrant
const char *hm2_get_general_function_name(int gtag) {
    switch (gtag) { 
        case HM2_GTAG_WATCHDOG:        return "Watchdog";
        case HM2_GTAG_IOPORT:          return "IOPort";
        case HM2_GTAG_ENCODER:         return "Encoder";
        case HM2_GTAG_STEPGEN:         return "StepGen";
        case HM2_GTAG_PWMGEN:          return "PWMGen";
        case HM2_GTAG_TRANSLATIONRAM:  return "TranslationRAM";
        default: {
            static char unknown[100];
            rtapi_snprintf(unknown, 100, "(unknown-gtag-%d)", gtag);
            return unknown;
        }
    }
}


static int hm2_parse_config_string(hostmot2_t *hm2, char *config_string) {
    // default is to enable everything in the firmware
    hm2->config.num_encoders = -1;
    hm2->config.num_pwmgens = -1;
    hm2->config.num_stepgens = -1;
    hm2->config.enable_raw = 0;

    DBG("parsing config string \"%s\"\n", config_string);

    do {
        char *token;
        char *endp;

        token = strsep(&config_string, " ");
        if (token == NULL) break;

        if (strncmp(token, "num_encoders=", 13) == 0) {
            token += 13;
            hm2->config.num_encoders = simple_strtol(token, &endp, 0);

        } else if (strncmp(token, "num_pwmgens=", 12) == 0) {
            token += 12;
            hm2->config.num_pwmgens = simple_strtol(token, &endp, 0);

        } else if (strncmp(token, "num_stepgens=", 13) == 0) {
            token += 13;
            hm2->config.num_stepgens = simple_strtol(token, &endp, 0);

        } else if (strncmp(token, "enable_raw", 10) == 0) {
            token += 10;
            hm2->config.enable_raw = 1;

        } else {
            ERR("invalid token in config string: \"%s\"\n", token);
            return -EINVAL;
        }
    } while (1);

    DBG("final config:\n");
    DBG("    num_encoders=%d\n", hm2->config.num_encoders);
    DBG("    num_pwmgens=%d\n",  hm2->config.num_pwmgens);
    DBG("    num_stepgens=%d\n", hm2->config.num_stepgens);
    DBG("    enable_raw=%d\n",   hm2->config.enable_raw);

    return 0;
}




// 
// functions for dealing with the idrom
// 

static void hm2_print_idrom(int level, hostmot2_t *hm2) {
    PRINT(level, "IDRom:\n"); 

    if (hm2->idrom.idrom_type == 2) {
        PRINT(level, "    IDRom Type: 0x%08X\n", hm2->idrom.idrom_type); 
    } else {
        PRINT(level, "    IDRom Type: 0x%08X ***** Expected 2!  Continuing anyway! *****\n", hm2->idrom.idrom_type); 
    }

    if (hm2->idrom.offset_to_modules == 0x40) {
        PRINT(level, "    Offset to Modules: 0x%08X\n", hm2->idrom.offset_to_modules); 
    } else {
        PRINT(level, "    Offset to Modules: 0x%08X ***** Expected 0x40!  Continuing anyway *****\n", hm2->idrom.offset_to_modules); 
    }

    if (hm2->idrom.offset_to_pin_desc == 0x200) {
        PRINT(level, "    Offset to Pin Description: 0x%08X\n", hm2->idrom.offset_to_pin_desc); 
    } else {
        PRINT(level, "    Offset to Pin Description: 0x%08X ***** Expected 0x200!  Continuing anyway! *****\n", hm2->idrom.offset_to_pin_desc); 
    }

    PRINT(
        level,
        "    Board Name: %c%c%c%c%c%c%c%c\n",
        hm2->idrom.board_name[0],
        hm2->idrom.board_name[1],
        hm2->idrom.board_name[2],
        hm2->idrom.board_name[3],
        hm2->idrom.board_name[4],
        hm2->idrom.board_name[5],
        hm2->idrom.board_name[6],
        hm2->idrom.board_name[7]
    ); 

    PRINT(level, "    FPGA Size: %u\n", hm2->idrom.fpga_size); 
    PRINT(level, "    FPGA Pins: %u\n", hm2->idrom.fpga_pins); 

    PRINT(level, "    IO Ports: %u\n", hm2->idrom.io_ports); 
    PRINT(level, "    IO Width: %u\n", hm2->idrom.io_width); 
    if (hm2->idrom.port_width == 24) {
        PRINT(level, "    Port Width: %u\n", hm2->idrom.port_width); 
    } else {
        PRINT(level, "    Port Width: %u ***** Expected 24!  Continuing anyway! *****\n", hm2->idrom.port_width); 
    }

    PRINT(
        level,
        "    Clock Low: %d Hz (%d KHz, %d MHz)\n",
        hm2->idrom.clock_low,
        (hm2->idrom.clock_low / 1000),
        (hm2->idrom.clock_low / (1000 * 1000))
    ); 

    PRINT(
        level,
        "    Clock High: %d Hz (%d KHz, %d MHz)\n",
        hm2->idrom.clock_high,
        (hm2->idrom.clock_high / 1000),
        (hm2->idrom.clock_high / (1000 * 1000))
    ); 

    PRINT(level, "    Instance Stride 0: 0x%08X\n", hm2->idrom.instance_stride_0); 
    PRINT(level, "    Instance Stride 1: 0x%08X\n", hm2->idrom.instance_stride_1); 

    PRINT(level, "    Register Stride 0: 0x%08X\n", hm2->idrom.register_stride_0); 
    PRINT(level, "    Register Stride 1: 0x%08X\n", hm2->idrom.register_stride_1); 
}


static int hm2_read_idrom(hostmot2_t *hm2) {

    // 
    // find the idrom offset
    // 

    if (!hm2->llio->read(hm2->llio, HM2_ADDR_IDROM_OFFSET, &hm2->idrom_offset, 2)) {
        ERR("error reading IDROM Offset\n");
        return -EIO;
    }


    // 
    // first read in the idrom type to make sure we know how to deal with it
    //

    
    if (!hm2->llio->read(hm2->llio, hm2->idrom_offset, &hm2->idrom.idrom_type, sizeof(hm2->idrom.idrom_type))) {
        ERR("error reading IDROM type\n"); 
        return -EIO;
    }
    if (hm2->idrom.idrom_type != 2) {
        ERR("invalid IDROM type %d, expected 2, aborting load\n", hm2->idrom.idrom_type); 
        return -EINVAL;
    }


    // 
    // ok, read in the whole thing
    //

    
    if (!hm2->llio->read(hm2->llio, hm2->idrom_offset, &hm2->idrom, sizeof(hm2->idrom))) {
        ERR("error reading IDROM\n"); 
        return -EIO;
    }


    //
    // verify the idrom we read
    //

    if (hm2->idrom.port_width != 24) {
        ERR("invalid IDROM PortWidth %d, expected 24, aborting load\n", hm2->idrom.port_width); 
        hm2_print_idrom(RTAPI_MSG_WARN, hm2);
        return -EINVAL;
    }

    if (hm2->idrom.io_width != (hm2->idrom.io_ports * hm2->idrom.port_width)) {
        ERR(
            "IDROM IOWidth is %d, but IDROM IOPorts is %d and IDROM PortWidth is %d (inconsistent firmware), aborting driver load\n",
            hm2->idrom.io_width,
            hm2->idrom.io_ports,
            hm2->idrom.port_width
        );
        return -EINVAL;
    }

    if (hm2->idrom.io_ports != (hm2->llio->num_ioport_connectors)) {
        ERR(
            "IDROM IOPorts is %d but llio num_ioport_connectors is %d (inconsistent firmware), aborting driver load\n",
            hm2->idrom.io_ports,
            hm2->llio->num_ioport_connectors
        );
        return -EINVAL;
    }

    if (debug_idrom) {
        hm2_print_idrom(RTAPI_MSG_INFO, hm2);
    }

    return 0;
}




// reads the Module Descriptors
// doesnt do any validation or parsing or anything, that's in hm2_parse_module_descriptors(), which comes next
static int hm2_read_module_descriptors(hostmot2_t *hm2) {
    int addr = hm2->idrom_offset + hm2->idrom.offset_to_modules;

    for (
        hm2->num_mds = 0;
        hm2->num_mds < HM2_MAX_MODULE_DESCRIPTORS;
        hm2->num_mds ++, addr += 12
    ) {
        u32 d[3];
        hm2_module_descriptor_t *md = &hm2->md[hm2->num_mds];

        if (!hm2->llio->read(hm2->llio, addr, d, 12)) {
            ERR("error reading Module Descriptor %d (at 0x%04x)\n", hm2->num_mds, addr); 
            return -EIO;
        }

        md->gtag = d[0] & 0x000000FF;
        if (md->gtag == 0) {
            // done
            return 0;
        }

        md->version   = (d[0] >>  8) & 0x000000FF;
        md->clock_tag = (d[0] >> 16) & 0x000000FF;
        md->instances = (d[0] >> 24) & 0x000000FF;

        if (md->clock_tag == 1) {
            md->clock_freq = hm2->idrom.clock_low;
        } else if (md->clock_tag == 2) {
            md->clock_freq = hm2->idrom.clock_high;
        } else {
            ERR("Module Descriptor %d (at 0x%04x) has invalid ClockTag %d\n", hm2->num_mds, addr, md->clock_tag); 
            return -EINVAL;
        }

        md->base_address = (d[1] >> 00) & 0x0000FFFF;
        md->num_registers = (d[1] >> 16) & 0x000000FF;

        md->register_stride = (d[1] >> 24) & 0x0000000F;
        if (md->register_stride == 0) {
            md->register_stride = hm2->idrom.register_stride_0;
        } else if (md->register_stride == 1) {
            md->register_stride = hm2->idrom.register_stride_1;
        } else {
            ERR("Module Descriptor %d (at 0x%04x) has invalid RegisterStride %d\n", hm2->num_mds, addr, md->register_stride); 
            return -EINVAL;
        }

        md->instance_stride = (d[1] >> 28) & 0x0000000F;
        if (md->instance_stride == 0) {
            md->instance_stride = hm2->idrom.instance_stride_0;
        } else if (md->instance_stride == 1) {
            md->instance_stride = hm2->idrom.instance_stride_1;
        } else {
            ERR("Module Descriptor %d (at 0x%04x) has invalid InstanceStride %d\n", hm2->num_mds, addr, md->instance_stride); 
            return -EINVAL;
        }

        md->multiple_registers = d[2];

        if (debug_module_descriptors) {
            DBG("Module Descriptor %d at 0x%04X:\n", hm2->num_mds, addr);
            DBG("    General Function Tag: %d (%s)\n", md->gtag, hm2_get_general_function_name(md->gtag));
            DBG("    Version: %d\n", md->version);
            DBG("    Clock Tag: %d (%s MHz)\n", md->clock_tag, hm2_hz_to_mhz(md->clock_freq));
            DBG("    Instances: %d\n", md->instances);
            DBG("    Base Address: 0x%04X\n", md->base_address);
            DBG("    -- Num Registers: %d\n", md->num_registers);
            DBG("    Register Stride: 0x%08X\n", md->register_stride);
            DBG("    -- Instance Stride: 0x%08X\n", md->instance_stride);
            DBG("    -- Multiple Registers: 0x%08X\n", md->multiple_registers);
        }
    }
    
    return 0;
}




// 
// Here comes the code to "parse" the Module Descriptors, turn them into
// internal-to-the-driver representations, and export those internal
// representations to the HAL.
// 
// There's a general function that walks the MD list and tries to parse
// each MD in turn, and there's a special function to parse each GTag
// (aka Function)
// 
// The per-Module parsers return the number of instances accepted, which
// may be less than the number of instances available, or even 0, if the
// user has disabled some instances using modparams.  The per-Module
// parsers return -1 on error, which causes the module load to fail.
// 


int hm2_md_is_consistent(
    hostmot2_t *hm2,
    int md_index,
    u8 version,
    u8 num_registers,
    u32 instance_stride,
    u32 multiple_registers
) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];

    if (
        (md->num_registers == num_registers)
        && (md->version == version)
        && (md->instance_stride == instance_stride)
        && (md->multiple_registers == multiple_registers)
    ) {
        return 1;
    }

    ERR(
        "inconsistent Module Descriptor for %s, not loading driver\n",
        hm2_get_general_function_name(md->gtag)
    );

    ERR(
        "    Version = %d, expected %d\n",
        md->version,
        version
    );

    ERR(
        "    NumRegisters = %d, expected %d\n",
        md->num_registers,
        num_registers
    );

    ERR(
        "    InstanceStride = 0x%08X, expected 0x%08X\n",
        md->instance_stride,
        instance_stride
    );

    ERR(
        "    MultipleRegisters = 0x%08X, expected 0x%08X\n",
        md->multiple_registers,
        multiple_registers
    );

    return 0;
}




static int hm2_parse_module_descriptors(hostmot2_t *hm2) {
    int md_index;

    for (md_index = 0; md_index < hm2->num_mds; md_index ++) {
        hm2_module_descriptor_t *md = &hm2->md[md_index];
        int md_accepted;

        if (md->gtag == 0) {
            // done
            return 0;
        }

        md_accepted = 0;  // will be set by the switch

        switch (md->gtag) {

            case HM2_GTAG_IOPORT:
                md_accepted = hm2_ioport_parse_md(hm2, md_index);
                break;

            case HM2_GTAG_ENCODER:
                md_accepted = hm2_encoder_parse_md(hm2, md_index);
                break;

            case HM2_GTAG_PWMGEN:
                md_accepted = hm2_pwmgen_parse_md(hm2, md_index);
                break;

            case HM2_GTAG_STEPGEN:
                md_accepted = hm2_stepgen_parse_md(hm2, md_index);
                break;

            case HM2_GTAG_WATCHDOG:
                md_accepted = hm2_watchdog_parse_md(hm2, md_index);
                break;

            default:
                WARN(
                    "MD %d: %dx %s v%d: ignored\n",
                    md_index,
                    md->instances,
                    hm2_get_general_function_name(md->gtag),
                    md->version
                );
                continue;

        }

        if ((*hm2->llio->io_error) != 0) {
            ERR("IO error while parsing Module Descriptor %d\n", md_index);
            return -EIO;
        }

        if (md_accepted >= 0)  {
            INFO(
                "MD %d: %dx %s v%d: accepted, using %d\n",
                md_index,
                md->instances,
                hm2_get_general_function_name(md->gtag),
                md->version,
                md_accepted
            );
        } else {
            ERR("failed to parse Module Descriptor %d\n", md_index);
            return md_accepted;
        }

    }

    return 0;  // success!
}




//
// These functions free all the memory kmalloc'ed in hm2_parse_module_descriptors()
//

static void hm2_cleanup(hostmot2_t *hm2) {
    // clean up the Modules
    hm2_ioport_cleanup(hm2);
    hm2_encoder_cleanup(hm2);
    hm2_watchdog_cleanup(hm2);
    hm2_pwmgen_cleanup(hm2);

    // free all the tram entries
    hm2_tram_cleanup(hm2);
}




static void hm2_print_modules(int msg_level, hostmot2_t *hm2) {
    hm2_encoder_print_module(msg_level, hm2);
    hm2_pwmgen_print_module(msg_level, hm2);
    hm2_stepgen_print_module(msg_level, hm2);
    hm2_ioport_print_module(msg_level, hm2);
    hm2_watchdog_print_module(msg_level, hm2);
}




// 
// register and unregister, for the low-level I/O drivers to add and remove boards to this hostmot2 driver
//


EXPORT_SYMBOL_GPL(hm2_register);
int hm2_register(hm2_lowlevel_io_t *llio, char *config_string) {
    int r;
    hostmot2_t *hm2;


    PRINT_NO_LL(RTAPI_MSG_DBG, "attempting to register %s with config \"%s\"\n", llio->name, config_string);


    // 
    // export a parameter to deal with communication errors
    //

    {
        int r;
        char name[HAL_NAME_LEN + 2];

        llio->io_error = (hal_bit_t *)hal_malloc(sizeof(hal_bit_t));
        if (llio->io_error == NULL) {
            PRINT_NO_LL(RTAPI_MSG_ERR, "out of memory!\n");
            return -ENOMEM; 
        }

        (*llio->io_error) = 0;

        rtapi_snprintf(name, HAL_NAME_LEN, "%s.io_error", llio->name);
        r = hal_param_bit_new(name, HAL_RW, llio->io_error, llio->comp_id);
        if (r != HAL_SUCCESS) {
            PRINT_NO_LL(RTAPI_MSG_ERR, "error adding param '%s', aborting\n", name);
            return -EINVAL;
        }
    }


    // 
    // read & verify FPGA firmware IOCookie
    // 

    {
        uint32_t cookie;

        if (!llio->read(llio, HM2_ADDR_IOCOOKIE, &cookie, 4)) {
            PRINT_NO_LL(RTAPI_MSG_ERR, "%s: error reading hm2 cookie\n", llio->name); 
            return -EIO;
        }

        if (cookie != HM2_IOCOOKIE) {
            PRINT_NO_LL(RTAPI_MSG_ERR, "%s: invalid cookie, got 0x%08X, expected 0x%08X\n", llio->name, cookie, HM2_IOCOOKIE); 
            PRINT_NO_LL(RTAPI_MSG_ERR, "%s: FPGA failed to initialize, or unexpected firmware?\n", llio->name); 
            return -EINVAL;
        }
    }


    // 
    // read & verify FPGA firmware ConfigName
    // 

    {
        char name[9];  // read 8, plus 1 for the NULL

        if (!llio->read(llio, HM2_ADDR_CONFIGNAME, name, 8)) {
            PRINT_NO_LL(RTAPI_MSG_ERR, "%s: error reading HM2 Config Name\n", llio->name);
            return -EIO;
        }
        name[8] = '\0';

        if (strncmp(name, HM2_CONFIGNAME, 9) != 0) {
            PRINT_NO_LL(RTAPI_MSG_ERR, "%s: invalid config name, got '%s', expected '%s'\n", llio->name, name, HM2_CONFIGNAME); 
            return -EINVAL;
        }
    }


    // 
    // looks like HostMot2 alright, let's try to initalize it
    //

    hm2 = kmalloc(sizeof(hostmot2_t), GFP_KERNEL);
    if (hm2 == NULL) {
        PRINT_NO_LL(RTAPI_MSG_ERR, "out of memory!\n");
        return -ENOMEM;
    }

    memset(hm2, 0, sizeof(hostmot2_t));

    hm2->llio = llio;

    INIT_LIST_HEAD(&hm2->tram_read_entries);
    INIT_LIST_HEAD(&hm2->tram_write_entries);

    list_add_tail(&hm2->list, &hm2_list);


    // 
    // parse the config string
    // 

    r = hm2_parse_config_string(hm2, config_string);
    if (r != 0) {
        goto fail0;
    }


    // 
    // read the IDROM Header, the Pin Descriptors, and the Module Descriptors
    // 

    r = hm2_read_idrom(hm2);
    if (r != 0) {
        goto fail0;
    }

    r = hm2_read_pin_descriptors(hm2);
    if (r != 0) {
        goto fail0;
    }

    r = hm2_read_module_descriptors(hm2);
    if (r != 0) {
        goto fail0;
    }


    // 
    // process the Module Descriptors and initialize the HostMot2 Modules found
    // 

    r = hm2_parse_module_descriptors(hm2);
    if (r != 0) {
        goto fail1;
    }


    // 
    // allocate memory for the PC's copy of the HostMot2's registers
    //

    // FIXME: this allocates memory, need to free it if hm2_register fails later
    r = hm2_allocate_tram_regions(hm2);
    if (r < 0) {
        ERR("error allocating memory for HostMot2 registers\n");
        goto fail1;
    }


    //
    // At this point, all register buffers have been allocated.
    // All non-TRAM register buffers except for the IOPort registers have
    // been initialized. All HAL objects except for the GPIOs have been
    // allocated & exported to HAL.
    //


    // 
    // set IOPorts based on detected, enabled Modules
    // all in-use module instances get all their pins, the unused pins are left as GPIOs
    // 

    hm2_configure_pins(hm2);

    r = hm2_ioport_gpio_export_hal(hm2);
    if (r != 0) {
        goto fail1;
    }


    // 
    // the "raw" interface lets you peek and poke the HostMot2 registers from HAL
    //

    r = hm2_raw_setup(hm2);
    if (r != 0) {
        goto fail1;
    }


    //
    // At this point, all non-TRAM register buffers have been initialized
    // and all HAL objects have been allocated and exported to HAL.
    //


    // 
    // Write out all the non-TRAM register buffers to the FPGA.
    // 
    // This initializes the FPGA to the default load-time state chosen by
    // the hostmot2 driver.  Users can change the state later via HAL.
    //

    hm2_force_write(hm2);


    // 
    // read the TRAM one first time
    //

    r = hm2_tram_read(hm2);
    if (r != 0) {
        goto fail1;
    }

    // set HAL gpio input pins based on FPGA pins
    hm2_ioport_gpio_process_tram_read(hm2);

    // initialize encoder count & pos to 0
    hm2_encoder_tram_init(hm2);
    hm2_encoder_process_tram_read(hm2);

    // initialize step accumulator, hal count & position to 0
    hm2_stepgen_tram_init(hm2);
    hm2_stepgen_process_tram_read(hm2, 1000);


    // 
    // write the TRAM one first time
    //

    // set gpio output pins (tho there should be none yet) based on their HAL objects
    hm2_ioport_gpio_tram_write_init(hm2);
    hm2_ioport_gpio_prepare_tram_write(hm2);

    // tell stepgen not to move
    // NOTE: the 1000 is the fake "amount of time since function last ran"
    hm2_stepgen_prepare_tram_write(hm2, 1000);

    // tell pwmgen not to move
    hm2_pwmgen_prepare_tram_write(hm2);

    r = hm2_tram_write(hm2);
    if (r != 0) {
        goto fail1;
    }


    //
    // final check for comm errors
    //

    if ((*hm2->llio->io_error) != 0) {
        ERR("comm errors while initializing firmware!\n");
        goto fail1;
    }


    //
    // all initialized show what pins & modules we ended up with
    //

    hm2_print_pin_usage(RTAPI_MSG_INFO, hm2);

    if (debug_modules) {
        DBG("HM2 Modules used:\n");
        hm2_print_modules(RTAPI_MSG_DBG, hm2);
    }


    //
    // export the main read/write functions
    //

    {
        char name[HAL_NAME_LEN + 2];

        rtapi_snprintf(name, HAL_NAME_LEN, "%s.read", hm2->llio->name);
        r = hal_export_funct(name, hm2_read, hm2, 1, 0, hm2->llio->comp_id);
        if (r != 0) {
            ERR("error %d exporting read function %s\n", r, name);
            r = -EINVAL;
            goto fail1;
        }

        rtapi_snprintf(name, HAL_NAME_LEN, "%s.write", hm2->llio->name);
        r = hal_export_funct(name, hm2_write, hm2, 1, 0, hm2->llio->comp_id);
        if (r != 0) {
            ERR("error %d exporting write function %s\n", r, name);
            r = -EINVAL;
            goto fail1;
        }
    }


    //
    // found one!
    //

    PRINT_NO_LL(RTAPI_MSG_INFO, "registered %s\n", hm2->llio->name);

    return 0;


fail1:
    hm2_cleanup(hm2);  // undoes the kmallocs from hm2_parse_module_descriptors()

fail0:
    list_del(&hm2->list);
    kfree(hm2);
    return r;
}




EXPORT_SYMBOL_GPL(hm2_unregister);
void hm2_unregister(hm2_lowlevel_io_t *llio) {
    struct list_head *ptr;

    // kill the watchdog?  nah, let it bite us and make the board safe

    // FIXME: make sure to stop the stepgens & pwmgens

    list_for_each(ptr, &hm2_list) {
        hostmot2_t *hm2 = list_entry(ptr, hostmot2_t, list);
        if (hm2->llio != llio) continue;

        PRINT_NO_LL(RTAPI_MSG_INFO, "unregistering %s\n", hm2->llio->name);

        hm2_cleanup(hm2);

        list_del(ptr);
        kfree(hm2);

        return;
    }

    PRINT_NO_LL(RTAPI_MSG_WARN, "ignoring request to unregister %s: not found\n", llio->name);
    return;
}




//
// setup and cleanup code
//

int rtapi_app_main(void) {
    comp_id = hal_init("hostmot2");
    if(comp_id < 0) return comp_id;

    PRINT_NO_LL(RTAPI_MSG_INFO, "loading Mesa HostMot2 driver version %s\n", HM2_VERSION);
    INIT_LIST_HEAD(&hm2_list);

    hal_ready(comp_id);

    return 0;
}


void rtapi_app_exit(void) {
    PRINT_NO_LL(RTAPI_MSG_INFO, "unloading\n");
    hal_exit(comp_id);
}




// this pushes our idea of what things are like into the FPGA's poor little mind
void hm2_force_write(hostmot2_t *hm2) {
    hm2_watchdog_force_write(hm2);
    hm2_ioport_force_write(hm2);
    hm2_encoder_force_write(hm2);
    hm2_pwmgen_force_write(hm2);
    hm2_stepgen_force_write(hm2);
}

