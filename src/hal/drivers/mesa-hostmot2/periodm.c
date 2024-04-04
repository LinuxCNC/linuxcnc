
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

#include <rtapi_slab.h>

#include "rtapi.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "hal.h"

#include "hal/drivers/mesa-hostmot2/hostmot2.h"






void hm2_periodm_update_limit(hostmot2_t *hm2, int i) {
    hm2->periodm.limit_reg[i] =  ((double)hm2->periodm.clock_frequency / (double)*hm2->periodm.instance[i].hal.pin.minfreq) * *hm2->periodm.instance[i].hal.pin.averages;
    if ((((double)hm2->periodm.clock_frequency / (double)*hm2->periodm.instance[i].hal.pin.minfreq) * *hm2->periodm.instance[i].hal.pin.averages ) > 0xFFFFFFFF) {
        HM2_ERR("periodm %d has invalid min freq time, resetting to min\n", i);
        *hm2->periodm.instance[i].hal.pin.minfreq = (0.025 * *hm2->periodm.instance[i].hal.pin.averages);
        hm2->periodm.limit_reg[i] =  ((double)hm2->periodm.clock_frequency / (double)*hm2->periodm.instance[i].hal.pin.minfreq) * *hm2->periodm.instance[i].hal.pin.averages; 
    }
}

void hm2_periodm_update_mode(hostmot2_t *hm2, int i) {
    rtapi_u32 modebuff;
    rtapi_u32 filterclocks;
    rtapi_u32  averages;
    filterclocks = (double)*hm2->periodm.instance[i].hal.pin.filtertc * ((double)hm2->periodm.clock_frequency / (double)1e6);
    if (filterclocks > 0xFFFF) {
        HM2_ERR("periodm %d has invalid filter time constant, resetting to max\n", i);
        filterclocks = 0xFFFF;
        *hm2->periodm.instance[i].hal.pin.filtertc = (double)filterclocks * ((double)1e6 / (double)hm2->periodm.clock_frequency);
    }
    if (*hm2->periodm.instance[i].hal.pin.averages > 0xFFF) {
        HM2_ERR("periodm %d has invalid averages number, resetting to max\n", i);
        *hm2->periodm.instance[i].hal.pin.averages =  0xFFF;
    }    
    if (*hm2->periodm.instance[i].hal.pin.averages < 1) {
        HM2_ERR("periodm %d has invalid averages number, resetting to min\n", i);
        *hm2->periodm.instance[i].hal.pin.averages =  1;
    }    
    averages =  *hm2->periodm.instance[i].hal.pin.averages -1;    
    modebuff = 0;
    modebuff |= (*hm2->periodm.instance[i].hal.pin.polarity << 0);
    modebuff |= (averages << 4);
    modebuff |= (filterclocks << 16);
 
	 hm2->periodm.mode_write_reg[i] = modebuff;
}

void hm2_periodm_update_regs(hostmot2_t *hm2) {
    int i;

    if (hm2->periodm.num_instances == 0) return;

    for (i = 0; i < hm2->periodm.num_instances; i ++) {
        hm2_periodm_update_mode(hm2,i);
        hm2_periodm_update_limit(hm2,i);
    }
}


void hm2_periodm_force_write(hostmot2_t *hm2) {
    hm2_periodm_update_regs(hm2);
    
    hm2->llio->write(hm2->llio, hm2->periodm.mode_write_addr, hm2->periodm.mode_write_reg, (hm2->periodm.num_instances * sizeof(rtapi_u32)));
    hm2->llio->write(hm2->llio, hm2->periodm.limit_addr, hm2->periodm.limit_reg, (hm2->periodm.num_instances * sizeof(rtapi_u32)));
    
    if ((*hm2->llio->io_error) != 0) return;

}




//
// Since these are all pins, write is the same as force write 
//

void hm2_periodm_write(hostmot2_t *hm2) {
    if (hm2->periodm.num_instances == 0) return;
    hm2_periodm_force_write(hm2);
}




