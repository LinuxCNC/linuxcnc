
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




void hm2_pwmgen_force_write(hostmot2_t *hm2) {
    int i;

    for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
        hm2->pwmgen.pwm_mode_reg[i] = hm2->pwmgen.instance[i].pwm_width_select;
        hm2->pwmgen.pwm_mode_reg[i] |= hm2->pwmgen.instance[i].pwm_mode_select << 2;

        switch (hm2->pwmgen.instance[i].hal.param.output_type) {
            case 1: {  // PWM & Dir
                // leave the Output Mode bits 0
                break;
            }

            case 2: {  // Up & Down
                hm2->pwmgen.pwm_mode_reg[i] |= 0x2 << 3;
                break;
            }

            default: {  // unknown pwm mode!  complain and switch to pwm/dir
                WARN(
                    "invalid pwmgen output_type %d, 1 and 2 are supported, switching to 1\n",
                    hm2->pwmgen.instance[i].hal.param.output_type
                ); 
                hm2->pwmgen.instance[i].hal.param.output_type = 1;
                // leave the Output Mode bits 0
                break;
            }
        }

        hm2->pwmgen.pwm_mode_reg[i] |= hm2->pwmgen.instance[i].pwm_double_buffered << 5;
    }


    // update enable register
    hm2->pwmgen.enable_reg = 0;
    for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
        if (*(hm2->pwmgen.instance[i].hal.pin.enable)) {
            hm2->pwmgen.enable_reg |= (1 << i);
        }
    }


    hm2->llio->write(hm2->llio, hm2->pwmgen.pwm_mode_addr, hm2->pwmgen.pwm_mode_reg, (hm2->encoder.num_instances * sizeof(u32)));
    hm2->llio->write(hm2->llio, hm2->pwmgen.enable_addr, &hm2->pwmgen.enable_reg, sizeof(u32));
    hm2->llio->write(hm2->llio, hm2->pwmgen.pwmgen_master_rate_dds_addr, &hm2->pwmgen.pwmgen_master_rate_dds_reg, sizeof(u32));

    if ((*hm2->llio->io_error) != 0) return;

    for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
        hm2->pwmgen.instance[i].hal.param.written_output_type = hm2->pwmgen.instance[i].hal.param.output_type;
    }
}




//
// Update the PWM Mode Registers of all pwmgen instances that need it
//

void hm2_pwmgen_write(hostmot2_t *hm2) {
    int need_update = 0;
    int i;
    u32 new_enable = 0;

    // check output type
    for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
        if (hm2->pwmgen.instance[i].hal.param.output_type == hm2->pwmgen.instance[i].hal.param.written_output_type) continue;
        need_update = 1;
    }

    // update enable register?
    for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
        if (*(hm2->pwmgen.instance[i].hal.pin.enable)) {
            new_enable |= (1 << i);
        }
    }
    if (new_enable != hm2->pwmgen.enable_reg) {
        need_update = 1;
    }

    if (need_update) {
        hm2_pwmgen_force_write(hm2);
    }
}




