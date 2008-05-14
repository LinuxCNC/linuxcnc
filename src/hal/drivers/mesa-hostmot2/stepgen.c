
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


#define fperiod (period * 1e-9)




// 
// read accumulator to figure out where the stepper has gotten to
// 

void hm2_stepgen_process_tram_read(hostmot2_t *hm2, long period) {
    int i;
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        u32 acc = hm2->stepgen.accumulator_reg[i];
        s32 counts_delta;
        double pos_delta;

        // those tricky users are always trying to get us to divide by zero
        if (fabs(hm2->stepgen.instance[i].hal.param.position_scale) < 1e-10) {
            WARN("stepgen %d position_scale is too small, resetting to 1\n", i);
            hm2->stepgen.instance[i].hal.param.position_scale = 1.0;
        }

        counts_delta = (acc >> 16) - (hm2->stepgen.instance[i].prev_accumulator >> 16);
        if (counts_delta > 32768) {
            counts_delta -= 65536;
        } else if (counts_delta < -32768) {
            counts_delta += 65536;
        }
        
        pos_delta = (acc / 65536.0) - (hm2->stepgen.instance[i].prev_accumulator / 65536.0);
        if (pos_delta > 32768.0) {
            pos_delta -= 65536.0;
        } else if (pos_delta < -32768.0) {
            pos_delta += 65536.0;
        }

        *(hm2->stepgen.instance[i].hal.pin.counts) += counts_delta;
        *(hm2->stepgen.instance[i].hal.pin.position_fb) = *(hm2->stepgen.instance[i].hal.pin.counts) / hm2->stepgen.instance[i].hal.param.position_scale;
        *(hm2->stepgen.instance[i].hal.pin.velocity_fb) = pos_delta / hm2->stepgen.instance[i].hal.param.position_scale / fperiod;

        hm2->stepgen.instance[i].prev_accumulator = hm2->stepgen.accumulator_reg[i];
    }
}


// 
// FIXME: the rate setting below needs work
// should it be in full.fractional step count units instead of position units?
//
void hm2_stepgen_prepare_tram_write(hostmot2_t *hm2, long period) {
    int i;
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        double error = *(hm2->stepgen.instance[i].hal.pin.position_fb) - *(hm2->stepgen.instance[i].hal.pin.position_cmd);

        // FIXME: this deadband is bogus
        if (fabs(error) < 0.0005) {
            hm2->stepgen.step_rate_reg[i] = 0;
        } else {
            hm2->stepgen.step_rate_reg[i] = -1 * error * hm2->stepgen.instance[i].hal.param.position_scale / fperiod;
        }
        *(hm2->stepgen.instance[i].hal.pin.rate) = hm2->stepgen.step_rate_reg[i];
        *(hm2->stepgen.instance[i].hal.pin.error) = error;
    }
}


static void hm2_stepgen_update_dir_setup_time(hostmot2_t *hm2, int i) {
    hm2->stepgen.dir_setup_time_reg[i] = hm2->stepgen.instance[i].hal.param.dirsetup * hm2->stepgen.clock_frequency;
    if (hm2->stepgen.dir_setup_time_reg[i] > 0x3FFF) {
        WARN("stepgen %d has invalid dirsetup, resetting to max\n", i);
        hm2->stepgen.dir_setup_time_reg[i] = 0x3FFF;
        hm2->stepgen.instance[i].hal.param.dirsetup = (float)hm2->stepgen.dir_setup_time_reg[i] / hm2->stepgen.clock_frequency;
    }
    hm2->stepgen.instance[i].written_dirsetup = hm2->stepgen.instance[i].hal.param.dirsetup;
}


static void hm2_stepgen_update_dir_hold_time(hostmot2_t *hm2, int i) {
    hm2->stepgen.dir_hold_time_reg[i] = hm2->stepgen.instance[i].hal.param.dirhold * hm2->stepgen.clock_frequency;
    if (hm2->stepgen.dir_hold_time_reg[i] > 0x3FFF) {
        WARN("stepgen %d has invalid dirhold, resetting to max\n", i);
        hm2->stepgen.dir_hold_time_reg[i] = 0x3FFF;
        hm2->stepgen.instance[i].hal.param.dirhold = (float)hm2->stepgen.dir_hold_time_reg[i] / hm2->stepgen.clock_frequency;
    }
    hm2->stepgen.instance[i].written_dirhold = hm2->stepgen.instance[i].hal.param.dirhold;
}


