//
// Copyright (C) 2018 Sebastian Kuzminsky
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//
//
// This is a driver for the Hostmot2 rcpwmgen module.
// a module for RC servo control 
//
// Register map:
//
//
// 0x9A00  W/O per channel 17 bit PWM width register 
// PWM Width = (register_value+1)/(ClockLow/16)
// 
//
// 0x9B00  master RCWPM  W/O 32 bit frequency setting divisor
// PWM frequency = (register_value+2)/Clocklow
//

#include <rtapi_slab.h>

#include "rtapi.h"
#include "hal.h"

#include "hal/drivers/mesa-hostmot2/hostmot2.h"


int hm2_rcpwmgen_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r;

    if (hm2->rcpwmgen.num_instances != 0) {
        HM2_ERR(
            "found duplicate Module Descriptor for %s (inconsistent firmware), not loading driver\n",
            hm2_get_general_function_name(md->gtag)
        );
        return -EINVAL;
    }

    if (hm2->config.num_rcpwmgens > md->instances) {
        HM2_ERR(
            "config.num_rcpwmgens=%d, but only %d are available, not loading driver\n",
            hm2->config.num_rcpwmgens,
            md->instances
        );
        return -EINVAL;
    }

    if (hm2->config.num_rcpwmgens == 0) {
        return 0;
    }


    //
    // Looks good, start initializing.
    //

    if (hm2->config.num_rcpwmgens == -1) {
        hm2->rcpwmgen.num_instances = md->instances;
    } else {
        hm2->rcpwmgen.num_instances = hm2->config.num_rcpwmgens;
    }

    hm2->rcpwmgen.clock_frequency = md->clock_freq;
    hm2->rcpwmgen.version = md->version;
    // allocate the module-instance HAL shared memory
    hm2->rcpwmgen.instance = (hm2_rcpwmgen_instance_t *)hal_malloc(hm2->rcpwmgen.num_instances * sizeof(hm2_rcpwmgen_instance_t));
    if (hm2->rcpwmgen.instance == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    // allocate the module-global HAL shared memory
    hm2->rcpwmgen.hal = (hm2_rcpwmgen_module_global_t *)hal_malloc(sizeof(hm2_rcpwmgen_module_global_t));
    if (hm2->rcpwmgen.hal == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }
    
    hm2->rcpwmgen.width_reg = (rtapi_u32 *)rtapi_kmalloc(hm2->rcpwmgen.num_instances * sizeof(rtapi_u32), RTAPI_GFP_KERNEL);
    if (hm2->rcpwmgen.width_reg == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }


    hm2->rcpwmgen.width_addr = md->base_address + (0 * md->register_stride);
    hm2->rcpwmgen.rate_addr = md->base_address + (1 * md->register_stride);
    //

    r = hm2_register_tram_write_region(hm2, hm2->rcpwmgen.width_addr, (hm2->rcpwmgen.num_instances * sizeof(rtapi_u32)), &hm2->rcpwmgen.width_reg);
    if (r < 0) {
        HM2_ERR("error registering tram write region for rcpwmgen width register (%d)\n", r);
        goto fail1;
    }

    //
    // Export to HAL.
    //

    {
        int i;
        char name[HAL_NAME_LEN + 1];
         for (i = 0; i < hm2->rcpwmgen.num_instances; i ++) {
 
            rtapi_snprintf(name, sizeof(name), "%s.rcpwmgen.%02d.width", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->rcpwmgen.instance[i].hal.pin.width), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                r = -ENOMEM;
                goto fail1;
            }
            rtapi_snprintf(name, sizeof(name), "%s.rcpwmgen.%02d.scale", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->rcpwmgen.instance[i].hal.pin.scale), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                r = -ENOMEM;
                goto fail1;
            }
            rtapi_snprintf(name, sizeof(name), "%s.rcpwmgen.%02d.offset", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_IN, &(hm2->rcpwmgen.instance[i].hal.pin.offset), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                r = -ENOMEM;
                goto fail1;
            }
        }

        rtapi_snprintf(name, sizeof(name), "%s.rcpwmgen.rate", hm2->llio->name);
        r = hal_pin_float_new(name, HAL_IN, &(hm2->rcpwmgen.hal->pin.rate), hm2->llio->comp_id);
        if (r < 0) {
            HM2_ERR("error adding pin '%s', aborting\n", name);
            goto fail1;
        }
    }

    // initialize width to 0, scale to 1, offset 0 and rate to 50 Hz
    *hm2->rcpwmgen.hal->pin.rate = 50;
    int i;
    for (i = 0; i < hm2->rcpwmgen.num_instances; i ++) {
       *hm2->rcpwmgen.instance[i].hal.pin.width = 0;
       *hm2->rcpwmgen.instance[i].hal.pin.scale = 1.0;
       *hm2->rcpwmgen.instance[i].hal.pin.offset = 0;
    }
    hm2->rcpwmgen.error_throttle = 0;

    return hm2->rcpwmgen.num_instances;

fail1:
    rtapi_kfree(hm2->rcpwmgen.width_reg);

fail0:
    hm2->rcpwmgen.num_instances = 0;
    return r;
}


void hm2_rcpwmgen_cleanup(hostmot2_t *hm2) {
    if (hm2->rcpwmgen.num_instances <= 0) return;
}