int hm2_pwmgen_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r;

    if (hm2->config.num_pwmgens == 0) {
        INFO("num_pwmgens=0, skipping pwmgen MD\n");
        return 0;
    }

    if (!hm2_md_is_consistent(hm2, md_index, 0, 5, 4, 0x0003)) {
        ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }

    if (hm2->pwmgen.num_instances != 0) {
        ERR(
            "Module Descriptor contains duplicate %s (inconsistent firmware), not loading driver\n",
            hm2_get_general_function_name(md->gtag)
        );
        return -EINVAL;
    }

    if (hm2->config.num_pwmgens == -1) {
        hm2->pwmgen.num_instances = md->instances;
    } else if (hm2->config.num_pwmgens > md->instances) {
        ERR(
            "config.num_pwmgens=%d, but only %d are available, not loading driver\n",
            hm2->config.num_pwmgens,
            md->instances
        );
        return -1;
    } else {
        hm2->pwmgen.num_instances = hm2->config.num_pwmgens;
    }

    hm2->pwmgen.instance = (hm2_pwmgen_instance_t *)hal_malloc(hm2->pwmgen.num_instances * sizeof(hm2_pwmgen_instance_t));
    if (hm2->pwmgen.instance == NULL) {
        ERR("out of memory!\n");
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
        ERR("error registering tram write region for PWM Value register (%d)\n", r);
        goto fail0;
    }

    hm2->pwmgen.pwm_mode_reg = (u32 *)kmalloc(hm2->pwmgen.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->pwmgen.pwm_mode_reg == NULL) {
        ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    // export to HAL
    // FIXME: r hides the r in enclosing function, and it returns the wrong thing
    {
        int i;
        int r;
        char name[HAL_NAME_LEN + 2];

        for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
            // pins
            rtapi_snprintf(name, HAL_NAME_LEN, "%s.pwmgen.%02d.value", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->pwmgen.instance[i].hal.pin.value), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.pwmgen.%02d.enable", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->pwmgen.instance[i].hal.pin.enable), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            // parameters

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.pwmgen.%02d.scale", hm2->llio->name, i);
            r = hal_param_float_new(name, HAL_RW, &(hm2->pwmgen.instance[i].hal.param.scale), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding param '%s', aborting\n", name);
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
            if (r != HAL_SUCCESS) {
                ERR("error adding param, aborting\n");
                goto fail1;
            }

            // init hal objects
            *(hm2->pwmgen.instance[i].hal.pin.enable) = 0;
            *(hm2->pwmgen.instance[i].hal.pin.value) = 0.0;
            hm2->pwmgen.instance[i].hal.param.scale = 1.0;
            hm2->pwmgen.instance[i].hal.param.output_type = 1;  // default to PWM+Dir
            hm2->pwmgen.instance[i].hal.param.written_output_type = -666;  // force at update at the start

            // these are the fields of the PWM Mode Register that are not exposed to HAL
            // FIXME: let the user choose some of these bits?
            hm2->pwmgen.instance[i].pwm_width_select = 0x3;
            hm2->pwmgen.instance[i].pwm_mode_select = 0;
            hm2->pwmgen.instance[i].pwm_double_buffered = 1;
        }
    }


    // 
    // set the PWM frequency
    // FIXME: this is broken and should be exported to HAL
    // 
    // 16 bit PWM gen master rate DDS (PWMCLOCK = CLKHIGH*Rate/65536)
    // PWM rate will be PWMCLOCK/(2^PWMBITS) for normal and interleaved PWM
    // and PWMCLOCK/(2^(PWMBITS+1)) for symmetrical mode PWM.
    // 
    // ClockHigh = 100 MHz
    // 7i30 max PWM rate is 50 KHz
    // 
    // PWMClock = 100 MHz * (val / 65536)
    // PWMRate = PWMClock / 2^PWMBits = PWMClock / 2^12 = PWMClock / 4096
    // 
    // PWMRate = 50 Khz = PWMClock / 4096
    // PWMClock = 205 MHz!?
    // 
    // PWMClock = 100 MHz * 65536 / 65536 = 100 MHz
    // PWMRate = 100 MHz / 4096 = 24 KHz
    // 
    // FIXME: maybe use fewer bits?
    // 

    hm2->pwmgen.pwmgen_master_rate_dds_reg = 65535;


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




void hm2_pwmgen_print_module(int msg_level, hostmot2_t *hm2) {
    int i;
    PRINT(msg_level, "PWMGen: %d\n", hm2->pwmgen.num_instances);
    if (hm2->pwmgen.num_instances <= 0) return;
    PRINT(msg_level, "    clock_frequency: %d Hz (%s MHz)\n", hm2->pwmgen.clock_frequency, hm2_hz_to_mhz(hm2->pwmgen.clock_frequency));
    PRINT(msg_level, "    version: %d\n", hm2->pwmgen.version);
    PRINT(msg_level, "    pwmgen_master_rate_dds: 0x%08X (%d)\n", hm2->pwmgen.pwmgen_master_rate_dds_reg, hm2->pwmgen.pwmgen_master_rate_dds_reg);
    PRINT(msg_level, "    pdmgen_master_rate_dds: 0x%08X (%d)\n", hm2->pwmgen.pdmgen_master_rate_dds_reg, hm2->pwmgen.pdmgen_master_rate_dds_reg);
    PRINT(msg_level, "    enable: 0x%08X\n", hm2->pwmgen.enable_reg);
    PRINT(msg_level, "    pwm_value_addr: 0x%04X\n", hm2->pwmgen.pwm_value_addr);
    PRINT(msg_level, "    pwm_mode_addr: 0x%04X\n", hm2->pwmgen.pwm_mode_addr);
    PRINT(msg_level, "    pwmgen_master_rate_dds_addr: 0x%04X\n", hm2->pwmgen.pwmgen_master_rate_dds_addr);
    PRINT(msg_level, "    pdmgen_master_rate_dds_addr: 0x%04X\n", hm2->pwmgen.pdmgen_master_rate_dds_addr);
    PRINT(msg_level, "    enable_addr: 0x%04X\n", hm2->pwmgen.enable_addr);
    for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
        PRINT(msg_level, "    instance %d:\n", i);
        PRINT(msg_level, "        hw:\n");
        PRINT(msg_level, "            pwm_val = 0x%08X\n", hm2->pwmgen.pwm_value_reg[i]);
        PRINT(msg_level, "            pwm_mode = 0x%08X\n", hm2->pwmgen.pwm_mode_reg[i]);
    }
}




void hm2_pwmgen_prepare_tram_write(hostmot2_t *hm2) {
    int i;

    for (i = 0; i < hm2->pwmgen.num_instances; i ++) {
        float val;

        // FIXME
        val = fabs(*(hm2->pwmgen.instance[i].hal.pin.value));
        if (val > 1.0) val = 1.0;
        hm2->pwmgen.pwm_value_reg[i] = val / hm2->pwmgen.instance[i].hal.param.scale;
        hm2->pwmgen.pwm_value_reg[i] <<= 16;
        if (*(hm2->pwmgen.instance[i].hal.pin.value) < 0) {
            hm2->pwmgen.pwm_value_reg[i] |= (1 << 31);
        }
    }
}