static void hm2_stepgen_update_pulse_idle_width(hostmot2_t *hm2, int i) {
    hm2->stepgen.pulse_idle_width_reg[i] = hm2->stepgen.instance[i].hal.param.stepspace * hm2->stepgen.clock_frequency;
    if (hm2->stepgen.pulse_idle_width_reg[i] > 0x3FFF) {
        WARN("stepgen %d has invalid stepspace, resetting to max\n", i);
        hm2->stepgen.pulse_idle_width_reg[i] = 0x3FFF;
        hm2->stepgen.instance[i].hal.param.stepspace = (float)hm2->stepgen.pulse_idle_width_reg[i] / hm2->stepgen.clock_frequency;
    }
    hm2->stepgen.instance[i].written_stepspace = hm2->stepgen.instance[i].hal.param.stepspace;
}


static void hm2_stepgen_update_pulse_width(hostmot2_t *hm2, int i) {
    hm2->stepgen.pulse_width_reg[i] = hm2->stepgen.instance[i].hal.param.steplen * hm2->stepgen.clock_frequency;
    if (hm2->stepgen.pulse_width_reg[i] > 0x3FFF) {
        WARN("stepgen %d has invalid steplen, resetting to max\n", i);
        hm2->stepgen.pulse_width_reg[i] = 0x3FFF;
        hm2->stepgen.instance[i].hal.param.steplen = (float)hm2->stepgen.pulse_width_reg[i] / hm2->stepgen.clock_frequency;
    }
    hm2->stepgen.instance[i].written_steplen = hm2->stepgen.instance[i].hal.param.steplen;
}


void hm2_stepgen_write(hostmot2_t *hm2) {
    int i;

    // FIXME
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        if (hm2->stepgen.instance[i].hal.param.dirsetup != hm2->stepgen.instance[i].written_dirsetup) {
            hm2_stepgen_update_dir_setup_time(hm2, i);
            hm2->llio->write(hm2->llio, hm2->stepgen.dir_setup_time_addr + (i * sizeof(u32)), &hm2->stepgen.dir_setup_time_reg[i], sizeof(u32));
        }

        if (hm2->stepgen.instance[i].hal.param.dirhold != hm2->stepgen.instance[i].written_dirhold) {
            hm2_stepgen_update_dir_hold_time(hm2, i);
            hm2->llio->write(hm2->llio, hm2->stepgen.dir_hold_time_addr + (i * sizeof(u32)), &hm2->stepgen.dir_hold_time_reg[i], sizeof(u32));
        }

        if (hm2->stepgen.instance[i].hal.param.steplen != hm2->stepgen.instance[i].written_steplen) {
            hm2_stepgen_update_pulse_width(hm2, i);
            hm2->llio->write(hm2->llio, hm2->stepgen.pulse_width_addr + (i * sizeof(u32)), &hm2->stepgen.pulse_width_reg[i], sizeof(u32));
        }

        if (hm2->stepgen.instance[i].hal.param.stepspace != hm2->stepgen.instance[i].written_stepspace) {
            hm2_stepgen_update_pulse_idle_width(hm2, i);
            hm2->llio->write(hm2->llio, hm2->stepgen.pulse_idle_width_addr + (i * sizeof(u32)), &hm2->stepgen.pulse_idle_width_reg[i], sizeof(u32));
        }
    }
}


static void hm2_stepgen_force_write_mode(hostmot2_t *hm2) {
    int size = hm2->stepgen.num_instances * sizeof(u32);
    hm2->llio->write(hm2->llio, hm2->stepgen.mode_addr, hm2->stepgen.mode_reg, size);
}


static void hm2_stepgen_force_write_dir_setup_time(hostmot2_t *hm2) {
    int i;
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        hm2_stepgen_update_dir_setup_time(hm2, i);
    }
    hm2->llio->write(
        hm2->llio,
        hm2->stepgen.dir_setup_time_addr,
        hm2->stepgen.dir_setup_time_reg,
        (hm2->stepgen.num_instances * sizeof(u32))
    );
    
}


static void hm2_stepgen_force_write_dir_hold_time(hostmot2_t *hm2) {
    int i;
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        hm2_stepgen_update_dir_hold_time(hm2, i);
    }
    hm2->llio->write(
        hm2->llio,
        hm2->stepgen.dir_hold_time_addr,
        hm2->stepgen.dir_hold_time_reg,
        (hm2->stepgen.num_instances * sizeof(u32))
    );
}


