//----------------------------------------------------------------------//
// Description: encoder.c                                               //
// Code to interface to a PRU software encoder input                    //
//                                                                      //
// Author(s): Charles Steinkuehler                                      //
// License: GNU GPL Version 2.0 or (at your option) any later version.  //
//                                                                      //
// Major Changes:                                                       //
// 2014-Feb    Charles Steinkuehler                                     //
//             Initial version, based in part on:                       //
//               encoder.c      John Kasunich                           //
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

// LUT used to decide when/how to modify count value
// LUT index value consists of 6-bits:
// Mode1 Mode0 B_new A_new B_old A_old
//
// Mode selects counting mode:
//   Mode 0 = Quadrature
//   Mode 1 = Up/Down Counter (counts rising edges on A, up when B=1, down when B=0) (hostmot2 count mode)
//   Mode 2 = Up Counter (counts rising edges on A, always counts up, B ignored)     (HAL encoder count mode)
//   MOde 3 = Quadrature x1 count mode (HAL encoder x1 mode)
//
// PRU only does unsigned math, so newcout = count + LUT_Value - 1
//      LUT = 0 : Count--
//      LUT = 1 : No change
//      LUT = 2 : Count++
//      LUT = others : INVALID

const PRU_encoder_LUT_t Counter_LUT = { {
//                  New New Old Old
// Quadrature  B A | B   A   B   A 
// x4 Mode     ====================
    1,      // 0 0 | 0   0   0   0 
    0,      // 0 - | 0   0   0   1 
    2,      // - 0 | 0   0   1   0 
    1,      // - - | 0   0   1   1     
    2,      // 0 + | 0   1   0   0 
    1,      // 0 1 | 0   1   0   1 
    1,      // - + | 0   1   1   0 
    0,      // - 1 | 0   1   1   1 
    0,      // + 0 | 1   0   0   0 
    1,      // + - | 1   0   0   1 
    1,      // 1 0 | 1   0   1   0 
    2,      // 1 - | 1   0   1   1 
    1,      // + + | 1   1   0   0 
    2,      // + 1 | 1   1   0   1 
    0,      // 1 + | 1   1   1   0 
    1,      // 1 1 | 1   1   1   1 

//                  New New Old Old
// Up/Down     B A | B   A   B   A 
// Mode        ====================
    1,      // 0 0 | 0   0   0   0 
    1,      // 0 - | 0   0   0   1 
    1,      // - 0 | 0   0   1   0 
    1,      // - - | 0   0   1   1     
    0,      // 0 + | 0   1   0   0 
    1,      // 0 1 | 0   1   0   1 
    2,      // - + | 0   1   1   0 
    1,      // - 1 | 0   1   1   1 
    1,      // + 0 | 1   0   0   0 
    1,      // + - | 1   0   0   1 
    1,      // 1 0 | 1   0   1   0 
    1,      // 1 - | 1   0   1   1 
    0,      // + + | 1   1   0   0 
    1,      // + 1 | 1   1   0   1 
    2,      // 1 + | 1   1   1   0 
    1,      // 1 1 | 1   1   1   1 

//                  New New Old Old
// Counter     B A | B   A   B   A 
// Mode        ====================
    1,      // 0 0 | 0   0   0   0 
    1,      // 0 - | 0   0   0   1 
    1,      // - 0 | 0   0   1   0 
    1,      // - - | 0   0   1   1     
    2,      // 0 + | 0   1   0   0 
    1,      // 0 1 | 0   1   0   1 
    2,      // - + | 0   1   1   0 
    1,      // - 1 | 0   1   1   1 
    1,      // + 0 | 1   0   0   0 
    1,      // + - | 1   0   0   1 
    1,      // 1 0 | 1   0   1   0 
    1,      // 1 - | 1   0   1   1 
    2,      // + + | 1   1   0   0 
    1,      // + 1 | 1   1   0   1 
    2,      // 1 + | 1   1   1   0 
    1,      // 1 1 | 1   1   1   1 

//                  New New Old Old
// Quadrature  B A | B   A   B   A 
// x1 Mode     ====================
    1,      // 0 0 | 0   0   0   0 
    1,      // 0 - | 0   0   0   1 
    1,      // - 0 | 0   0   1   0 
    1,      // - - | 0   0   1   1     
    2,      // 0 + | 0   1   0   0 
    1,      // 0 1 | 0   1   0   1 
    1,      // - + | 0   1   1   0 
    1,      // - 1 | 0   1   1   1 
    1,      // + 0 | 1   0   0   0 
    1,      // + - | 1   0   0   1 
    1,      // 1 0 | 1   0   1   0 
    1,      // 1 - | 1   0   1   1 
    1,      // + + | 1   1   0   0 
    1,      // + 1 | 1   1   0   1 
    0,      // 1 + | 1   1   1   0 
    1       // 1 1 | 1   1   1   1 
} };

