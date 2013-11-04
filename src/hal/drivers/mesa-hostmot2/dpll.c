
//
//    Copyright (C) 2013 Andy Pugh

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

// A driver for the Hostmot2 HM2_DPLL module that allows pre-triggering of some
// other modules. 


#include "config_module.h"
#include RTAPI_INC_SLAB_H

#include "rtapi.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "hal.h"

#include "hal/drivers/mesa-hostmot2/hostmot2.h"

int hm2_dpll_parse_md(hostmot2_t *hm2, int md_index) {

    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r;

    //
    // some standard sanity checks
    //

    if (!hm2_md_is_consistent_or_complain(hm2, md_index, 0, 7, 4, 0x0000)) {
        HM2_ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }


    if (hm2->config.num_dplls == 0) return 0;

    if (hm2->config.num_dplls > md->instances) {
        hm2->dpll.num_instances = md->instances;
        HM2_ERR( "There are only %d dplls on this board type, using %d\n",
                md->instances, md->instances );
    } else if (hm2->config.num_dplls == -1) {
        hm2->dpll.num_instances = md->instances;
    } else hm2->dpll.num_instances = hm2->config.num_dplls;

    //
    // looks good, start initializing
    //

    hm2->dpll.clock_frequency = md->clock_freq;
    hm2->dpll.base_rate_addr = md->base_address + 0 * md->register_stride;
    hm2->dpll.phase_err_addr = md->base_address + 1 * md->register_stride;
    hm2->dpll.control_reg0_addr = md->base_address + 2 * md->register_stride;
    hm2->dpll.control_reg1_addr = md->base_address + 3 * md->register_stride;
    hm2->dpll.timer_12_addr = md->base_address + 4 * md->register_stride;
    hm2->dpll.timer_34_addr = md->base_address + 5 * md->register_stride;
    hm2->dpll.hm2_dpll_sync_addr = md->base_address + 6 * md->register_stride;
    
    // export to HAL
    hm2->dpll.pins = hal_malloc(sizeof(hm2_dpll_pins_t));

    r = hal_pin_float_newf(HAL_IN, &(hm2->dpll.pins->time1_us),
            hm2->llio->comp_id, "%s.dpll.01.timer-us", hm2->llio->name);
    r += hal_pin_float_newf(HAL_IN, &(hm2->dpll.pins->time2_us),
            hm2->llio->comp_id, "%s.dpll.02.timer-us", hm2->llio->name);
    r += hal_pin_float_newf(HAL_IN, &(hm2->dpll.pins->time3_us),
            hm2->llio->comp_id, "%s.dpll.03.timer-us", hm2->llio->name);
    r += hal_pin_float_newf(HAL_IN, &(hm2->dpll.pins->time4_us),
            hm2->llio->comp_id, "%s.dpll.04.timer-us", hm2->llio->name);
    r += hal_pin_float_newf(HAL_IN, &(hm2->dpll.pins->base_freq),
            hm2->llio->comp_id, "%s.dpll.base-freq-khz", hm2->llio->name);
    r += hal_pin_float_newf(HAL_OUT, &(hm2->dpll.pins->phase_error),
            hm2->llio->comp_id, "%s.dpll.phase-error-us", hm2->llio->name);
    r += hal_pin_u32_newf(HAL_IN, &(hm2->dpll.pins->time_const),
            hm2->llio->comp_id, "%s.dpll.time-const", hm2->llio->name);
    r += hal_pin_u32_newf(HAL_IN, &(hm2->dpll.pins->plimit),
            hm2->llio->comp_id, "%s.dpll.plimit", hm2->llio->name);
    r += hal_pin_u32_newf(HAL_OUT, &(hm2->dpll.pins->ddssize),
            hm2->llio->comp_id, "%s.dpll.ddsize", hm2->llio->name);
    r += hal_pin_u32_newf(HAL_OUT, &(hm2->dpll.pins->prescale),
            hm2->llio->comp_id, "%s.dpll.prescale", hm2->llio->name);
    if (r < 0) {
        HM2_ERR("error adding hm2_dpll timer pins, Aborting\n");
        goto fail0;
    }
    *hm2->dpll.pins->time1_us = 100.0;
    *hm2->dpll.pins->time2_us = 100.0;
    *hm2->dpll.pins->time3_us = 100.0;
    *hm2->dpll.pins->time4_us = 100.0;
    *hm2->dpll.pins->prescale = 1;
    *hm2->dpll.pins->base_freq = -1; // An indication it needs init
    *hm2->dpll.pins->time_const = 0xA000;
    *hm2->dpll.pins->plimit = 0x400000;

    r = hm2_register_tram_read_region(hm2, hm2->dpll.hm2_dpll_sync_addr,
            sizeof(u32), &hm2->dpll.hm2_dpll_sync_reg);
    if (r < 0) {
        HM2_ERR("Error registering tram synch write. Aborting\n");
        goto fail0;
    }
    r = hm2_register_tram_read_region(hm2, hm2->dpll.control_reg1_addr,
            sizeof(u32), &hm2->dpll.control_reg1_read);
    if (r < 0) {
        HM2_ERR("Error registering dpll control reg 1. Aborting\n");
        goto fail0;
    }

    return hm2->dpll.num_instances;

    fail0:
    return r;

}