static void hm2_stepgen_force_write_pulse_idle_width(hostmot2_t *hm2) {
    int i;
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        hm2_stepgen_update_pulse_idle_width(hm2, i);
    }
    hm2->llio->write(
        hm2->llio,
        hm2->stepgen.pulse_idle_width_addr,
        hm2->stepgen.pulse_idle_width_reg,
        (hm2->stepgen.num_instances * sizeof(u32))
    );
}


static void hm2_stepgen_force_write_pulse_width_time(hostmot2_t *hm2) {
    int i;
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        hm2_stepgen_update_pulse_width(hm2, i);
    }
    hm2->llio->write(
        hm2->llio,
        hm2->stepgen.pulse_width_addr,
        hm2->stepgen.pulse_width_reg,
        (hm2->stepgen.num_instances * sizeof(u32))
    );
}


void hm2_stepgen_force_write(hostmot2_t *hm2) {
    hm2_stepgen_force_write_mode(hm2);
    hm2_stepgen_force_write_dir_setup_time(hm2);
    hm2_stepgen_force_write_dir_hold_time(hm2);
    hm2_stepgen_force_write_pulse_width_time(hm2);
    hm2_stepgen_force_write_pulse_idle_width(hm2);
}




void hm2_stepgen_tram_init(hostmot2_t *hm2) {
    int i;

    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        hm2->stepgen.instance[i].prev_accumulator = hm2->stepgen.accumulator_reg[i];
    }
}