void hpg_encoder_read_chan(hal_pru_generic_t *hpg, int instance, int channel) {
    u16 reg_count;
    s32 reg_count_diff;

    hpg_encoder_instance_t *inst;
    hpg_encoder_channel_instance_t *e;

    inst = &hpg->encoder.instance[instance];
    e    = &hpg->encoder.instance[instance].chan[channel];

    // sanity check
    if (*(e->hal.pin.scale) == 0.0) {
        HPG_ERR("encoder.%02d.scale == 0.0, bogus, setting to 1.0\n", instance);
        *(e->hal.pin.scale) = 1.0;
    }

    PRU_encoder_chan_t *pruchan = (PRU_encoder_chan_t *) ((u32) hpg->pru_data + (u32) inst->task.addr + sizeof(inst->pru));
    
    e->pru.raw.dword[1] = pruchan[channel].raw.dword[1];    // Encoder count
    e->pru.raw.dword[2] = pruchan[channel].raw.dword[2];    // Index count and latched count

//  HPG_ERR("rawenc:%08x %08x %08x\n", pruchan[channel].raw.dword[0],pruchan[channel].raw.dword[1],pruchan[channel].raw.dword[2]);

    // 
    // figure out current rawcounts accumulated by the driver
    // 

    reg_count = e->pru.hdr.count;

    reg_count_diff = (s32)reg_count - (s32)e->prev_reg_count;
    if (reg_count_diff > 32768) reg_count_diff -= 65536;
    if (reg_count_diff < -32768) reg_count_diff += 65536;

    *(e->hal.pin.rawcounts) += reg_count_diff;

    *(e->hal.pin.rawlatch)  = e->pru.hdr.Z_capture;

    *(e->hal.pin.count) += 1;

    e->prev_reg_count = reg_count;

}

void hpg_encoder_read(hal_pru_generic_t *hpg) {
    int i,j;
    
    for (i = 0; i < hpg->encoder.num_instances; i ++) {
        for (j = 0; j < hpg->encoder.instance[i].num_channels; j ++) {
            hpg_encoder_read_chan(hpg, i, j);
        }
    }
}

