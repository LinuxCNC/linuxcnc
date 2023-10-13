
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




void hm2_oneshot_update_width1(hostmot2_t *hm2, int i) {
    // widths are in ms
    hm2->oneshot.width1_reg[i] = (double)*hm2->oneshot.instance[i].hal.pin.width1 * ((double)hm2->oneshot.clock_frequency / (double)1e3);
    if (hm2->oneshot.width1_reg[i] > 0x7FFFFFFF) {
        HM2_ERR("oneshot %d has invalid width1, resetting to max\n", i);
        hm2->oneshot.width1_reg[i] = 0x7FFFFFFF;
        *hm2->oneshot.instance[i].hal.pin.width1 = (double)hm2->oneshot.width1_reg[i] * ((double)1e3 / (double)hm2->oneshot.clock_frequency);
    }
}

void hm2_oneshot_update_width2(hostmot2_t *hm2, int i) {
    // widths are in ms
    hm2->oneshot.width2_reg[i] = (double)*hm2->oneshot.instance[i].hal.pin.width2 * ((double)hm2->oneshot.clock_frequency / (double)1e3);
    if (hm2->oneshot.width2_reg[i] > 0x7FFFFFFF) {
        HM2_ERR("oneshot %d has invalid width1, resetting to max\n", i);
        hm2->oneshot.width2_reg[i] = 0x7FFFFFFF;
        *hm2->oneshot.instance[i].hal.pin.width2 = (double)hm2->oneshot.width2_reg[i] * ((double)1e3 / (double)hm2->oneshot.clock_frequency);
    }
}

void hm2_oneshot_update_filter1(hostmot2_t *hm2, int i) {
    // widths are in ms
    hm2->oneshot.filter1_reg[i] = (double)*hm2->oneshot.instance[i].hal.pin.filter1 * ((double)hm2->oneshot.clock_frequency / (double)1e3);
    if (hm2->oneshot.filter1_reg[i] > 0xFFFFFF) {
        HM2_ERR("oneshot %d has invalid filter1 time, resetting to max\n", i);
        hm2->oneshot.width1_reg[i] = 0xFFFFFF;
        *hm2->oneshot.instance[i].hal.pin.filter1 = (double)hm2->oneshot.filter1_reg[i] * ((double)1e3 / (double)hm2->oneshot.clock_frequency);
    }
}

void hm2_oneshot_update_filter2(hostmot2_t *hm2, int i) {
    // widths are in ms
    hm2->oneshot.filter2_reg[i] = (double)*hm2->oneshot.instance[i].hal.pin.filter2 * ((double)hm2->oneshot.clock_frequency / (double)1e3);
    if (hm2->oneshot.filter2_reg[i] > 0xFFFFFF) {
        HM2_ERR("oneshot %d has invalid filter2 time, resetting to max\n", i);
        hm2->oneshot.width2_reg[i] = 0xFFFFFF;
        *hm2->oneshot.instance[i].hal.pin.filter2 = (double)hm2->oneshot.filter2_reg[i] * ((double)1e3 / (double)hm2->oneshot.clock_frequency);
    }
} 

void hm2_oneshot_update_rate(hostmot2_t *hm2, int i) {
       hm2->oneshot.rate_reg[i] = (uint32_t)(*hm2->oneshot.instance[i].hal.pin.rate * (4294967296.0 / (double)hm2->oneshot.clock_frequency));
}

void hm2_oneshot_update_control(hostmot2_t *hm2, int i) {
    rtapi_u32 controlbuff;
    if(*hm2->oneshot.instance[i].hal.pin.trigselect1 > 7) {
        HM2_ERR("oneshot %d has invalid trigger 1 select value , resetting to 0\n", i);
		  *hm2->oneshot.instance[i].hal.pin.trigselect1 = 0;
    }     
	 if(*hm2->oneshot.instance[i].hal.pin.trigselect2 > 7) {
	     HM2_ERR("oneshot %d has invalid trigger 2 select value , resetting to 0\n", i);
		  *hm2->oneshot.instance[i].hal.pin.trigselect2 = 0;
    }     
    controlbuff = 0;
    controlbuff |= (*hm2->oneshot.instance[i].hal.pin.trigselect1 << 0);
    controlbuff |= (*hm2->oneshot.instance[i].hal.pin.trigrise1 << 3);
    controlbuff |= (*hm2->oneshot.instance[i].hal.pin.trigfall1 << 4);
    controlbuff |= (*hm2->oneshot.instance[i].hal.pin.retrig1 << 5);
    controlbuff |= (*hm2->oneshot.instance[i].hal.pin.enable1 << 6);
    controlbuff |= (*hm2->oneshot.instance[i].hal.pin.reset1 << 7);
    controlbuff |= (*hm2->oneshot.instance[i].hal.pin.swtrig1 << 10);
    controlbuff |= ((*hm2->oneshot.instance[i].hal.pin.dpll_timer_num & 7) << 12);
    controlbuff |= (*hm2->oneshot.instance[i].hal.pin.trigselect2 << 16);
    controlbuff |= (*hm2->oneshot.instance[i].hal.pin.trigrise2 << 19);
    controlbuff |= (*hm2->oneshot.instance[i].hal.pin.trigfall2 << 20);
    controlbuff |= (*hm2->oneshot.instance[i].hal.pin.retrig2 << 21);
    controlbuff |= (*hm2->oneshot.instance[i].hal.pin.enable2 << 22);
    controlbuff |= (*hm2->oneshot.instance[i].hal.pin.reset2 << 23);
    controlbuff |= (*hm2->oneshot.instance[i].hal.pin.swtrig2 << 26);

	 hm2->oneshot.control_reg[i] = controlbuff;
}

