//
//    Copyright (C) 2012 Michael Geszkiewicz
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
//    Module descriptors and modules management
//

#include "hostmot2.h"
#include "modules.h"

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

int hm2_md_is_consistent_or_complain(
    hostmot2_t *hm2,
    int md_index,
    u8 version,
    u8 num_registers,
    u32 instance_stride,
    u32 multiple_registers
) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];

    if (hm2_md_is_consistent(hm2, md_index, version, num_registers, instance_stride, multiple_registers)) return 1;

    HM2_ERR(
        "inconsistent Module Descriptor for %s, not loading driver\n",
        hm2_get_general_function_name(md->gtag)
    );

    HM2_ERR(
        "    Version = %d, expected %d\n",
        md->version,
        version
    );

    HM2_ERR(
        "    NumRegisters = %d, expected %d\n",
        md->num_registers,
        num_registers
    );

    HM2_ERR(
        "    InstanceStride = 0x%08X, expected 0x%08X\n",
        md->instance_stride,
        instance_stride
    );

    HM2_ERR(
        "    MultipleRegisters = 0x%08X, expected 0x%08X\n",
        md->multiple_registers,
        multiple_registers
    );

    return 0;
}


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

    return 0;
}

// reads the Module Descriptors
// doesnt do any validation or parsing or anything, that's in hm2_parse_module_descriptors(), which comes next
int hm2_read_module_descriptors(hostmot2_t *hm2, int debug_module_descriptors) {
    int addr = hm2->idrom_offset + hm2->idrom.offset_to_modules;

    for (
        hm2->num_mds = 0;
        hm2->num_mds < HM2_MAX_MODULE_DESCRIPTORS;
        hm2->num_mds ++, addr += 12
    ) {
        u32 d[3];
        hm2_module_descriptor_t *md = &hm2->md[hm2->num_mds];

        if (!hm2->llio->read(hm2->llio, addr, d, 12)) {
            HM2_ERR("error reading Module Descriptor %d (at 0x%04x)\n", hm2->num_mds, addr);
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
            HM2_ERR("Module Descriptor %d (at 0x%04x) has invalid ClockTag %d\n", hm2->num_mds, addr, md->clock_tag);
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
            HM2_ERR("Module Descriptor %d (at 0x%04x) has invalid RegisterStride %d\n", hm2->num_mds, addr, md->register_stride);
            return -EINVAL;
        }

        md->instance_stride = (d[1] >> 28) & 0x0000000F;
        if (md->instance_stride == 0) {
            md->instance_stride = hm2->idrom.instance_stride_0;
        } else if (md->instance_stride == 1) {
            md->instance_stride = hm2->idrom.instance_stride_1;
        } else {
            HM2_ERR("Module Descriptor %d (at 0x%04x) has invalid InstanceStride %d\n", hm2->num_mds, addr, md->instance_stride);
            return -EINVAL;
        }

        md->multiple_registers = d[2];

        if (debug_module_descriptors) {
            HM2_PRINT("Module Descriptor %d at 0x%04X:\n", hm2->num_mds, addr);
            HM2_PRINT("    General Function Tag: %d (%s)\n", md->gtag, hm2_get_general_function_name(md->gtag));
            HM2_PRINT("    Version: %d\n", md->version);
            HM2_PRINT("    Clock Tag: %d (%s MHz)\n", md->clock_tag, hm2_hz_to_mhz(md->clock_freq));
            HM2_PRINT("    Instances: %d\n", md->instances);
            HM2_PRINT("    Base Address: 0x%04X\n", md->base_address);
            HM2_PRINT("    -- Num Registers: %d\n", md->num_registers);
            HM2_PRINT("    Register Stride: 0x%08X\n", md->register_stride);
            HM2_PRINT("    -- Instance Stride: 0x%08X\n", md->instance_stride);
            HM2_PRINT("    -- Multiple Registers: 0x%08X\n", md->multiple_registers);
        }
    }

    return 0;
}

int hm2_parse_module_descriptors(hostmot2_t *hm2) {
    int md_index, md_accepted;
    
    // Run through once looking for IO Ports in case other modules
    // need them
    for (md_index = 0; md_index < hm2->num_mds; md_index ++) {
        hm2_module_descriptor_t *md = &hm2->md[md_index];

        if (md->gtag != HM2_GTAG_IOPORT) {
            continue;
        }

        md_accepted = hm2_ioport_parse_md(hm2, md_index);

        if ((*hm2->llio->io_error) != 0) {
            HM2_ERR("IO error while parsing Module Descriptor %d\n", md_index);
            return -EIO;
        }

        if (md_accepted >= 0)  {
            HM2_INFO(
                     "MD %d: %dx %s v%d: accepted, using %d\n",
                     md_index,
                     md->instances,
                     hm2_get_general_function_name(md->gtag),
                     md->version,
                     md_accepted
                     );
        } else {
            HM2_ERR("failed to parse Module Descriptor %d\n", md_index);
            return md_accepted;
        }
    }

    // Now look for the other modules. 
    for (md_index = 0; md_index < hm2->num_mds; md_index ++) {
        hm2_module_descriptor_t *md = &hm2->md[md_index];

        if (md->gtag == 0) {
            // done
            return 0;
        }

        md_accepted = 0;  // will be set by the switch

        switch (md->gtag) {

            case HM2_GTAG_ENCODER:
            case HM2_GTAG_MUXED_ENCODER:
                md_accepted = hm2_encoder_parse_md(hm2, md_index);
                break;
            
            case HM2_GTAG_RESOLVER:
                md_accepted = hm2_resolver_parse_md(hm2, md_index);
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

            case HM2_GTAG_TPPWM:
                md_accepted = hm2_tp_pwmgen_parse_md(hm2, md_index);
                break;

            case HM2_GTAG_SMARTSERIAL:
                md_accepted = hm2_sserial_parse_md(hm2, md_index);
                break;
                
            case HM2_GTAG_BSPI:
                md_accepted = hm2_bspi_parse_md(hm2, md_index);
                break;

            case HM2_GTAG_LED:
                md_accepted = hm2_led_parse_md(hm2, md_index);
                break;

            default:
                HM2_WARN(
                    "MD %d: %dx %s v%d: ignored\n",
                    md_index,
                    md->instances,
                    hm2_get_general_function_name(md->gtag),
                    md->version
                );
                continue;

        }

        if ((*hm2->llio->io_error) != 0) {
            HM2_ERR("IO error while parsing Module Descriptor %d\n", md_index);
            return -EIO;
        }

        if (md_accepted >= 0)  {
            HM2_INFO(
                "MD %d: %dx %s v%d: accepted, using %d\n",
                md_index,
                md->instances,
                hm2_get_general_function_name(md->gtag),
                md->version,
                md_accepted
            );
        } else {
            HM2_ERR("failed to parse Module Descriptor %d\n", md_index);
            return md_accepted;
        }

    }    
                                           
    return 0;  // success!
}

