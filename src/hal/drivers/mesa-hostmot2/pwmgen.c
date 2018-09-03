
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

#include "config_module.h"
#include RTAPI_INC_SLAB_H

#include "rtapi.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "hal.h"

#include "hal/drivers/mesa-hostmot2/hostmot2.h"




void hm2_pwmgen_handle_pwm_frequency(hostmot2_t *hm2) {
    u32 dds;


    if (hm2->pwmgen.hal->param.pwm_frequency < 1) {
        HM2_ERR("pwmgen.pwm_frequency %d is too low, setting to 1\n", hm2->pwmgen.hal->param.pwm_frequency);
        hm2->pwmgen.hal->param.pwm_frequency = 1;
    }


    // 
    // hal->param.pwm_frequency is the user's desired PWM frequency in Hz
    //
    // We get to play with PWMClock (frequency at which the PWM counter
    // runs) and PWMBits (number of bits of the PWM Value Register that
    // are used to hold the count, this affects resolution).
    //
    // PWMClock is controlled by the 16-bit PWM Master Rate DDS Register:
    //     PWMClock = ClockHigh * DDS / 65536
    //
    // PWMBits is 9-12.  More is better (higher resolution).
    //
    // The key equation is:
    //     PWMFreq = PWMClock / (2^PWMBits)
    //
    // This can be rewritten as:
    //     PWMFreq = (ClockHigh * DDS / 65536) / (2^PWMBits)
    //     PWMFreq = (ClockHigh * DDS) / (65536 * 2^PWMBits)
    //
    // Solve for DDS:
    //     PWMFreq * (65536 * 2^PWMBits) = ClockHigh * DDS
    //     DDS = (PWMFreq * 65536 * 2^PWMBits) / ClockHigh
    //

    // can we do it with 12 bits?
    dds = ((double)hm2->pwmgen.hal->param.pwm_frequency * 65536.0 * 4096.0) / (double)hm2->pwmgen.clock_frequency;
    if (dds < 65536) {
        hm2->pwmgen.pwmgen_master_rate_dds_reg = dds;
        hm2->pwmgen.pwm_bits = 12;
        return;
    }

    // try 11 bits
    dds = ((double)hm2->pwmgen.hal->param.pwm_frequency * 65536.0 * 2048.0) / (double)hm2->pwmgen.clock_frequency;
    if (dds < 65536) {
        hm2->pwmgen.pwmgen_master_rate_dds_reg = dds;
        hm2->pwmgen.pwm_bits = 11;
        return;
    }

    // try 10 bits
    dds = ((double)hm2->pwmgen.hal->param.pwm_frequency * 65536.0 * 1024.0) / (double)hm2->pwmgen.clock_frequency;
    if (dds < 65536) {
        hm2->pwmgen.pwmgen_master_rate_dds_reg = dds;
        hm2->pwmgen.pwm_bits = 10;
        return;
    }

    // try 9 bits
    dds = ((double)hm2->pwmgen.hal->param.pwm_frequency * 65536.0 * 512.0) / (double)hm2->pwmgen.clock_frequency;
    if (dds < 65536) {
        hm2->pwmgen.pwmgen_master_rate_dds_reg = dds;
        hm2->pwmgen.pwm_bits = 9;
        return;
    }

    // no joy, lower frequency until it'll work with 9 bits
    // From above:
    //     PWMFreq = (ClockHigh * DDS) / (65536 * 2^PWMBits)
    hm2->pwmgen.hal->param.pwm_frequency = ((double)hm2->pwmgen.clock_frequency * 65535.0) / (65536.0 * 512.0);
    HM2_ERR("max PWM frequency is %d Hz\n", hm2->pwmgen.hal->param.pwm_frequency);
    hm2->pwmgen.pwmgen_master_rate_dds_reg = 65535;
    hm2->pwmgen.pwm_bits = 9;
}