void hm2_oneshot_update_regs(hostmot2_t *hm2) {
    int i;

    if (hm2->oneshot.num_instances == 0) return;

    for (i = 0; i < hm2->oneshot.num_instances; i ++) {
        hm2_oneshot_update_width1(hm2,i);
        hm2_oneshot_update_width2(hm2,i);
        hm2_oneshot_update_filter1(hm2,i);
        hm2_oneshot_update_filter2(hm2,i);
        hm2_oneshot_update_rate(hm2,i);
        hm2_oneshot_update_control(hm2,i);
  
    }
}


void hm2_oneshot_force_write(hostmot2_t *hm2) {
    hm2_oneshot_update_regs(hm2);
    
    hm2->llio->write(hm2->llio, hm2->oneshot.width1_addr, hm2->oneshot.width1_reg, (hm2->oneshot.num_instances * sizeof(rtapi_u32)));
    hm2->llio->write(hm2->llio, hm2->oneshot.width2_addr, hm2->oneshot.width2_reg, (hm2->oneshot.num_instances * sizeof(rtapi_u32)));
    hm2->llio->write(hm2->llio, hm2->oneshot.filter1_addr, hm2->oneshot.filter1_reg, (hm2->oneshot.num_instances * sizeof(rtapi_u32)));
    hm2->llio->write(hm2->llio, hm2->oneshot.filter2_addr, hm2->oneshot.filter2_reg, (hm2->oneshot.num_instances * sizeof(rtapi_u32)));
    hm2->llio->write(hm2->llio, hm2->oneshot.rate_addr, hm2->oneshot.rate_reg, (hm2->oneshot.num_instances * sizeof(rtapi_u32)));
    hm2->llio->write(hm2->llio, hm2->oneshot.control_addr, hm2->oneshot.control_reg, (hm2->oneshot.num_instances * sizeof(rtapi_u32)));

    
    if ((*hm2->llio->io_error) != 0) return;

}




//
// Since these are all pins, write is the same as force write 
//

void hm2_oneshot_write(hostmot2_t *hm2) {

    if (hm2->oneshot.num_instances == 0) return;
    hm2_oneshot_force_write(hm2);
}




