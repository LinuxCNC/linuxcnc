//----------------------------------------------------------------------//
// Description: pwmgen.c                                                //
// Code to interface to a PRU driven step generator                     //
//                                                                      //
// Author(s): Charles Steinkuehler                                      //
// License: GNU GPL Version 2.0 or (at your option) any later version.  //
//                                                                      //
// Last change:                                                         //
// 2012-Dec-30 Charles Steinkuehler                                     //
//             Initial version, based in part on:                       //
//               hal_pru.c      Micheal Halberler                       //
//               supply.c       Matt Shaver                             //
//               stepgen.c      John Kasunich                           //
//               hostmot2 code  Sebastian Kuzminsky                     //
//----------------------------------------------------------------------//
// This file is part of LinuxCNC HAL                                    //
//                                                                      //
// Copyright (C) 2012  Charles Steinkuehler                             //
//                     <charles AT steinkuehler DOT net>                //
//                                                                      //
// This program is free software; you can redistribute it and/or        //
// modify it under the terms of the GNU General Public License          //
// as published by the Free Software Foundation; either version 2       //
// of the License, or (at your option) any later version.               //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program; if not, write to the Free Software          //
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA        //
// 02110-1301, USA.                                                     //
//                                                                      //
// THE AUTHORS OF THIS PROGRAM ACCEPT ABSOLUTELY NO LIABILITY FOR       //
// ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE   //
// TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of      //
// harming persons must have provisions for completely removing power   //
// from all motors, etc, before persons enter any danger area.  All     //
// machinery must be designed to comply with local and national safety  //
// codes, and the authors of this software can not, and do not, take    //
// any responsibility for such compliance.                              //
//                                                                      //
// This code was written as part of the LinuxCNC project.  For more     //
// information, go to www.linuxcnc.org.                                 //
//----------------------------------------------------------------------//

// Use config_module.h instead of config.h so we can use RTAPI_INC_LIST_H
#include "config_module.h"

// this probably should be an ARM335x #define
#if !defined(TARGET_PLATFORM_BEAGLEBONE)
#error "This driver is for the beaglebone platform only"
#endif

#if !defined(BUILD_SYS_USER_DSO)
#error "This driver is for usermode threads only"
#endif

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "hal.h"

#include "hal/drivers/hal_pru_generic/hal_pru_generic.h"

void hpg_pwmgen_handle_pwm_frequency(hal_pru_generic_t *hpg, int i) {
    u32 pwm_pru_periods;
    pwm_pru_periods = 1.0e9 / (double) hpg->pwmgen.instance[i].hal.param.pwm_frequency / hpg->config.pru_period;
    if (pwm_pru_periods < 65535) {
        hpg->pwmgen.instance[i].pru.prescale = 1;
        hpg->pwmgen.instance[i].pru.period = pwm_pru_periods - 1;
    } else {
        // prescale required
        hpg->pwmgen.instance[i].pru.prescale = ceil((double) pwm_pru_periods / 65535.0);
        hpg->pwmgen.instance[i].pru.period = (pwm_pru_periods / hpg->pwmgen.instance[i].pru.prescale) - 1;
    }
}

int export_pwmgen(hal_pru_generic_t *hpg, int i)
{
    char name[HAL_NAME_LEN + 1];
    int r, j;

    // HAL values common to all outputs in this instance
    rtapi_snprintf(name, sizeof(name), "%s.pwmgen.%02d.pwm_frequency", hpg->config.name, i);
    r = hal_param_u32_new(name, HAL_RW, &(hpg->pwmgen.instance[i].hal.param.pwm_frequency), hpg->config.comp_id);
    if (r != 0) { return r; }

    hpg->pwmgen.instance[i].hal.param.pwm_frequency = 100;

    for (j=0; j < hpg->pwmgen.instance[i].num_outputs; j++) {
        // Export HAL Pins
        rtapi_snprintf(name, sizeof(name), "%s.pwmgen.%02d.out.%02d.enable", hpg->config.name, i, j);
        r = hal_pin_bit_new(name, HAL_IN, &(hpg->pwmgen.instance[i].out[j].hal.pin.enable), hpg->config.comp_id);
        if (r != 0) { return r; }

        rtapi_snprintf(name, sizeof(name), "%s.pwmgen.%02d.out.%02d.value", hpg->config.name, i, j);
        r = hal_pin_float_new(name, HAL_IN, &(hpg->pwmgen.instance[i].out[j].hal.pin.value), hpg->config.comp_id);
        if (r != 0) { return r; }

        // Export HAL Parameters
        rtapi_snprintf(name, sizeof(name), "%s.pwmgen.%02d.out.%02d.scale", hpg->config.name, i, j);
        r = hal_param_float_new(name, HAL_RW, &(hpg->pwmgen.instance[i].out[j].hal.param.scale), hpg->config.comp_id);
        if (r != 0) { return r; }

        rtapi_snprintf(name, sizeof(name), "%s.pwmgen.%02d.out.%02d.pin", hpg->config.name, i, j);
        r = hal_param_u32_new(name, HAL_RW, &(hpg->pwmgen.instance[i].out[j].hal.param.pin), hpg->config.comp_id);
        if (r != 0) { return r; }

        // Initialize HAL Pins
        *(hpg->pwmgen.instance[i].out[j].hal.pin.enable) = 0;
        *(hpg->pwmgen.instance[i].out[j].hal.pin.value)  = 0.0;

        // Initialize HAL Parameters
        hpg->pwmgen.instance[i].out[j].hal.param.pin   = PRU_DEFAULT_PIN;
        hpg->pwmgen.instance[i].out[j].hal.param.scale = 1.0;

//Debugging...enable by default
*(hpg->pwmgen.instance[i].out[j].hal.pin.enable) = 1;
*(hpg->pwmgen.instance[i].out[j].hal.pin.value)  = 0.2 * ((float) j + 1);
hpg->pwmgen.instance[i].out[j].hal.param.pin   = (j + 1) * 0x21 ;
    }

    return 0;
}