void hm2_pwmgen_handle_pdm_frequency(hostmot2_t *hm2) {
    u32 dds;


    if (hm2->pwmgen.hal->param.pdm_frequency < 1) {
        HM2_ERR("pwmgen.pdm_frequency %d is too low, setting to 1\n", hm2->pwmgen.hal->param.pdm_frequency);
        hm2->pwmgen.hal->param.pdm_frequency = 1;
    }


    // 
    // hal->param.pdm_frequency is the user's desired PDM frequency in Hz
    //
    // We get to play with PDMClock (frequency at which the PDM counter
    // runs) only - PDMBits (number of bits of the PDM Value Register that
    // are used to hold the count, this affects resolution) is fixed at 12.
    //
    // PDMClock is controlled by the 16-bit PDM Master Rate DDS Register:
    //     PDMClock = ClockHigh * DDS / 65536
    //
    // PDMBits is 12.
    //
    // The key equation is:
    //     PDMFreq = PDMClock / (2^PDMBits)
    //
    // This can be rewritten as:
    //     PDMFreq = (ClockHigh * DDS / 65536) / (2^PDMBits)
    //     PDMFreq = (ClockHigh * DDS) / (65536 * 2^PDMBits)
    //     PDMFreq = (ClockHigh * DDS) / (65536 * 4096)
    //
    // The PDMFreq is the frequency at which the 4096-pulse pattern
    // repeats.  The pulse frequency is 4096 times higher:
    //
    //     PulseFreq = PDMFreq * 4096
    //     PulseFreq = (ClockHigh * DDS) / 65536
    //
    // Solve for DDS:
    //     PDMFreq * (65536 * 4096) = ClockHigh * DDS
    //     DDS = (PDMFreq * 65536 * 4096) / ClockHigh
    //
    //     PulseFreq = (ClockHigh * DDS) / 65536
    //     DDS = (PulseFreq * 65536) / ClockHigh
    //

    // can we do it with 12 bits?
    dds = ((double)hm2->pwmgen.hal->param.pdm_frequency * 65536.0) / (double)hm2->pwmgen.clock_frequency;
    if (dds == 0) {
        // too slow, set frequency to minimum
        // From above:
        //     PulseFreq = (ClockHigh * DDS) / 65536
        dds = 1;
        hm2->pwmgen.hal->param.pdm_frequency = ((double)hm2->pwmgen.clock_frequency * (double)dds) / 65536.0;
        HM2_ERR("min PDM frequency is %d Hz\n", hm2->pwmgen.hal->param.pdm_frequency);
        hm2->pwmgen.pdmgen_master_rate_dds_reg = 1;
        return;
    }

    if (dds < 65536) {
        // ok
        hm2->pwmgen.pdmgen_master_rate_dds_reg = dds;
        return;
    }

    // user wants too much, lower frequency until it'll work with 12 bits
    // From above:
    //     PulseFreq = (ClockHigh * DDS) / 65536
    hm2->pwmgen.hal->param.pdm_frequency = ((double)hm2->pwmgen.clock_frequency * 65535.0) / 65536.0;
    HM2_ERR("max PDM frequency is %d Hz\n", hm2->pwmgen.hal->param.pdm_frequency);
    hm2->pwmgen.pdmgen_master_rate_dds_reg = 65535;
}