int hm2_periodm_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r;

    // 
    // some standard sanity checks
    //

    if (!hm2_md_is_consistent_or_complain(hm2, md_index, 0, 4, 4, 0x000F)) {
        HM2_ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }

    if (hm2->periodm.num_instances != 0) {
        HM2_ERR(
            "found duplicate Module Descriptor for %s (inconsistent firmware), not loading driver\n",
            hm2_get_general_function_name(md->gtag)
        );
        return -EINVAL;
    }

    if (hm2->config.num_periodms > md->instances) {
        HM2_ERR(
            "config.num_periodms=%d, but only %d are available, not loading driver\n",
            hm2->config.num_periodms,
            md->instances
        );
        return -EINVAL;
    }

    if (hm2->config.num_periodms == 0) {
        return 0;
    }


    // 
    // looks good, start initializing
    // 

    if (hm2->config.num_periodms == -1) {
        hm2->periodm.num_instances = md->instances;
     } else {
        hm2->periodm.num_instances = hm2->config.num_periodms;
    }



    hm2->periodm.instance = (hm2_periodm_instance_t *)hal_malloc(hm2->periodm.num_instances * sizeof(hm2_periodm_instance_t));
    if (hm2->periodm.instance == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    hm2->periodm.clock_frequency = md->clock_freq;
    hm2->periodm.version = md->version;

    hm2->periodm.mode_read_addr = md->base_address + (0 * md->register_stride);
    hm2->periodm.mode_write_addr = md->base_address + (0 * md->register_stride);
    hm2->periodm.period_addr = md->base_address + (1 * md->register_stride);
    hm2->periodm.width_addr = md->base_address + (2 * md->register_stride);
    hm2->periodm.limit_addr = md->base_address + (3 * md->register_stride);

    r = hm2_register_tram_read_region(hm2, hm2->periodm.mode_read_addr, (hm2->periodm.num_instances * sizeof(rtapi_u32)), &hm2->periodm.mode_read_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for moderegister (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_read_region(hm2, hm2->periodm.period_addr, (hm2->periodm.num_instances * sizeof(rtapi_u32)), &hm2->periodm.period_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for period register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_read_region(hm2, hm2->periodm.width_addr, (hm2->periodm.num_instances * sizeof(rtapi_u32)), &hm2->periodm.width_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for width register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_write_region(hm2, hm2->periodm.mode_write_addr, (hm2->periodm.num_instances * sizeof(rtapi_u32)), &hm2->periodm.mode_write_reg);
    if (r < 0) {
        HM2_ERR("error registering tram write region for mode register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_write_region(hm2, hm2->periodm.limit_addr, (hm2->periodm.num_instances * sizeof(rtapi_u32)), &hm2->periodm.limit_reg);
    if (r < 0) {
        HM2_ERR("error registering tram write region for limit register (%d)\n", r);
        goto fail0;
    }


    // export to HAL
    // FIXME: r hides the r in enclosing function, and it returns the wrong thing
    {
        int i;
        int r;
        char name[HAL_NAME_LEN + 1];


        for (i = 0; i < hm2->periodm.num_instances; i ++) {
            // pins
            rtapi_snprintf(name, sizeof(name), "%s.periodm.%02d.period_us", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_OUT, &(hm2->periodm.instance[i].hal.pin.period), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.periodm.%02d.width_us", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_OUT, &(hm2->periodm.instance[i].hal.pin.width), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }
 
            rtapi_snprintf(name, sizeof(name), "%s.periodm.%02d.duty_cycle", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_OUT, &(hm2->periodm.instance[i].hal.pin.dutycycle), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.periodm.%02d.duty_cycle_scale", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->periodm.instance[i].hal.pin.dutyscale), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.periodm.%02d.duty_cycle_offset", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->periodm.instance[i].hal.pin.dutyoffset), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.periodm.%02d.frequency", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_OUT, &(hm2->periodm.instance[i].hal.pin.frequency), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.periodm.%02d.filtertc_us", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->periodm.instance[i].hal.pin.filtertc), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.periodm.%02d.minimum_frequency", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->periodm.instance[i].hal.pin.minfreq), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.periodm.%02d.averages", hm2->llio->name, i);
            r = hal_pin_u32_new(name, HAL_IO, &(hm2->periodm.instance[i].hal.pin.averages), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.periodm.%02d.invert", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IO, &(hm2->periodm.instance[i].hal.pin.polarity), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.periodm.%02d.valid", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_OUT, &(hm2->periodm.instance[i].hal.pin.valid), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.periodm.%02d.input_status", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_OUT, &(hm2->periodm.instance[i].hal.pin.input), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            // init hal objects
            *(hm2->periodm.instance[i].hal.pin.polarity) = 0;            
            *(hm2->periodm.instance[i].hal.pin.filtertc) = 1.0;
            *(hm2->periodm.instance[i].hal.pin.averages) = 1;
            *(hm2->periodm.instance[i].hal.pin.minfreq) = 1.0;
            *(hm2->periodm.instance[i].hal.pin.dutyscale) = 100;
            *(hm2->periodm.instance[i].hal.pin.dutyoffset) = 0;
            
        }
    }

    return hm2->periodm.num_instances;


fail1:
    rtapi_kfree(hm2->periodm.mode_read_reg);

fail0:
    hm2->periodm.num_instances = 0;
    return r;
}




void hm2_periodm_cleanup(hostmot2_t *hm2) {
    if (hm2->periodm.num_instances <= 0) return;
    if (hm2->periodm.mode_read_reg != NULL) {
        rtapi_kfree(hm2->periodm.mode_read_reg);
        hm2->periodm.mode_read_reg = NULL;
    }
    hm2->periodm.num_instances = 0;
}




void hm2_periodm_print_module(hostmot2_t *hm2) {
    int i;
    if (hm2->periodm.num_instances <= 0) return;
    HM2_PRINT("periodms: %d\n", hm2->periodm.num_instances);
    HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n", hm2->periodm.clock_frequency, hm2_hz_to_mhz(hm2->periodm.clock_frequency));
    HM2_PRINT("    version: %d\n", hm2->periodm.version);
    HM2_PRINT("    mode_addr: 0x%04X\n", hm2->periodm.mode_read_addr);
    HM2_PRINT("    period_addr: 0x%04X\n", hm2->periodm.period_addr);
    HM2_PRINT("    width_addr: 0x%04X\n", hm2->periodm.width_addr);
    HM2_PRINT("    limit_addr: 0x%04X\n", hm2->periodm.limit_addr);

    for (i = 0; i < hm2->periodm.num_instances; i ++) {
        HM2_PRINT("        mode_reg = 0x%08X\n", hm2->periodm.mode_read_reg[i]);
        HM2_PRINT("        period_reg = 0x%08X\n", hm2->periodm.period_reg[i]);
        HM2_PRINT("        width_reg = 0x%08X\n", hm2->periodm.width_reg[i]);
        HM2_PRINT("        limit_reg = 0x%08X\n", hm2->periodm.limit_reg[i]);
    }
}




void hm2_periodm_prepare_tram_write(hostmot2_t *hm2) {
    if (hm2->periodm.num_instances <= 0) return;
    hm2_periodm_update_regs(hm2);
}

void hm2_periodm_process_tram_read(hostmot2_t *hm2) {
    int i;
    rtapi_u32 mode = 0;
    for (i = 0; i < hm2->periodm.num_instances; i ++) {
        mode = hm2->periodm.mode_read_reg[i];
//        *hm2->periodm.instance[i].hal.pin.polarity = ((mode & (1 << 0)) != 0);
        *hm2->periodm.instance[i].hal.pin.valid = ((mode & (1 << 1)) != 0);
        *hm2->periodm.instance[i].hal.pin.input = ((mode & (1 << 2)) != 0);

        *hm2->periodm.instance[i].hal.pin.period = (hm2->periodm.period_reg[i] / ((double)hm2->periodm.clock_frequency)) * (double)1e6 / *hm2->periodm.instance[i].hal.pin.averages; 
        *hm2->periodm.instance[i].hal.pin.width  = (hm2->periodm.width_reg[i]  / ((double)hm2->periodm.clock_frequency)) * (double)1e6 / *hm2->periodm.instance[i].hal.pin.averages; 
			if (*hm2->periodm.instance[i].hal.pin.period !=  0) {
			    *hm2->periodm.instance[i].hal.pin.dutycycle =  (((double)*hm2->periodm.instance[i].hal.pin.width
			     / (double)*hm2->periodm.instance[i].hal.pin.period) * (double)*hm2->periodm.instance[i].hal.pin.dutyscale) + *hm2->periodm.instance[i].hal.pin.dutyoffset;
			    *hm2->periodm.instance[i].hal.pin.frequency =  (1.0 / (hm2->periodm.period_reg[i] / (double)hm2->periodm.clock_frequency)) * *hm2->periodm.instance[i].hal.pin.averages;
			}   

    }    
}