int hm2_oneshot_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r;


    // 
    // some standard sanity checks
    //

    if (!hm2_md_is_consistent_or_complain(hm2, md_index, 0, 6, 4, 0x003F)) {
        HM2_ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }

    if (hm2->oneshot.num_instances != 0) {
        HM2_ERR(
            "found duplicate Module Descriptor for %s (inconsistent firmware), not loading driver\n",
            hm2_get_general_function_name(md->gtag)
        );
        return -EINVAL;
    }

    if (hm2->config.num_oneshots > md->instances) {
        HM2_ERR(
            "config.num_oneshots=%d, but only %d are available, not loading driver\n",
            hm2->config.num_oneshots,
            md->instances
        );
        return -EINVAL;
    }

    if (hm2->config.num_oneshots == 0) {
        return 0;
    }


    // 
    // looks good, start initializing
    // 


    if (hm2->config.num_oneshots == -1) {
        hm2->oneshot.num_instances = md->instances;
    } else {
        hm2->oneshot.num_instances = hm2->config.num_oneshots;
    }



    hm2->oneshot.instance = (hm2_oneshot_instance_t *)hal_malloc(hm2->oneshot.num_instances * sizeof(hm2_oneshot_instance_t));
    if (hm2->oneshot.instance == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    hm2->oneshot.clock_frequency = md->clock_freq;
    hm2->oneshot.version = md->version;

    hm2->oneshot.width1_addr = md->base_address + (0 * md->register_stride);
    hm2->oneshot.width2_addr = md->base_address + (1 * md->register_stride);
    hm2->oneshot.filter1_addr = md->base_address + (2 * md->register_stride);
    hm2->oneshot.filter2_addr = md->base_address + (3 * md->register_stride);
    hm2->oneshot.rate_addr = md->base_address + (4 * md->register_stride);
    hm2->oneshot.control_addr = md->base_address + (5 * md->register_stride);
    hm2->oneshot.control_read_addr = md->base_address + (5 * md->register_stride);

    r = hm2_register_tram_write_region(hm2, hm2->oneshot.width1_addr, (hm2->oneshot.num_instances * sizeof(rtapi_u32)), &hm2->oneshot.width1_reg);
    if (r < 0) {
        HM2_ERR("error registering tram write region for Width1 register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_write_region(hm2, hm2->oneshot.width2_addr, (hm2->oneshot.num_instances * sizeof(rtapi_u32)), &hm2->oneshot.width2_reg);
    if (r < 0) {
        HM2_ERR("error registering tram write region for Width2 register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_write_region(hm2, hm2->oneshot.filter1_addr, (hm2->oneshot.num_instances * sizeof(rtapi_u32)), &hm2->oneshot.filter1_reg);
    if (r < 0) {
        HM2_ERR("error registering tram write region for Filter1 register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_write_region(hm2, hm2->oneshot.filter2_addr, (hm2->oneshot.num_instances * sizeof(rtapi_u32)), &hm2->oneshot.filter2_reg);
    if (r < 0) {
        HM2_ERR("error registering tram write region for Filter2 register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_write_region(hm2, hm2->oneshot.rate_addr, (hm2->oneshot.num_instances * sizeof(rtapi_u32)), &hm2->oneshot.rate_reg);
    if (r < 0) {
        HM2_ERR("error registering tram write region for Rate register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_write_region(hm2, hm2->oneshot.control_addr, (hm2->oneshot.num_instances * sizeof(rtapi_u32)), &hm2->oneshot.control_reg);
    if (r < 0) {
        HM2_ERR("error registering tram write region for Control register (%d)\n", r);
        goto fail0;
    }

    r = hm2_register_tram_read_region(hm2, hm2->oneshot.control_read_addr, (hm2->oneshot.num_instances * sizeof(rtapi_u32)), &hm2->oneshot.control_read_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for Control register (%d)\n", r);
        goto fail0;
    }


    // export to HAL
    // FIXME: r hides the r in enclosing function, and it returns the wrong thing
    {
        int i;
        int r;
        char name[HAL_NAME_LEN + 1];


        for (i = 0; i < hm2->oneshot.num_instances; i ++) {
            // pins
            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.width1", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.width1), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.width2", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.width2), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.filter1", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.filter1), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.filter2", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.filter2), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.rate", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.rate), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.trigger_select1", hm2->llio->name, i);
            r = hal_pin_u32_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.trigselect1), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.trigger_select2", hm2->llio->name, i);
            r = hal_pin_u32_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.trigselect2), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.dpll_timer_number", hm2->llio->name, i);
            r = hal_pin_s32_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.dpll_timer_num), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }
  
            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.trigger_on_rise1", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.trigrise1), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.trigger_on_rise2", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.trigrise2), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.trigger_on_fall1", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.trigfall1), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.trigger_on_fall2", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.trigfall2), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.retriggerable1", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.retrig1), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.retriggerable2", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.retrig2), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.enable1", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.enable1), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.enable2", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.enable2), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.reset1", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.reset1), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.reset2", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.reset2), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.swtrigger1", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.swtrig1), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.swtrigger2", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->oneshot.instance[i].hal.pin.swtrig2), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.exttrigger1", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_OUT, &(hm2->oneshot.instance[i].hal.pin.exttrig1), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.exttrigger2", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_OUT, &(hm2->oneshot.instance[i].hal.pin.exttrig2), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }
 
            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.out1", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_OUT, &(hm2->oneshot.instance[i].hal.pin.out1), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, sizeof(name), "%s.oneshot.%02d.out2", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_OUT, &(hm2->oneshot.instance[i].hal.pin.out2), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }


            // init hal objects
            *(hm2->oneshot.instance[i].hal.pin.width1) = 1.0;
            *(hm2->oneshot.instance[i].hal.pin.width2) = 1.0;
            *(hm2->oneshot.instance[i].hal.pin.filter1) = 0.1;
            *(hm2->oneshot.instance[i].hal.pin.filter2) = 0.1;
            *(hm2->oneshot.instance[i].hal.pin.rate) = 1000.0;
            *(hm2->oneshot.instance[i].hal.pin.trigselect1) = 0;
            *(hm2->oneshot.instance[i].hal.pin.trigselect2) = 0;
            *(hm2->oneshot.instance[i].hal.pin.trigrise1) = 0;
            *(hm2->oneshot.instance[i].hal.pin.trigrise2) = 0;
            *(hm2->oneshot.instance[i].hal.pin.trigfall1) = 0;
            *(hm2->oneshot.instance[i].hal.pin.trigfall2) = 0;
            *(hm2->oneshot.instance[i].hal.pin.retrig1) = 0;
            *(hm2->oneshot.instance[i].hal.pin.retrig2) = 0;
            *(hm2->oneshot.instance[i].hal.pin.enable1) = 0;
            *(hm2->oneshot.instance[i].hal.pin.enable2) = 0;
            *(hm2->oneshot.instance[i].hal.pin.reset1) = 0;
            *(hm2->oneshot.instance[i].hal.pin.reset2) = 0;
            *(hm2->oneshot.instance[i].hal.pin.swtrig1) = 0;
            *(hm2->oneshot.instance[i].hal.pin.swtrig2) = 0;
        }
    }


    return hm2->oneshot.num_instances;