void hm2_pwmgen_force_write(hostmot2_t *hm2) {
    int i;
    u32 pwm_width;

    if (hm2->pwmgen.num_instances == 0) return;

    hm2_pwmgen_handle_pwm_frequency(hm2);
    hm2_pwmgen_handle_pdm_frequency(hm2);

    switch (hm2->pwmgen.pwm_bits) {
        case 9: {
            pwm_width = 0x00;
            break;
        }
        case 10: {
            pwm_width = 0x01;
            break;
        }
        case 11: {
            pwm_width = 0x02;
            break;
        }
        case 12: {
            pwm_width = 0x03;
            break;
        }
        default: {
            // this should never happen
            HM2_ERR("invalid pwmgen.bits=%d, resetting to 9\n", hm2->pwmgen.pwm_bits);
            hm2->pwmgen.pwm_bits = 9;
            pwm_width = 0x00;
            break;
        }
    }


    for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
        u32 double_buffered;

        hm2->pwmgen.pwm_mode_reg[i] = pwm_width;

        switch (hm2->pwmgen.instance[i].hal.param.output_type) {
            case HM2_PWMGEN_OUTPUT_TYPE_PWM: {
                // leave the Output Mode bits 0
                double_buffered = 1;
                break;
            }

            case HM2_PWMGEN_OUTPUT_TYPE_UP_DOWN: {
                hm2->pwmgen.pwm_mode_reg[i] |= 0x2 << 3;
                double_buffered = 1;
                break;
            }

            case HM2_PWMGEN_OUTPUT_TYPE_PDM: {
                hm2->pwmgen.pwm_mode_reg[i] |= 0x3 << 3;
                double_buffered = 0;
                break;
            }

            case HM2_PWMGEN_OUTPUT_TYPE_PWM_SWAPPED: {  // Dir & PWM (ie with the output pins swapped), "for locked antiphase"
                hm2->pwmgen.pwm_mode_reg[i] |= 0x1 << 3;
                double_buffered = 1;
                break;
            }

            default: {  // unknown pwm mode!  complain and switch to pwm/dir
                HM2_ERR(
                    "invalid pwmgen output_type %d requested\n",
                    hm2->pwmgen.instance[i].hal.param.output_type
                ); 
                HM2_ERR(
                    "supported .output-type values are: %d (PWM & Dir), %d (Up & Down), %d (PDM & Dir), and %d (Dir & PWM)\n",
                    HM2_PWMGEN_OUTPUT_TYPE_PWM,
                    HM2_PWMGEN_OUTPUT_TYPE_UP_DOWN,
                    HM2_PWMGEN_OUTPUT_TYPE_PDM,
                    HM2_PWMGEN_OUTPUT_TYPE_PWM_SWAPPED
                ); 
                HM2_ERR("switching to 1 (PWM & Dir)\n"); 
                hm2->pwmgen.instance[i].hal.param.output_type = HM2_PWMGEN_OUTPUT_TYPE_PWM;
                double_buffered = 1;
                // leave the Output Mode bits 0
                break;
            }
        }

        hm2->pwmgen.pwm_mode_reg[i] |= (double_buffered << 5);
    }


    // update enable register
    hm2->pwmgen.enable_reg = 0;
    for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
        if (*(hm2->pwmgen.instance[i].hal.pin.enable)) {
            hm2->pwmgen.enable_reg |= (1 << i);
        }
    }


    hm2->llio->write(hm2->llio, hm2->pwmgen.pwm_mode_addr, hm2->pwmgen.pwm_mode_reg, (hm2->pwmgen.num_instances * sizeof(u32)));
    hm2->llio->write(hm2->llio, hm2->pwmgen.enable_addr, &hm2->pwmgen.enable_reg, sizeof(u32));
    hm2->llio->write(hm2->llio, hm2->pwmgen.pwmgen_master_rate_dds_addr, &hm2->pwmgen.pwmgen_master_rate_dds_reg, sizeof(u32));
    hm2->llio->write(hm2->llio, hm2->pwmgen.pdmgen_master_rate_dds_addr, &hm2->pwmgen.pdmgen_master_rate_dds_reg, sizeof(u32));

    if ((*hm2->llio->io_error) != 0) return;

    for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
        hm2->pwmgen.instance[i].written_output_type = hm2->pwmgen.instance[i].hal.param.output_type;
        hm2->pwmgen.instance[i].written_enable = *hm2->pwmgen.instance[i].hal.pin.enable;
    }

    hm2->pwmgen.written_pwm_frequency = hm2->pwmgen.hal->param.pwm_frequency;
    hm2->pwmgen.written_pdm_frequency = hm2->pwmgen.hal->param.pdm_frequency;
}




//
// Update the PWM Mode Registers of all pwmgen instances that need it
//

void hm2_pwmgen_write(hostmot2_t *hm2) {
    int i;

    if (hm2->pwmgen.num_instances == 0) return;

    // check output type
    for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
        if (hm2->pwmgen.instance[i].hal.param.output_type != hm2->pwmgen.instance[i].written_output_type) {
            goto force_write;
        }
    }

    // check pwm & pdm frequency
    if (hm2->pwmgen.hal->param.pwm_frequency != hm2->pwmgen.written_pwm_frequency) goto force_write;
    if (hm2->pwmgen.hal->param.pdm_frequency != hm2->pwmgen.written_pdm_frequency) goto force_write;

    // update enable register?
    for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
        if (*(hm2->pwmgen.instance[i].hal.pin.enable) != hm2->pwmgen.instance[i].written_enable) {
            goto force_write;
        }
    }

    return;

force_write:
    hm2_pwmgen_force_write(hm2);
}