int export_encoder(hal_pru_generic_t *hpg, int i)
{
    int r, j;

    // HAL values common to all channels in this instance
    // ...nothing to do here...

    // HAL values for individual channels
    for (j=0; j < hpg->encoder.instance[i].num_channels; j++) {
        // Export HAL Pins
        r = hal_pin_s32_newf(HAL_OUT, &(hpg->encoder.instance[i].chan[j].hal.pin.rawcounts), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.rawcounts", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'rawcounts', aborting\n", i, j);
            return r;
        }

        r = hal_pin_s32_newf(HAL_OUT, &(hpg->encoder.instance[i].chan[j].hal.pin.rawlatch), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.rawlatch", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'rawlatch', aborting\n", i, j);
            return r;
        }

        r = hal_pin_s32_newf(HAL_OUT, &(hpg->encoder.instance[i].chan[j].hal.pin.count), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.count", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'count', aborting\n", i, j);
            return r;
        }

        r = hal_pin_s32_newf(HAL_OUT, &(hpg->encoder.instance[i].chan[j].hal.pin.count_latch), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.count-latched", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'count-latched', aborting\n", i, j);
            return r;
        }

        r = hal_pin_float_newf(HAL_OUT, &(hpg->encoder.instance[i].chan[j].hal.pin.position), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.position", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'position', aborting\n", i, j);
            return r;
        }

        r = hal_pin_float_newf(HAL_OUT, &(hpg->encoder.instance[i].chan[j].hal.pin.position_latch), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.position-latched", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'position-latching', aborting\n", i, j);
            return r;
        }

        r = hal_pin_float_newf(HAL_OUT, &(hpg->encoder.instance[i].chan[j].hal.pin.velocity), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.velocity", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'velocity', aborting\n", i, j);
            return r;
        }

        r = hal_pin_bit_newf(HAL_IN, &(hpg->encoder.instance[i].chan[j].hal.pin.reset), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.reset", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'reset', aborting\n", i, j);
            return r;
        }

        r = hal_pin_bit_newf(HAL_IO, &(hpg->encoder.instance[i].chan[j].hal.pin.index_enable), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.index-enable", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'index-enable', aborting\n", i, j);
            return r;
        }

        r = hal_pin_bit_newf(HAL_IN, &(hpg->encoder.instance[i].chan[j].hal.pin.latch_enable), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.latch-enable", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'latch-enable', aborting\n", i, j);
            return r;
        }

        r = hal_pin_bit_newf(HAL_IN, &(hpg->encoder.instance[i].chan[j].hal.pin.latch_polarity), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.latch-polarity", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'latch-polarity', aborting\n", i, j);
            return r;
        }

        r = hal_pin_bit_newf(HAL_OUT, &(hpg->encoder.instance[i].chan[j].hal.pin.quadrature_error), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.quadrature-error", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'quadrature-encoder', aborting\n", i, j);
            return r;
        }

        r = hal_pin_float_newf(HAL_IN, &(hpg->encoder.instance[i].chan[j].hal.pin.scale), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.scale", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'scale', aborting\n", i, j);
            return r;
        }

        r = hal_pin_u32_newf(HAL_IN, &(hpg->encoder.instance[i].chan[j].hal.pin.A_pin), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.A-pin", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'A-pin', aborting\n", i, j);
            return r;
        }

        r = hal_pin_bit_newf(HAL_IN, &(hpg->encoder.instance[i].chan[j].hal.pin.A_invert), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.A-invert", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'A-invert', aborting\n", i, j);
            return r;
        }

        r = hal_pin_u32_newf(HAL_IN, &(hpg->encoder.instance[i].chan[j].hal.pin.B_pin), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.B-pin", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'B-pin', aborting\n", i, j);
            return r;
        }

        r = hal_pin_bit_newf(HAL_IN, &(hpg->encoder.instance[i].chan[j].hal.pin.B_invert), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.B-invert", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'B-invert', aborting\n", i, j);
            return r;
        }

        r = hal_pin_u32_newf(HAL_IN, &(hpg->encoder.instance[i].chan[j].hal.pin.index_pin), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.index-pin", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'index-pin', aborting\n", i, j);
            return r;
        }

        r = hal_pin_bit_newf(HAL_IN, &(hpg->encoder.instance[i].chan[j].hal.pin.index_invert), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.index-invert", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'index-invert', aborting\n", i, j);
            return r;
        }

        r = hal_pin_bit_newf(HAL_IN, &(hpg->encoder.instance[i].chan[j].hal.pin.index_mask), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.index-mask", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'index-mask', aborting\n", i, j);
            return r;
        }

        r = hal_pin_bit_newf(HAL_IN, &(hpg->encoder.instance[i].chan[j].hal.pin.index_mask_invert), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.index-mask-invert", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'index-mask-invert', aborting\n", i, j);
            return r;
        }

        r = hal_pin_u32_newf(HAL_IN, &(hpg->encoder.instance[i].chan[j].hal.pin.counter_mode), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.counter-mode", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'counter-mode', aborting\n", i, j);
            return r;
        }

        r = hal_pin_bit_newf(HAL_IN, &(hpg->encoder.instance[i].chan[j].hal.pin.filter), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.filter", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'filter', aborting\n", i, j);
            return r;
        }

        r = hal_pin_float_newf(HAL_IN, &(hpg->encoder.instance[i].chan[j].hal.pin.vel_timeout), hpg->config.comp_id, "%s.encoder.%02d.chan.%02d.vel-timeout", hpg->config.halname, i, j);
        if (r < 0) {
            HPG_ERR("encoder %02d chan %02d: error adding pin 'vel-timeout', aborting\n", i, j);
            return r;
        }

        //
        // init the hal objects that need it
        //

        *(hpg->encoder.instance[i].chan[j].hal.pin.scale) = 1.0;
        *(hpg->encoder.instance[i].chan[j].hal.pin.index_invert) = 0;
        *(hpg->encoder.instance[i].chan[j].hal.pin.index_mask) = 0;
        *(hpg->encoder.instance[i].chan[j].hal.pin.index_mask_invert) = 0;
        *(hpg->encoder.instance[i].chan[j].hal.pin.counter_mode) = 0;
