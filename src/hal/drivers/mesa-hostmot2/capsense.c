
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

int hm2_capsense_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r;


    if (hm2->capsense.num_instances != 0) {
        HM2_ERR(
            "found duplicate Module Descriptor for %s (inconsistent firmware), not loading driver\n",
            hm2_get_general_function_name(md->gtag)
        );
        return -EINVAL;
    }

    // allocate the module-global HAL shared memory
    hm2->capsense.hal = (hm2_capsense_module_global_t *)hal_malloc(sizeof(hm2_capsense_module_global_t));
    if (hm2->capsense.hal == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    if (hm2->config.num_capsensors > md->instances) {
        HM2_ERR(
            "config.num_capsensors=%d, but only %d are available, not loading driver\n",
            hm2->config.num_capsensors,
            md->instances
        );
        return -EINVAL;
    }

    if (hm2->config.num_capsensors == 0) {
        return 0;
    }

    // 
    // looks good, start initializing
    // 


    if (hm2->config.num_capsensors == -1) {
        hm2->capsense.num_instances = md->instances;
    } else {
        hm2->capsense.num_instances = hm2->config.num_capsensors;
    }


    hm2->capsense.instance = (hm2_capsense_instance_t *)hal_malloc(hm2->capsense.num_instances * sizeof(hm2_capsense_instance_t));
    if (hm2->capsense.instance == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    hm2->capsense.capsense_data_addr = md->base_address + (0 * md->register_stride);
    hm2->capsense.capsense_hysteresis_addr = md->base_address + (1 * md->register_stride);

//    r = hm2_register_tram_read_region(hm2, hm2->capsense.capsense_data_addr, (hm2->capsense.num_instances * sizeof(u32)), &hm2->capsense.capsense_data_reg);
    r = hm2_register_tram_read_region(hm2, hm2->capsense.capsense_data_addr, (1 * sizeof(u32)), &hm2->capsense.capsense_data_reg);
    if (r < 0) {
        HM2_ERR("error registering tram read region for CAPSENSE Data register (%d)\n", r);
        goto fail0;
    }

//    hm2->capsense.capsense_data_reg = (u32 *)kmalloc(hm2->capsense.num_instances * sizeof(u32), GFP_KERNEL);
    hm2->capsense.capsense_data_reg = (u32 *)kmalloc(1 * sizeof(u32), GFP_KERNEL);
    if (hm2->capsense.capsense_data_reg == NULL) {
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

        // parameters
/*
        // these hal parameters affect all capsense instances
        r = hal_pin_u32_newf(
            HAL_IO,
            &(hm2->capsense.hal->pin.capsense_hysteresis),
            hm2->llio->comp_id,
            "%s.capsense.capsense_hysteresis",
            hm2->llio->name
        );
        if (r < 0) {
            HM2_ERR("error adding capsense.capsense_hysteresis pin, aborting\n");
            goto fail1;
        }
        hm2->capsense.hal->pin.capsense_hysteresis = 0x33333333;
//        hm2->capsense.written_capsense_hysteresis_reg = 0;
*/

        rtapi_snprintf(name, sizeof(name), "%s.capsense.%02d.hysteresis", hm2->llio->name, 0);
        r = hal_pin_u32_new(name, HAL_IN, &(hm2->capsense.hal->pin.capsense_hysteresis), hm2->llio->comp_id);
        if (r < 0) {
            HM2_ERR("error adding capsense.hysteresis pin, aborting\n");
            goto fail1;
        }

        *hm2->capsense.hal->pin.capsense_hysteresis = 0x33333333;
 
       for (i = 0; i < hm2->capsense.num_instances; i ++) {
            // pins
            rtapi_snprintf(name, sizeof(name), "%s.capsense.%02d.trigged", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_OUT, &(hm2->capsense.instance[i].hal.pin.sensepad), hm2->llio->comp_id);
            if (r < 0) {
                HM2_ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }
            // init hal objects
/*
            for(i=0;i<hm2->capsense.num_instances;i++){
                *(hm2->capsense.instance[i].hal.pin.sensepad) = 0;
            }
*/
        }
    }


    return hm2->capsense.num_instances;

fail1:
    kfree(hm2->capsense.capsense_data_reg);

fail0:
    hm2->capsense.num_instances = 0;
    return r;
}


/*
int hm2_capsense_setup(hostmot2_t *hm2) {
    int r,i;
    char name[HAL_NAME_LEN + 1];


    if (hm2->config.enable_capsense == 0) {
        return 0;
    }
    
    hm2->capsense = (capsense_t *)hal_malloc(sizeof(capsense_t));
    if (hm2->capsense == NULL) {
        HM2_ERR("out of memory!\n");
        hm2->config.enable_capsense = 0;
        return -ENOMEM;
    }

    for(i=0;i<NUM_capsense_SENSORS;i=i+1){
        rtapi_snprintf(name, sizeof(name), "%s.capsense.sensepad.%d.out", hm2->llio->name, i);
        r = hal_pin_bit_new(name, HAL_OUT, &(hm2->capsense->hal.pin.sensepad[i]), hm2->llio->comp_id);
        if (r < 0) {
            HM2_ERR("error adding pin '%s', aborting\n", name);
            return -EINVAL;
        }
    }

*/
    // init hal objects
/*
    for(i=0;i<NUM_capsense_SENSORS;i++){
        *(hm2->capsense->hal.pin.sensepad[i]) = 0;
    }

	 hm2_set_pin_direction(hm2, 36, HM2_PIN_DIR_IS_OUTPUT);	
    
    return 0;
}
*/

void hm2_capsense_read(hostmot2_t *hm2) {
    int i;
    u32 val;
    hal_bit_t bit;

     if (hm2->capsense.num_instances == 0) return;
	 
    hm2->llio->read(
        hm2->llio,
        hm2->capsense.capsense_data_addr,
        &val,
        sizeof(u32)
    );

    for(i=0;i<hm2->capsense.num_instances; i ++){
        bit = (val >> i) & 0x1;
        *hm2->capsense.instance[i].hal.pin.sensepad = bit;
    }
}

//void hm2_capsense_allocate_pins(hostmot2_t *hm2) {
//    hm2_set_pin_direction(hm2, 36, HM2_PIN_DIR_IS_OUTPUT);
//}