void hm2_rcpwmgen_update_regs(hostmot2_t *hm2) {    
// update hardware register imagesfrom hal pins 

    if (hm2->rcpwmgen.num_instances <= 0) {
        return;
    }

    if  (hm2->rcpwmgen.error_throttle >0) {
         hm2->rcpwmgen.error_throttle-- ;
    }

    rtapi_u32 reg;

    // Set rate
    double rate = *hm2->rcpwmgen.hal->pin.rate;
    if  (rate < 0.01) {
        *hm2->rcpwmgen.hal->pin.rate = 0.01;
        rate = 0.01;
        if (hm2->rcpwmgen.error_throttle == 0) {
            HM2_ERR("rcpwmgen frequency must be >= .01, resetting to %.3lf \n",0.01);
            hm2->rcpwmgen.error_throttle = 100;
        }
    } 

    if (rate > 1000) {
        *hm2->rcpwmgen.hal->pin.rate = 1000;
        rate = 1000;
         if (hm2->rcpwmgen.error_throttle == 0) {
             HM2_ERR("rcpwmgen frequency must be <= 1000, resetting to %.3lf \n",1000.0);
            hm2->rcpwmgen.error_throttle = 100;
         }
    }
    
    // The `rate` variable has the desired frequency in Hz.
    // The rate register wants a Divisor, and gives a frequency
    // of (ClockLow/(Divisor+2))  So Divisor = ClockLow/(Rate) - 2
    reg = (hm2->rcpwmgen.clock_frequency / (rate)) - 2;
    hm2->rcpwmgen.rate_reg = reg;
    
    int i;   
    // Set width.
    for (i = 0; i < hm2->rcpwmgen.num_instances; i ++) {
	if (*hm2->rcpwmgen.instance[i].hal.pin.scale == 0) {
            if (hm2->rcpwmgen.error_throttle == 0) {
                HM2_ERR("rcpwmgen %d zero scale is illegal, resetting to %.3lf \n", i,1.0);
                hm2->rcpwmgen.error_throttle = 100;
            }
	    *hm2->rcpwmgen.instance[i].hal.pin.scale = 1.0;
        }
        double width = (*hm2->rcpwmgen.instance[i].hal.pin.width)/(*hm2->rcpwmgen.instance[i].hal.pin.scale)+(*hm2->rcpwmgen.instance[i].hal.pin.offset);
        // The `width` variable has the desired pulse width in milliseconds.
        // The width register sets the width to reg*(CLockLow/16+1)

        if (width < 0) {
            *hm2->rcpwmgen.instance[i].hal.pin.width = 0.0;
            width = 0.0;
            if (hm2->rcpwmgen.error_throttle == 0) {
                HM2_ERR("rcpwmgen %d width must be >= 0, resetting to %.3lf \n", i,0.0);
                hm2->rcpwmgen.error_throttle = 100;
            }
	 }
         reg = ((hm2->rcpwmgen.clock_frequency/(16.0*1000.0)))*width -1; 
         if ((reg +1) > 65535 ) {
            *hm2->rcpwmgen.instance[i].hal.pin.width = 65535/(hm2->rcpwmgen.clock_frequency/(16.0*1000.0));
            reg = 65535;
            if (hm2->rcpwmgen.error_throttle == 0) {
                HM2_ERR("rcpwmgen %d width too large,resetting to %.3lf \n", i,*hm2->rcpwmgen.instance[i].hal.pin.width);  
                hm2->rcpwmgen.error_throttle = 100;
            }
        }
        hm2->rcpwmgen.width_reg[i] = reg;
    }
}

//
// Update the RCPWM rate Register if needed
//

void hm2_rcpwmgen_write(hostmot2_t *hm2) {

    if (hm2->rcpwmgen.num_instances == 0) return;

    // check rate 
 
    if ( *hm2->rcpwmgen.hal->pin.rate != hm2->rcpwmgen.written_rate) {
            hm2->rcpwmgen.written_rate = *hm2->rcpwmgen.hal->pin.rate;
            goto force_write;
       }
	

    return;
    force_write:
    hm2_rcpwmgen_force_write(hm2);
}

// force write is just for initialization and parameters after a change

void hm2_rcpwmgen_force_write(hostmot2_t *hm2) {

    hm2_rcpwmgen_update_regs(hm2);
    // Write register values to board.
    hm2->llio->write(hm2->llio, hm2->rcpwmgen.rate_addr, &hm2->rcpwmgen.rate_reg,sizeof(rtapi_u32));

}



void hm2_rcpwmgen_prepare_tram_write(hostmot2_t *hm2) {

    hm2_rcpwmgen_update_regs(hm2);	

}


void hm2_rcpwmgen_print_module(hostmot2_t *hm2) {
    int i;
    if (hm2->rcpwmgen.num_instances <= 0) return;
    HM2_PRINT("rcpwmgens: %d\n", hm2->rcpwmgen.num_instances);
    HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n", hm2->rcpwmgen.clock_frequency, hm2_hz_to_mhz(hm2->rcpwmgen.clock_frequency));
    HM2_PRINT("    version: %d\n", hm2->rcpwmgen.version);
    HM2_PRINT("    width_addr: 0x%04X\n", hm2->rcpwmgen.width_addr);
    HM2_PRINT("    rate_addr: 0x%04X\n", hm2->rcpwmgen.rate_addr);
    for (i = 0; i < hm2->rcpwmgen.num_instances; i ++) {
        HM2_PRINT("    instance %d:\n", i);
        HM2_PRINT("        width_reg = 0x%08X\n", hm2->rcpwmgen.width_reg[i]);
        HM2_PRINT("        rate_reg = 0x%08X\n", hm2->rcpwmgen.rate_reg);
    }
}
