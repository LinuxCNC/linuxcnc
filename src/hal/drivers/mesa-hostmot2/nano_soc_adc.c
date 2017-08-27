
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



int hm2_adc_setup(hostmot2_t *hm2) {
    int r,i;
    char name[HAL_NAME_LEN + 1];


    if (hm2->config.enable_adc == 0) {
        return 0;
    }
    
    hm2->nano_soc_adc = (de0_nano_soc_adc_t *)hal_malloc(sizeof(de0_nano_soc_adc_t));
    if (hm2->nano_soc_adc == NULL) {
        HM2_ERR("out of memory!\n");
        hm2->config.enable_adc = 0;
        return -ENOMEM;
    }

    for(i=0;i<NUM_ADC_SAMPLES;i=i+1){
        rtapi_snprintf(name, sizeof(name), "%s.nano_soc_adc.ch.%d.out", hm2->llio->name, i);
        r = hal_pin_u32_new(name, HAL_OUT, &(hm2->nano_soc_adc->hal.pin.sample[i]), hm2->llio->comp_id);
        if (r < 0) {
            HM2_ERR("error adding pin '%s', aborting\n", name);
            return -EINVAL;
        }
    }


    // init hal objects

    for(i=0;i<NUM_ADC_SAMPLES;i++){
        *(hm2->nano_soc_adc->hal.pin.sample[i]) = 0;
    }

    return 0;
}


void de0_nano_soc_adc_read(hostmot2_t *hm2) {
    int i;
    u32 val;
    u32 num_samples=NUM_ADC_SAMPLES;
    u32 reset_reg = 0x0100;
    u32 start_reg = 0x0101;

	 if (hm2->config.enable_adc == 0) return;
	 
    hm2->llio->read(
        hm2->llio,
        DE0_NANO_SOC_ADC_BASE,
        &val,
        sizeof(u32)
    );
    if (val & 1){ 
    /* insert dummy read of first sample */
//        hm2->llio->read(
//            hm2->llio,
//            DE0_NANO_SOC_ADC_DATA,
//            (void *)hm2->nano_soc_adc->hal.pin.sample[0],
//            sizeof(u32)
//        );
	 
	    for(i=0;i<NUM_ADC_SAMPLES;i=i+1){
                hm2->llio->read(
                hm2->llio,
                DE0_NANO_SOC_ADC_DATA,
                (void *)hm2->nano_soc_adc->hal.pin.sample[i],
                sizeof(u32)
            );
        }

        hm2->llio->write(
            hm2->llio,
            DE0_NANO_SOC_ADC_DATA,
            &num_samples,
            sizeof(u32)
        );

        hm2->llio->write(
            hm2->llio,
            DE0_NANO_SOC_ADC_BASE,
            &reset_reg,
            sizeof(u32)
        );

        hm2->llio->write(
            hm2->llio,
            DE0_NANO_SOC_ADC_BASE,
            &start_reg,
            sizeof(u32)
        );
    }
}