fail1:
    rtapi_kfree(hm2->oneshot.control_reg);

fail0:
    hm2->oneshot.num_instances = 0;
    return r;
}




void hm2_oneshot_cleanup(hostmot2_t *hm2) {
    if (hm2->oneshot.num_instances <= 0) return;
    if (hm2->oneshot.control_reg != NULL) {
        rtapi_kfree(hm2->oneshot.control_reg);
        hm2->oneshot.control_reg = NULL;
    }
    hm2->oneshot.num_instances = 0;
}




void hm2_oneshot_print_module(hostmot2_t *hm2) {
    int i;
    if (hm2->oneshot.num_instances <= 0) return;
    HM2_PRINT("OneShots: %d\n", hm2->oneshot.num_instances);
    HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n", hm2->oneshot.clock_frequency, hm2_hz_to_mhz(hm2->oneshot.clock_frequency));
    HM2_PRINT("    version: %d\n", hm2->oneshot.version);
    HM2_PRINT("    width1_addr: 0x%04X\n", hm2->oneshot.width1_addr);
    HM2_PRINT("    width2_addr: 0x%04X\n", hm2->oneshot.width2_addr);
    HM2_PRINT("    filter1_addr: 0x%04X\n", hm2->oneshot.filter1_addr);
    HM2_PRINT("    filter2_addr: 0x%04X\n", hm2->oneshot.filter2_addr);
    HM2_PRINT("    rate_addr: 0x%04X\n", hm2->oneshot.rate_addr);
    HM2_PRINT("    control_addr: 0x%04X\n", hm2->oneshot.control_addr);

    for (i = 0; i < hm2->oneshot.num_instances; i ++) {
        HM2_PRINT("        width1_reg = 0x%08X\n", hm2->oneshot.width1_reg[i]);
        HM2_PRINT("        width2_reg = 0x%08X\n", hm2->oneshot.width2_reg[i]);
        HM2_PRINT("        filter1_reg = 0x%08X\n", hm2->oneshot.filter1_reg[i]);
        HM2_PRINT("        filter2_reg = 0x%08X\n", hm2->oneshot.filter2_reg[i]);
        HM2_PRINT("        rate_reg = 0x%08X\n", hm2->oneshot.rate_reg[i]);
        HM2_PRINT("        control_reg = 0x%08X\n", hm2->oneshot.control_reg[i]);
    }
}




void hm2_oneshot_prepare_tram_write(hostmot2_t *hm2) {

    if (hm2->oneshot.num_instances <= 0) return;
    hm2_oneshot_update_regs(hm2);


}

void hm2_oneshot_process_tram_read(hostmot2_t *hm2) {
    int i;
    rtapi_u32 control = 0;
    for (i = 0; i < hm2->oneshot.num_instances; i ++) {
        control = hm2->oneshot.control_read_reg[i];
        *hm2->oneshot.instance[i].hal.pin.out1 = ((control & (1 << 8)) != 0);
        *hm2->oneshot.instance[i].hal.pin.out2 = ((control & (1 << 24)) != 0);
        *hm2->oneshot.instance[i].hal.pin.exttrig1 = ((control & (1 << 9)) != 0);
        *hm2->oneshot.instance[i].hal.pin.exttrig2 = ((control & (1 << 25)) != 0);
    }    
}