void hm2_dpll_process_tram_read(hostmot2_t *hm2, long period){
    hm2_dpll_pins_t *pins;
    
    if (hm2->dpll.num_instances == 0) return;
    
     pins = hm2->dpll.pins;
    
    *pins->phase_error = (s32)*hm2->dpll.hm2_dpll_sync_reg 
            * (period / 4294967296000.00) ;
    *pins->ddssize = *hm2->dpll.control_reg1_read & 0xFF;
}

void hm2_dpll_write(hostmot2_t *hm2, long period) {
    hm2_dpll_pins_t *pins;
    double period_ms = period / 1000;
    u32 buff;
    static int init_counter = 0;
    
    if (hm2->dpll.num_instances == 0) return;
    
    if (init_counter < 100){
        init_counter++;
        buff = 0; // Force phase error to zero at startup
        hm2->llio->write(hm2->llio,
                hm2->dpll.phase_err_addr,
                &buff,
                sizeof(u32));
        hm2->dpll.control_reg0_written= buff;
    }
    
    pins = hm2->dpll.pins;
    
    if (*pins->base_freq < 0 ) {
        *pins->base_freq = 1000.0/period_ms;
    }

    *pins->prescale = (0x40000000LL * hm2->dpll.clock_frequency)
                    / ((1LL << *pins->ddssize) * *pins->base_freq * 1000.0);

    if (*pins->prescale < 1) *pins->prescale = 1;
    
    buff = (u32)((*pins->base_freq * 1000.0 
            * (1LL << *pins->ddssize) 
            * *pins->prescale)
            / hm2->dpll.clock_frequency);

    if (buff != hm2->dpll.base_rate_written){
        hm2->llio->write(hm2->llio,
                hm2->dpll.base_rate_addr,
                &buff,
                sizeof(u32));
        hm2->dpll.base_rate_written= buff;
    }
    buff = (u32)(*pins->prescale << 24 
                | *pins->plimit);
    if (buff != hm2->dpll.control_reg0_written){
        hm2->llio->write(hm2->llio,
                hm2->dpll.control_reg0_addr,
                &buff,
                sizeof(u32));
        hm2->dpll.control_reg0_written= buff;
    }
    buff = (u32)(*pins->time_const << 16);
    if (buff != hm2->dpll.control_reg1_written){
        hm2->llio->write(hm2->llio,
                hm2->dpll.control_reg1_addr,
                &buff,
                sizeof(u32));
        hm2->dpll.control_reg1_written= buff;
    }
    buff = (u32)((-*hm2->dpll.pins->time2_us / period_ms) * 0x10000) << 16
         | (u32)((-*hm2->dpll.pins->time1_us / period_ms) * 0x10000);
    if (buff != hm2->dpll.timer_12_written){
        hm2->llio->write(hm2->llio,
                hm2->dpll.timer_12_addr,
                &buff,
                sizeof(u32));
        hm2->dpll.timer_12_written = buff;
    }
    buff = (u32)((-*hm2->dpll.pins->time4_us / period_ms) * 0x10000) << 16
         | (u32)((-*hm2->dpll.pins->time3_us / period_ms) * 0x10000);
    if (buff != hm2->dpll.timer_34_written){
        hm2->llio->write(hm2->llio,
                hm2->dpll.timer_34_addr,
                &buff,
                sizeof(u32));
        hm2->dpll.timer_34_written = buff;
    }
}

int hm2_dpll_force_write(hostmot2_t *hm2) {
    int i;
    if (hm2->dpll.num_instances == 0) return 0;
    for (i = 0; i < hm2->absenc.num_chans; i ++){
        hm2_sserial_remote_t *chan = &hm2->absenc.chans[i];
        switch (chan->myinst){
        case HM2_GTAG_SSI:
            chan->params->timer_num = 1;
            break;
        case HM2_GTAG_BISS:
            if (hm2->dpll.num_instances > 1){
                chan->params->timer_num = 2;
            } else {
                chan->params->timer_num = 1;
            }
            break;
        case HM2_GTAG_FABS:
            if (hm2->dpll.num_instances > 2){
                chan->params->timer_num = 3;
            } else {
                chan->params->timer_num = 1;
            }
            break;
        }
   /* Other triggerable component types should be added here */
    }
    return 0;
}


void hm2_dpll_cleanup(hostmot2_t *hm2) {
    // Should all be handled by the HAL housekeeping
}