//      *(hpg->encoder.instance[i].chan[j].hal.pin.filter) = 1;
        *(hpg->encoder.instance[i].chan[j].hal.pin.vel_timeout) = 0.5;

        *hpg->encoder.instance[i].chan[j].hal.pin.rawcounts = 0;
        *hpg->encoder.instance[i].chan[j].hal.pin.rawlatch = 0;

        *hpg->encoder.instance[i].chan[j].hal.pin.count = 0;
        *hpg->encoder.instance[i].chan[j].hal.pin.count_latch = 0;
        *hpg->encoder.instance[i].chan[j].hal.pin.position = 0.0;
        *hpg->encoder.instance[i].chan[j].hal.pin.position_latch = 0.0;
        *hpg->encoder.instance[i].chan[j].hal.pin.velocity = 0.0;
        *hpg->encoder.instance[i].chan[j].hal.pin.quadrature_error = 0;

        hpg->encoder.instance[i].chan[j].zero_offset = 0;

        hpg->encoder.instance[i].chan[j].prev_reg_count = 0;

        hpg->encoder.instance[i].chan[j].state = HM2_ENCODER_STOPPED;
    }

    return 0;
}

int hpg_encoder_init(hal_pru_generic_t *hpg){
    int r,i;

    if (hpg->config.num_encoders <= 0)
        return 0;

rtapi_print("hpg_encoder_init\n");

    // FIXME: Support multiple encoder tasks like so:  num_encoders=3,4,2,5
    // hpg->encoder.num_instances = hpg->config.num_encoders;
    hpg->encoder.num_instances = 1;

    // Allocate HAL shared memory for instance state data
    hpg->encoder.instance = (hpg_encoder_instance_t *) hal_malloc(sizeof(hpg_encoder_instance_t) * hpg->encoder.num_instances);
    if (hpg->encoder.instance == 0) {
    HPG_ERR("ERROR: hal_malloc() failed\n");
    return -1;
    }

rtapi_print("malloc: hpg_encoder_instance_t = %p\n",hpg->encoder.instance);

    // Clear memory
    memset(hpg->encoder.instance, 0, (sizeof(hpg_encoder_instance_t) * hpg->encoder.num_instances) );

    for (i=0; i < hpg->encoder.num_instances; i++) {

        // FIXME: Support multiple instances like so:  num_encoders=3,4,2,5
        hpg->encoder.instance[i].num_channels = hpg->config.num_encoders;

        // Allocate HAL shared memory for channel state data
        hpg->encoder.instance[i].chan = (hpg_encoder_channel_instance_t *) hal_malloc(sizeof(hpg_encoder_channel_instance_t) * hpg->encoder.instance[i].num_channels);
        if (hpg->encoder.instance[i].chan == 0) {
            HPG_ERR("ERROR: hal_malloc() failed\n");
            return -1;
        }

rtapi_print("malloc: hpg_encoder_channel_instance_t = %p\n",hpg->encoder.instance[i].chan);

        int len = sizeof(hpg->encoder.instance[i].pru) + (sizeof(PRU_encoder_chan_t) * hpg->encoder.instance[i].num_channels);
        hpg->encoder.instance[i].task.addr = pru_malloc(hpg, len);
        hpg->encoder.instance[i].pru.task.hdr.mode = eMODE_ENCODER;

        hpg->encoder.instance[i].LUT = pru_malloc(hpg, sizeof(Counter_LUT));

        pru_task_add(hpg, &(hpg->encoder.instance[i].task));

        if ((r = export_encoder(hpg,i)) != 0){ 
            HPG_ERR("ERROR: failed to export encoder %i: %i\n",i,r);
            return -1;
        }

    }

    return 0;
}