int hm2_stepgen_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r;

    if (hm2->config.num_stepgens == 0) {
        INFO("num_stepgens=0, skipping stepgen MD\n");
        return 0;
    }

    if (!hm2_md_is_consistent(hm2, md_index, 0, 10, 4, 0x01FF)) {
        ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }

    if (hm2->stepgen.num_instances != 0) {
        ERR(
            "Module Descriptor contains duplicate %s (inconsistent firmware), not loading driver\n",
            hm2_get_general_function_name(md->gtag)
        );
        return -EINVAL;
    }

    if (hm2->config.num_stepgens == -1) {
        hm2->stepgen.num_instances = md->instances;
    } else if (hm2->config.num_stepgens > md->instances) {
        ERR(
            "config.num_stepgens=%d, but only %d are available, not loading driver\n",
            hm2->config.num_stepgens,
            md->instances
        );
        return -1;
    } else {
        hm2->stepgen.num_instances = hm2->config.num_stepgens;
    }

    hm2->stepgen.instance = (hm2_stepgen_instance_t *)hal_malloc(hm2->stepgen.num_instances * sizeof(hm2_stepgen_instance_t));
    if (hm2->stepgen.instance == NULL) {
        ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    hm2->stepgen.clock_frequency = md->clock_freq;
    hm2->stepgen.version = md->version;

    hm2->stepgen.step_rate_addr = md->base_address + (0 * md->register_stride);
    hm2->stepgen.accumulator_addr = md->base_address + (1 * md->register_stride);
    hm2->stepgen.mode_addr = md->base_address + (2 * md->register_stride);
    hm2->stepgen.dir_setup_time_addr = md->base_address + (3 * md->register_stride);
    hm2->stepgen.dir_hold_time_addr = md->base_address + (4 * md->register_stride);
    hm2->stepgen.pulse_width_addr = md->base_address + (5 * md->register_stride);
    hm2->stepgen.pulse_idle_width_addr = md->base_address + (6 * md->register_stride);
    hm2->stepgen.table_sequence_data_setup_addr = md->base_address + (7 * md->register_stride);
    hm2->stepgen.table_sequence_length_addr = md->base_address + (8 * md->register_stride);

    r = hm2_register_tram_write_region(hm2, hm2->stepgen.step_rate_addr, (hm2->stepgen.num_instances * sizeof(u32)), &hm2->stepgen.step_rate_reg);
    if (r < 0) {
        ERR("error registering tram write region for StepGen Step Rate register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_read_region(hm2, hm2->stepgen.accumulator_addr, (hm2->stepgen.num_instances * sizeof(u32)), &hm2->stepgen.accumulator_reg);
    if (r < 0) {
        ERR("error registering tram read region for StepGen Accumulator register (%d)\n", r);
        goto fail0;
    }

    hm2->stepgen.mode_reg = (u32 *)kmalloc(hm2->stepgen.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->stepgen.mode_reg == NULL) {
        ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    hm2->stepgen.dir_setup_time_reg = (u32 *)kmalloc(hm2->stepgen.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->stepgen.dir_setup_time_reg == NULL) {
        ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail1;
    }

    hm2->stepgen.dir_hold_time_reg = (u32 *)kmalloc(hm2->stepgen.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->stepgen.dir_hold_time_reg == NULL) {
        ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail2;
    }

    hm2->stepgen.pulse_width_reg = (u32 *)kmalloc(hm2->stepgen.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->stepgen.pulse_width_reg == NULL) {
        ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail3;
    }

    hm2->stepgen.pulse_idle_width_reg = (u32 *)kmalloc(hm2->stepgen.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->stepgen.pulse_idle_width_reg == NULL) {
        ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail4;
    }


    // export to HAL
    {
        int i;
        char name[HAL_NAME_LEN + 2];

        for (i = 0; i < hm2->stepgen.num_instances; i ++) {
            // pins
            rtapi_snprintf(name, HAL_NAME_LEN, "%s.stepgen.%02d.position-cmd", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->stepgen.instance[i].hal.pin.position_cmd), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding pin '%s', aborting\n", name);
                r = -ENOMEM;
                goto fail5;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.stepgen.%02d.position-fb", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_OUT, &(hm2->stepgen.instance[i].hal.pin.position_fb), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding pin '%s', aborting\n", name);
                r = -ENOMEM;
                goto fail5;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.stepgen.%02d.velocity-fb", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_OUT, &(hm2->stepgen.instance[i].hal.pin.velocity_fb), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding pin '%s', aborting\n", name);
                r = -ENOMEM;
                goto fail5;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.stepgen.%02d.counts", hm2->llio->name, i);
            r = hal_pin_s32_new(name, HAL_OUT, &(hm2->stepgen.instance[i].hal.pin.counts), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding pin '%s', aborting\n", name);
                r = -ENOMEM;
                goto fail5;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.stepgen.%02d.rate", hm2->llio->name, i);
            r = hal_pin_u32_new(name, HAL_OUT, &(hm2->stepgen.instance[i].hal.pin.rate), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding pin '%s', aborting\n", name);
                r = -ENOMEM;
                goto fail5;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.stepgen.%02d.error", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_OUT, &(hm2->stepgen.instance[i].hal.pin.error), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding pin '%s', aborting\n", name);
                r = -ENOMEM;
                goto fail5;
            }

            // parameters
            rtapi_snprintf(name, HAL_NAME_LEN, "%s.stepgen.%02d.position-scale", hm2->llio->name, i);
            r = hal_param_float_new(name, HAL_RW, &(hm2->stepgen.instance[i].hal.param.position_scale), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding param '%s', aborting\n", name);
                r = -ENOMEM;
                goto fail5;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.stepgen.%02d.steplen", hm2->llio->name, i);
            r = hal_param_float_new(name, HAL_RW, &(hm2->stepgen.instance[i].hal.param.steplen), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding param '%s', aborting\n", name);
                r = -ENOMEM;
                goto fail5;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.stepgen.%02d.stepspace", hm2->llio->name, i);
            r = hal_param_float_new(name, HAL_RW, &(hm2->stepgen.instance[i].hal.param.stepspace), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding param '%s', aborting\n", name);
                r = -ENOMEM;
                goto fail5;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.stepgen.%02d.dirsetup", hm2->llio->name, i);
            r = hal_param_float_new(name, HAL_RW, &(hm2->stepgen.instance[i].hal.param.dirsetup), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding param '%s', aborting\n", name);
                r = -ENOMEM;
                goto fail5;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.stepgen.%02d.dirhold", hm2->llio->name, i);
            r = hal_param_float_new(name, HAL_RW, &(hm2->stepgen.instance[i].hal.param.dirhold), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding param '%s', aborting\n", name);
                r = -ENOMEM;
                goto fail5;
            }

            // init
            *(hm2->stepgen.instance[i].hal.pin.position_cmd) = 0.0;
            *(hm2->stepgen.instance[i].hal.pin.counts) = 0;
            *(hm2->stepgen.instance[i].hal.pin.position_fb) = 0.0;
            *(hm2->stepgen.instance[i].hal.pin.velocity_fb) = 0.0;

            hm2->stepgen.instance[i].hal.param.position_scale = 1.0;

            // start out slow, let the user speed up if they want
            hm2->stepgen.instance[i].hal.param.steplen = (float)0x3FFF / hm2->stepgen.clock_frequency;
            hm2->stepgen.instance[i].hal.param.stepspace = (float)0x3FFF / hm2->stepgen.clock_frequency;
            hm2->stepgen.instance[i].hal.param.dirsetup = (float)0x3FFF / hm2->stepgen.clock_frequency;
            hm2->stepgen.instance[i].hal.param.dirhold = (float)0x3FFF / hm2->stepgen.clock_frequency;

            hm2->stepgen.instance[i].written_steplen = 0.0;
            hm2->stepgen.instance[i].written_stepspace = 0.0;
            hm2->stepgen.instance[i].written_dirsetup = 0.0;
            hm2->stepgen.instance[i].written_dirhold = 0.0;

            hm2->stepgen.mode_reg[i] = 0;  // step/dir

            hm2->stepgen.instance[i].prev_accumulator = 0;
        }
    }


    return hm2->stepgen.num_instances;


fail5:
    kfree(hm2->stepgen.pulse_idle_width_reg);

fail4:
    kfree(hm2->stepgen.pulse_width_reg);

fail3:
    kfree(hm2->stepgen.dir_hold_time_reg);

fail2:
    kfree(hm2->stepgen.dir_setup_time_reg);

fail1:
    kfree(hm2->stepgen.mode_reg);

fail0:
    hm2->stepgen.num_instances = 0;
    return r;
}




void hm2_stepgen_print_module(int msg_level, hostmot2_t *hm2) {
    int i;
    PRINT(msg_level, "StepGen: %d\n", hm2->stepgen.num_instances);
    if (hm2->stepgen.num_instances <= 0) return;
    PRINT(msg_level, "    clock_frequency: %d Hz (%s MHz)\n", hm2->stepgen.clock_frequency, hm2_hz_to_mhz(hm2->stepgen.clock_frequency));
    PRINT(msg_level, "    version: %d\n", hm2->stepgen.version);
    PRINT(msg_level, "    step_rate_addr: 0x%04X\n", hm2->stepgen.step_rate_addr);
    PRINT(msg_level, "    accumulator_addr: 0x%04X\n", hm2->stepgen.accumulator_addr);
    PRINT(msg_level, "    mode_addr: 0x%04X\n", hm2->stepgen.mode_addr);
    PRINT(msg_level, "    dir_setup_time_addr: 0x%04X\n", hm2->stepgen.dir_setup_time_addr);
    PRINT(msg_level, "    dir_hold_time_addr: 0x%04X\n", hm2->stepgen.dir_hold_time_addr);
    PRINT(msg_level, "    pulse_width_addr: 0x%04X\n", hm2->stepgen.pulse_width_addr);
    PRINT(msg_level, "    pulse_idle_width_addr: 0x%04X\n", hm2->stepgen.pulse_idle_width_addr);
    PRINT(msg_level, "    table_sequence_data_setup_addr: 0x%04X\n", hm2->stepgen.table_sequence_data_setup_addr);
    PRINT(msg_level, "    table_sequence_length_addr: 0x%04X\n", hm2->stepgen.table_sequence_length_addr);
    PRINT(msg_level, "    master_dds_addr: 0x%04X\n", hm2->stepgen.master_dds_addr);
    for (i = 0; i < hm2->stepgen.num_instances; i ++) {
        PRINT(msg_level, "    instance %d:\n", i);
        PRINT(msg_level, "        hw:\n");
        PRINT(msg_level, "            step_rate = 0x%08X\n", hm2->stepgen.step_rate_reg[i]);
        PRINT(msg_level, "            accumulator = 0x%08X\n", hm2->stepgen.accumulator_reg[i]);
        PRINT(msg_level, "            mode = 0x%08X\n", hm2->stepgen.mode_reg[i]);
        PRINT(msg_level, "            dir_setup_time = 0x%08X\n", hm2->stepgen.dir_setup_time_reg[i]);
        PRINT(msg_level, "            dir_hold_time = 0x%08X\n", hm2->stepgen.dir_hold_time_reg[i]);
        PRINT(msg_level, "            pulse_width = 0x%08X\n", hm2->stepgen.pulse_width_reg[i]);
        PRINT(msg_level, "            pulse_idle_width = 0x%08X\n", hm2->stepgen.pulse_idle_width_reg[i]);
    }
}

