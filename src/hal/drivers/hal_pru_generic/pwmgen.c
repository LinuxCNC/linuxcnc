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

int export_pwmgen(hal_pru_generic_t *hpg, int i)
{
    char name[HAL_NAME_LEN + 1];
    int r;

    // Export HAL Pins
    rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.enable", hpg->config.name, i);
    r = hal_pin_bit_new(name, HAL_IN, &(hpg->pwmgen.instance[i].hal_enable), hpg->config.comp_id);
    if (r != 0) { return r; }

    rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.period", hpg->config.name, i);
    r = hal_pin_u32_new(name, HAL_IN, &(hpg->pwmgen.instance[i].hal_period), hpg->config.comp_id);
    if (r != 0) { return r; }

    rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.out1", hpg->config.name, i);
    r = hal_pin_u32_new(name, HAL_IN, &(hpg->pwmgen.instance[i].hal_out1), hpg->config.comp_id);
    if (r != 0) { return r; }

    rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.out2", hpg->config.name, i);
    r = hal_pin_u32_new(name, HAL_IN, &(hpg->pwmgen.instance[i].hal_out2), hpg->config.comp_id);
    if (r != 0) { return r; }

    // Export HAL Parameters
    rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.pin1", hpg->config.name, i);
    r = hal_param_u32_new(name, HAL_RW, &(hpg->pwmgen.instance[i].hal_pin1), hpg->config.comp_id);
    if (r != 0) { return r; }

    rtapi_snprintf(name, sizeof(name), "%s.pwm.%02d.pin2", hpg->config.name, i);
    r = hal_param_u32_new(name, HAL_RW, &(hpg->pwmgen.instance[i].hal_pin2), hpg->config.comp_id);
    if (r != 0) { return r; }

    // Initialize HAL Pins
    *(hpg->pwmgen.instance[i].hal_enable)   = 0;
    *(hpg->pwmgen.instance[i].hal_period)   = 0;
    *(hpg->pwmgen.instance[i].hal_out1)     = 0;
    *(hpg->pwmgen.instance[i].hal_out2)     = 0;

    // Initialize HAL Parameters
    hpg->pwmgen.instance[i].hal_pin1        = PRU_DEFAULT_PIN;
    hpg->pwmgen.instance[i].hal_pin2        = PRU_DEFAULT_PIN;

    return 0;
}

int hpg_pwmgen_init(hal_pru_generic_t *hpg){
    int r,i;

    if (hpg->config.num_pwmgens <= 0)
        return 0;

//  hpg->pwmgen.num_instances = hpg->config.num_pwmgens;
    hpg->pwmgen.num_instances = 1;

    // Allocate HAL shared memory for state data
    hpg->pwmgen.instance = (hpg_pwmgen_instance_t *) hal_malloc(sizeof(hpg_pwmgen_instance_t) * hpg->pwmgen.num_instances);
    if (hpg->pwmgen.instance == 0) {
	    HPG_ERR("ERROR: hal_malloc() failed\n");
	    return -1;
    }

    for (i=0; i < hpg->pwmgen.num_instances; i++) {
        int len = sizeof(hpg->pwmgen.instance[i].pru) + (sizeof(PRU_pwm_output_t) * hpg->config.num_pwmgens);
//      int len = sizeof(hpg->pwmgen.instance[i].pru);
        hpg->pwmgen.instance[i].task_addr = pru_malloc(hpg, len);
        hpg->pwmgen.instance[i].pru.task.hdr.mode = eMODE_PWM;

        pru_task_add(hpg, hpg->pwmgen.instance[i].task_addr);

PRU_task_pwm_t *pru = (PRU_task_pwm_t *) ((u32) hpg->pru_data + (u32) hpg->pwmgen.instance[i].task_addr);

hpg->pwmgen.instance[i].pru = *pru;

hpg->pwmgen.instance[i].pru.task.hdr.mode = eMODE_PWM;
hpg->pwmgen.instance[i].pru.task.hdr.len = 3;
hpg->pwmgen.instance[i].pru.task.hdr.dataX = 0x03;
hpg->pwmgen.instance[i].pru.task.hdr.dataY = 0x04;
hpg->pwmgen.instance[i].pru.period = 1000;
hpg->pwmgen.instance[i].pru.prescale = 1;
hpg->pwmgen.instance[i].pru.reserved = 0;

*pru = hpg->pwmgen.instance[i].pru;

PRU_pwm_output_t *out = (PRU_pwm_output_t *) ((u32) hpg->pru_data + (u32) hpg->pwmgen.instance[i].task_addr + sizeof(hpg->pwmgen.instance[i].pru));

out[0].value = 250;
out[0].pin   = 1;
out[1].value = 500;
out[1].pin   = 2;
out[2].value = 750;
out[2].pin   = 3;

// pru->task.hdr.mode  = eMODE_PWM;
// pru->task.hdr.dataX = 0x02;
// pru->task.hdr.dataY = 0x03;
// pru->Period = 1000;
// pru->High1 = 250;
// pru->High2 = 750;

        if ((r = export_pwmgen(hpg,i)) != 0){ 
            HPG_ERR("ERROR: failed to export pwmgen %i: %i\n",i,r);
            return -1;
        }
    }

    return 0;
fail0:
    hpg->pwmgen.num_instances = 0;
    return r;
}