int hm2_pwmgen_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r;


    // 
    // some standard sanity checks
    //

    if (!hm2_md_is_consistent_or_complain(hm2, md_index, 0, 5, 4, 0x0003)) {
        HM2_ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }

    if (hm2->pwmgen.num_instances != 0) {
        HM2_ERR(
            "found duplicate Module Descriptor for %s (inconsistent firmware), not loading driver\n",
            hm2_get_general_function_name(md->gtag)
        );
        return -EINVAL;
    }

    if (hm2->config.num_pwmgens > md->instances) {
        HM2_ERR(
            "config.num_pwmgens=%d, but only %d are available, not loading driver\n",
            hm2->config.num_pwmgens,
            md->instances
        );
        return -EINVAL;
    }

    if (hm2->config.num_pwmgens == 0) {
        return 0;
    }


    // 
    // looks good, start initializing
    // 


    if (hm2->config.num_pwmgens == -1) {
        hm2->pwmgen.num_instances = md->instances;
    } else {
        hm2->pwmgen.num_instances = hm2->config.num_pwmgens;
    }


    // allocate the module-global HAL shared memory
    hm2->pwmgen.hal = (hm2_pwmgen_module_global_t *)hal_malloc(sizeof(hm2_pwmgen_module_global_t));
    if (hm2->pwmgen.hal == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }


    hm2->pwmgen.instance = (hm2_pwmgen_instance_t *)hal_malloc(hm2->pwmgen.num_instances * sizeof(hm2_pwmgen_instance_t));
    if (hm2->pwmgen.instance == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    hm2->pwmgen.clock_frequency = md->clock_freq;
    hm2->pwmgen.version = md->version;

    hm2->pwmgen.pwm_value_addr = md->base_address + (0 * md->register_stride);
    hm2->pwmgen.pwm_mode_addr = md->base_address + (1 * md->register_stride);
    hm2->pwmgen.pwmgen_master_rate_dds_addr = md->base_address + (2 * md->register_stride);
    hm2->pwmgen.pdmgen_master_rate_dds_addr = md->base_address + (3 * md->register_stride);
    hm2->pwmgen.enable_addr = md->base_address + (4 * md->register_stride);

    r = hm2_register_tram_write_region(hm2, hm2->pwmgen.pwm_value_addr, (hm2->pwmgen.num_instances * sizeof(u32)), &hm2->pwmgen.pwm_value_reg);
    if (r < 0) {
        HM2_ERR("error registering tram write region for PWM Value register (%d)\n", r);
        goto fail0;
    }

    hm2->pwmgen.pwm_mode_reg = (u32 *)kmalloc(hm2->pwmgen.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->pwmgen.pwm_mode_reg == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    // export to HAL
    // FIXME: r hides the r in enclosing function, and it returns the wrong thing
    {
        int i;
        int r;
        char name[HAL_NAME_LEN + 1];


        // these hal parameters affect all pwmgen instances
        r = hal_param_u32_newf(
            HAL_RW,
            &(hm2->pwmgen.hal->param.pwm_frequency),
            hm2->llio->comp_id,
            "%s.pwmgen.pwm_frequency",
            hm2->llio->name
        );
        if (r < 0) {
            HM2_ERR("error adding pwmgen.pwm_frequency param, aborting\n");
            goto fail1;
        }
        hm2->pwmgen.hal->param.pwm_frequency = 20000;
        hm2->pwmgen.written_pwm_frequency = 0;

        r = hal_param_u32_newf(
            HAL_RW,
            &(hm2->pwmgen.hal->param.pdm_frequency),
            hm2->llio->comp_id,
            "%s.pwmgen.pdm_frequency",
            hm2->llio->name
        );
        if (r < 0) {
            HM2_ERR("error adding pwmgen.pdm_frequency param, aborting\n");
            goto fail1;
        }
        hm2->pwmgen.hal->param.pdm_frequency = 20000;
        hm2->pwmgen.written_pdm_frequency = 0;


        for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
            // pins
            rtapi_snprintf(name, sizeof(name), "%s.pwmgen.%02d.value", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->pwmgen.instance[i].hal.pin.value), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.pwmgen.%02d.enable", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->pwmgen.instance[i].hal.pin.enable), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            // parameters

            rtapi_snprintf(name, sizeof(name), "%s.pwmgen.%02d.scale", hm2->llio->name, i);
            r = hal_param_float_new(name, HAL_RW, &(hm2->pwmgen.instance[i].hal.param.scale), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding param '%s', aborting\n", name);
                goto fail1;
            }

            r = hal_param_s32_newf(
                HAL_RW,
                &(hm2->pwmgen.instance[i].hal.param.output_type),
                hm2->llio->comp_id,
                "%s.pwmgen.%02d.output-type",
                hm2->llio->name,
                i
            );
            if (r < 0) {
                HM2_ERR("error adding param, aborting\n");
                goto fail1;
            }

            // init hal objects
            *(hm2->pwmgen.instance[i].hal.pin.enable) = 0;
            *(hm2->pwmgen.instance[i].hal.pin.value) = 0.0;
            hm2->pwmgen.instance[i].hal.param.scale = 1.0;
            hm2->pwmgen.instance[i].hal.param.output_type = HM2_PWMGEN_OUTPUT_TYPE_PWM;

            hm2->pwmgen.instance[i].written_output_type = -666;  // force an update at the start
            hm2->pwmgen.instance[i].written_enable = -666;       // force an update at the start
        }
    }


    return hm2->pwmgen.num_instances;


fail1:
    kfree(hm2->pwmgen.pwm_mode_reg);

fail0:
    hm2->pwmgen.num_instances = 0;
    return r;
}