int hpg_pwmgen_init(hal_pru_generic_t *hpg){
    int r,i;

    if (hpg->config.num_pwmgens <= 0)
        return 0;

//  hpg->pwmgen.num_instances = hpg->config.num_pwmgens;
    hpg->pwmgen.num_instances = 1;

    // Allocate HAL shared memory for instance state data
    hpg->pwmgen.instance = (hpg_pwmgen_instance_t *) hal_malloc(sizeof(hpg_pwmgen_instance_t) * hpg->pwmgen.num_instances);
    if (hpg->pwmgen.instance == 0) {
	    HPG_ERR("ERROR: hal_malloc() failed\n");
	    return -1;
    }

    // Clear memory
    memset(hpg->pwmgen.instance, 0, (sizeof(hpg_pwmgen_instance_t) * hpg->pwmgen.num_instances) );

    for (i=0; i < hpg->pwmgen.num_instances; i++) {

hpg->pwmgen.instance[i].num_outputs = hpg->config.num_pwmgens;

        // Allocate HAL shared memory for output state data
        hpg->pwmgen.instance[i].out = (hpg_pwmgen_output_instance_t *) hal_malloc(sizeof(hpg_pwmgen_output_instance_t) * hpg->pwmgen.instance[i].num_outputs);
        if (hpg->pwmgen.instance[i].out == 0) {
	        HPG_ERR("ERROR: hal_malloc() failed\n");
	        return -1;
        }

        int len = sizeof(hpg->pwmgen.instance[i].pru) + (sizeof(PRU_pwm_output_t) * hpg->pwmgen.instance[i].num_outputs);
        hpg->pwmgen.instance[i].task.addr = pru_malloc(hpg, len);
        hpg->pwmgen.instance[i].pru.task.hdr.mode = eMODE_PWM;

        pru_task_add(hpg, &(hpg->pwmgen.instance[i].task));

        if ((r = export_pwmgen(hpg,i)) != 0){ 
            HPG_ERR("ERROR: failed to export pwmgen %i: %i\n",i,r);
            return -1;
        }

    }

    return 0;
}

void hpg_pwmgen_update(hal_pru_generic_t *hpg) {
    int i, j;

    if (hpg->pwmgen.num_instances <= 0) return;

    for (i = 0; i < hpg->pwmgen.num_instances; i ++) {
        double scaled_value;
        double abs_duty_cycle;

        PRU_pwm_output_t *out = (PRU_pwm_output_t *) ((u32) hpg->pru_data + (u32) hpg->pwmgen.instance[i].task.addr + sizeof(hpg->pwmgen.instance[i].pru));

        for (j = 0; j < hpg->pwmgen.instance[i].num_outputs ; j ++) {

            if (*hpg->pwmgen.instance[i].out[j].hal.pin.enable == 0) {
                hpg->pwmgen.instance[i].out[j].pru.value = 0;
                continue;
            }

            scaled_value = *hpg->pwmgen.instance[i].out[j].hal.pin.value / hpg->pwmgen.instance[i].out[j].hal.param.scale;

            abs_duty_cycle = fabs(scaled_value);
            if (abs_duty_cycle > 1.0) abs_duty_cycle = 1.0;

            // duty_cycle goes from 0.0 to 1.0, and needs to be cover the range of 0 to pwm_period, inclusive
            hpg->pwmgen.instance[i].out[j].pru.value = abs_duty_cycle * (double)(hpg->pwmgen.instance[i].pru.period + 1);

            hpg->pwmgen.instance[i].out[j].pru.pin = hpg->pwmgen.instance[i].out[j].hal.param.pin;

            out[j] = hpg->pwmgen.instance[i].out[j].pru;
        }

    }
}

void hpg_pwmgen_write(hal_pru_generic_t *hpg) {
    int i;

    if (hpg->pwmgen.num_instances <= 0) return;

    for (i = 0; i < hpg->pwmgen.num_instances; i ++) {

        hpg->pwmgen.instance[i].pru.task.hdr.mode = eMODE_PWM;
        hpg->pwmgen.instance[i].pru.task.hdr.len = hpg->pwmgen.instance[i].num_outputs;
        hpg->pwmgen.instance[i].pru.task.hdr.dataX = 0x00;
        hpg->pwmgen.instance[i].pru.task.hdr.dataY = 0x00;
        hpg->pwmgen.instance[i].pru.task.hdr.addr = hpg->pwmgen.instance[i].task.next;
        hpg->pwmgen.instance[i].pru.period = 1000;
        hpg->pwmgen.instance[i].pru.prescale = 1;
        hpg->pwmgen.instance[i].pru.reserved = 0;

        PRU_task_pwm_t *pru = (PRU_task_pwm_t *) ((u32) hpg->pru_data + (u32) hpg->pwmgen.instance[i].task.addr);
        *pru = hpg->pwmgen.instance[i].pru;
    }

    hpg_pwmgen_update(hpg);
}