void hpg_encoder_update(hal_pru_generic_t *hpg) {
    int i, j;

    if (hpg->encoder.num_instances <= 0) return;

    for (i = 0; i < hpg->encoder.num_instances; i ++) {

        // Update pin_invert register, shared between all channels
        u32 pin_invert = 0;
        for (j = 0; j < hpg->encoder.instance[i].num_channels ; j ++) {
            if (*(hpg->encoder.instance[i].chan[j].hal.pin.A_invert))
                pin_invert |= 1 << *(hpg->encoder.instance[i].chan[j].hal.pin.A_pin);

            if (*(hpg->encoder.instance[i].chan[j].hal.pin.B_invert))
                pin_invert |= 1 << *(hpg->encoder.instance[i].chan[j].hal.pin.B_pin);

            if (*(hpg->encoder.instance[i].chan[j].hal.pin.index_invert))
                pin_invert |= 1 << *(hpg->encoder.instance[i].chan[j].hal.pin.index_pin);
        }

        if (hpg->encoder.instance[i].written_pin_invert != pin_invert) {
            PRU_task_encoder_t *pru = (PRU_task_encoder_t *) ((u32) hpg->pru_data + (u32) hpg->encoder.instance[i].task.addr);
            pru->pin_invert = pin_invert;
            hpg->encoder.instance[i].written_pin_invert = pin_invert;
        }

        // Update per-channel state
        for (j = 0; j < hpg->encoder.instance[i].num_channels ; j ++) {

            hpg->encoder.instance[i].chan[j].pru.hdr.A_pin = *(hpg->encoder.instance[i].chan[j].hal.pin.A_pin);
            hpg->encoder.instance[i].chan[j].pru.hdr.B_pin = *(hpg->encoder.instance[i].chan[j].hal.pin.B_pin);
            hpg->encoder.instance[i].chan[j].pru.hdr.Z_pin = *(hpg->encoder.instance[i].chan[j].hal.pin.index_pin);
            hpg->encoder.instance[i].chan[j].pru.hdr.mode  = *(hpg->encoder.instance[i].chan[j].hal.pin.counter_mode);

            if (hpg->encoder.instance[i].chan[j].written_state != hpg->encoder.instance[i].chan[j].pru.raw.dword[0]) {

                PRU_encoder_chan_t *pruchan = (PRU_encoder_chan_t *) ((u32) hpg->pru_data + (u32) hpg->encoder.instance[i].task.addr + sizeof(hpg->encoder.instance[i].pru));

                pruchan[j].raw.dword[0] = hpg->encoder.instance[i].chan[j].pru.raw.dword[0];
                hpg->encoder.instance[i].chan[j].written_state = hpg->encoder.instance[i].chan[j].pru.raw.dword[0];
            }
        }
    }
}

//
// *_force_write sets up any persistent state data required that does not get
// written by the standard *_update() procedure, above
//
void hpg_encoder_force_write(hal_pru_generic_t *hpg) {
    int i, j;

    if (hpg->encoder.num_instances <= 0) return;

    for (i = 0; i < hpg->encoder.num_instances; i ++) {

        PRU_task_encoder_t *pru = (PRU_task_encoder_t *) ((u32) hpg->pru_data + (u32) hpg->encoder.instance[i].task.addr);

        // Global data common to all channels
        hpg->encoder.instance[i].pru.task.hdr.mode  = eMODE_ENCODER;
        hpg->encoder.instance[i].pru.task.hdr.len   = hpg->encoder.instance[i].num_channels;
        hpg->encoder.instance[i].pru.task.hdr.dataX = 0x00;
        hpg->encoder.instance[i].pru.task.hdr.dataY = 0x00;
        hpg->encoder.instance[i].pru.task.hdr.addr  = hpg->encoder.instance[i].task.next;

        hpg->encoder.instance[i].pru.pin_invert = 0;
        hpg->encoder.instance[i].pru.LUT        = hpg->encoder.instance[i].LUT;

        *pru = hpg->encoder.instance[i].pru;

        hpg->encoder.instance[i].written_pin_invert = hpg->encoder.instance[i].pru.pin_invert;

        // Per-channel data
        PRU_encoder_chan_t *pruchan = (PRU_encoder_chan_t *) ((u32) hpg->pru_data + (u32) hpg->encoder.instance[i].task.addr + sizeof(hpg->encoder.instance[i].pru));

        for (j = 0; j < hpg->encoder.instance[i].num_channels; j ++) {
            hpg->encoder.instance[i].chan[j].pru.hdr.A_pin = *(hpg->encoder.instance[i].chan[j].hal.pin.A_pin);
            hpg->encoder.instance[i].chan[j].pru.hdr.B_pin = *(hpg->encoder.instance[i].chan[j].hal.pin.B_pin);
            hpg->encoder.instance[i].chan[j].pru.hdr.Z_pin = *(hpg->encoder.instance[i].chan[j].hal.pin.index_pin);
            hpg->encoder.instance[i].chan[j].pru.hdr.mode  = *(hpg->encoder.instance[i].chan[j].hal.pin.counter_mode);

            hpg->encoder.instance[i].chan[j].pru.raw.dword[1]  = 0;
            hpg->encoder.instance[i].chan[j].pru.raw.dword[2]  = 0;

            pruchan[j] = hpg->encoder.instance[i].chan[j].pru;

            hpg->encoder.instance[i].chan[j].written_state = hpg->encoder.instance[i].chan[j].pru.raw.dword[0];
        }

        // LUT Table
        PRU_encoder_LUT_t *pru_lut = (PRU_encoder_LUT_t *) ((u32) hpg->pru_data + (u32) hpg->encoder.instance[i].LUT);
        *pru_lut = Counter_LUT;
    }

    // Call the regular update routine to finish up
    hpg_encoder_update(hpg);
}