void hm2_pwmgen_cleanup(hostmot2_t *hm2) {
    if (hm2->pwmgen.num_instances <= 0) return;
    if (hm2->pwmgen.pwm_mode_reg != NULL) {
        kfree(hm2->pwmgen.pwm_mode_reg);
        hm2->pwmgen.pwm_mode_reg = NULL;
    }
    hm2->pwmgen.num_instances = 0;
}




void hm2_pwmgen_print_module(hostmot2_t *hm2) {
    int i;
    HM2_PRINT("PWMGen: %d\n", hm2->pwmgen.num_instances);
    if (hm2->pwmgen.num_instances <= 0) return;
    HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n", hm2->pwmgen.clock_frequency, hm2_hz_to_mhz(hm2->pwmgen.clock_frequency));
    HM2_PRINT("    version: %d\n", hm2->pwmgen.version);
    HM2_PRINT("    pwmgen_master_rate_dds: 0x%08X (%d)\n", hm2->pwmgen.pwmgen_master_rate_dds_reg, hm2->pwmgen.pwmgen_master_rate_dds_reg);
    HM2_PRINT("    pdmgen_master_rate_dds: 0x%08X (%d)\n", hm2->pwmgen.pdmgen_master_rate_dds_reg, hm2->pwmgen.pdmgen_master_rate_dds_reg);
    HM2_PRINT("    enable: 0x%08X\n", hm2->pwmgen.enable_reg);
    HM2_PRINT("    pwm_value_addr: 0x%04X\n", hm2->pwmgen.pwm_value_addr);
    HM2_PRINT("    pwm_mode_addr: 0x%04X\n", hm2->pwmgen.pwm_mode_addr);
    HM2_PRINT("    pwmgen_master_rate_dds_addr: 0x%04X\n", hm2->pwmgen.pwmgen_master_rate_dds_addr);
    HM2_PRINT("    pdmgen_master_rate_dds_addr: 0x%04X\n", hm2->pwmgen.pdmgen_master_rate_dds_addr);
    HM2_PRINT("    enable_addr: 0x%04X\n", hm2->pwmgen.enable_addr);
    for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
        HM2_PRINT("    instance %d:\n", i);
        HM2_PRINT("        hw:\n");
        HM2_PRINT(
            "            pwm_val = 0x%08X (%s%d)\n",
            hm2->pwmgen.pwm_value_reg[i],
            ((hm2->pwmgen.pwm_value_reg[i] & 0x80000000) ? "-" : ""),
            ((hm2->pwmgen.pwm_value_reg[i]>>16) & 0x7fff)
        );
        HM2_PRINT("            pwm_mode = 0x%08X\n", hm2->pwmgen.pwm_mode_reg[i]);
    }
}




void hm2_pwmgen_prepare_tram_write(hostmot2_t *hm2) {
    int i;

    if (hm2->pwmgen.num_instances <= 0) return;

    for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
        double scaled_value;
        double abs_duty_cycle;
        int bits;

        // Normally the PWM & Dir IO pins of the pwmgen instance keep doing
        // their thing even if the enable bit is low.  This is because the
        // downstream equipment *should* ignore PWM & Dir if /Enable is
        // high (this is how it handles bootup & watchdog bites).
        // However, there apparently is equipment that does not behave this
        // way, and that benefits from having PWM & Dir go low when /Enable
        // goes high.  So...
        if (*hm2->pwmgen.instance[i].hal.pin.enable == 0) {
            hm2->pwmgen.pwm_value_reg[i] = 0;
            continue;
        }

        scaled_value = *hm2->pwmgen.instance[i].hal.pin.value / hm2->pwmgen.instance[i].hal.param.scale;

        abs_duty_cycle = rtapi_fabs(scaled_value);
        if (abs_duty_cycle > 1.0) abs_duty_cycle = 1.0;

        // duty_cycle goes from 0.0 to 1.0, and needs to be puffed out to pwm_bits (if it's pwm) or 12 (if it's pdm)
        if (hm2->pwmgen.instance[i].hal.param.output_type == HM2_PWMGEN_OUTPUT_TYPE_PDM) {
            bits = 12;
        } else {
            bits = hm2->pwmgen.pwm_bits;
        }
        hm2->pwmgen.pwm_value_reg[i] = abs_duty_cycle * (double)((1 << bits) - 1);
        hm2->pwmgen.pwm_value_reg[i] <<= 16;
        if (scaled_value < 0) {
            hm2->pwmgen.pwm_value_reg[i] |= (1 << 31);
        }
    }
}

