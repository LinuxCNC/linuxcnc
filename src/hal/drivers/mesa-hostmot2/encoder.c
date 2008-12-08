
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


//
//  This file contains the driver for the HostMot2 encoder v2 Module.
//
//  It supports Index and Index Mask, and high-fidelity velocity 
//  estimation.
//
//
//  Velocity estimation is made possible by a cool feature of the HostMot2
//  firmware:
//
//      The FPGA synthesizes a configurable-frequency "timestamp clock" by
//      dividing ClockLow by the value in the Quadrature Counter Timestamp
//      Divider Register.  (ClockLow is 33 MHz on the PCI cards and 50 MHz
//      on the 7i43.)
//
//      When a quadrature counter instance in the hostmot2 FPGA detects a
//      transition in its input Gray code, it increments the count and
//      latches both the (16-bit) count and the bottom 16 bits of the
//      timestamp clock into the Counter Register.
//
//  The velocity estimator used by the driver is similar to one described
//  by David Auslander in a paper titled "Vehicle-based Control Computer
//  Systems" (UCB ITS PRR 95 3), available at:
//
//      <http://repositories.cdlib.org/its/path/reports/UCB-ITS-PRR-95-3/>
//


#include <linux/slab.h>

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "hal.h"

#include "hal/drivers/mesa-hostmot2/hostmot2.h"




static void do_flag(u32 *reg, int condition, u32 bits) {
    if (condition) {
        *reg |= bits;
    } else {
        *reg &= ~bits;
    }
}


static void hm2_encoder_update_control_register(hostmot2_t *hm2) {
    int i;

    for (i = 0; i < hm2->encoder.num_instances; i ++) {
        hm2_encoder_instance_t *e = &hm2->encoder.instance[i];

        hm2->encoder.control_reg[i] = 0;

        do_flag(
            &hm2->encoder.control_reg[i],
            *e->hal.pin.index_enable,
            HM2_ENCODER_LATCH_ON_INDEX | HM2_ENCODER_INDEX_JUSTONCE
        );

        do_flag(
            &hm2->encoder.control_reg[i],
            e->hal.param.index_invert,
            HM2_ENCODER_INDEX_POLARITY
        );

        do_flag(
            &hm2->encoder.control_reg[i],
            e->hal.param.index_mask,
            HM2_ENCODER_INDEX_MASK
        );

        do_flag(
            &hm2->encoder.control_reg[i],
            e->hal.param.index_mask_invert,
            HM2_ENCODER_INDEX_MASK_POLARITY
        );

        do_flag(
            &hm2->encoder.control_reg[i],
            e->hal.param.counter_mode,
            HM2_ENCODER_COUNTER_MODE
        );

        do_flag(
            &hm2->encoder.control_reg[i],
            e->hal.param.filter,
            HM2_ENCODER_FILTER
        );
    }
}




void hm2_encoder_write(hostmot2_t *hm2) {
    int i;
    int need_update = 0;

    if (hm2->encoder.num_instances == 0) return;

    hm2_encoder_update_control_register(hm2);

    for (i = 0; i < hm2->encoder.num_instances; i ++) {
        if ((hm2->encoder.instance[i].prev_control & HM2_ENCODER_CONTROL_MASK) != (hm2->encoder.control_reg[i] & HM2_ENCODER_CONTROL_MASK)) {
            need_update = 1;
            break;
        }
    }

    if (need_update) {
        hm2->llio->write(
            hm2->llio,
            hm2->encoder.latch_control_addr,
            hm2->encoder.control_reg,
            (hm2->encoder.num_instances * sizeof(u32))
        );

        for (i = 0; i < hm2->encoder.num_instances; i ++) {
            hm2->encoder.instance[i].prev_control = hm2->encoder.control_reg[i];
        }
    }
}


void hm2_encoder_force_write(hostmot2_t *hm2) {
    int i;

    if (hm2->encoder.num_instances == 0) return;

    hm2_encoder_update_control_register(hm2);

    hm2->llio->write(
        hm2->llio,
        hm2->encoder.latch_control_addr,
        hm2->encoder.control_reg,
        (hm2->encoder.num_instances * sizeof(u32))
    );

    for (i = 0; i < hm2->encoder.num_instances; i ++) {
        hm2->encoder.instance[i].prev_control = hm2->encoder.control_reg[i];
    }

    hm2->llio->write(
        hm2->llio,
        hm2->encoder.timestamp_div_addr,
        &hm2->encoder.timestamp_div_reg,
        sizeof(u32)
    );
}




int hm2_encoder_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r;


    // 
    // some standard sanity checks
    //

    if (!hm2_md_is_consistent(hm2, md_index, 2, 5, 4, 0x0003)) {
        ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }

    if (hm2->encoder.num_instances != 0) {
        ERR(
            "found duplicate Module Descriptor for %s (inconsistent firmware), not loading driver\n",
            hm2_get_general_function_name(md->gtag)
        );
        return -EINVAL;
    }

    if (hm2->config.num_encoders > md->instances) {
        ERR(
            "config.num_encoders=%d, but only %d are available, not loading driver\n",
            hm2->config.num_encoders,
            md->instances
        );
        return -EINVAL;
    }

    if (hm2->config.num_encoders == 0) {
        return 0;
    }


    // 
    // looks good, start initializing
    // 


    if (hm2->config.num_encoders == -1) {
        hm2->encoder.num_instances = md->instances;
    } else {
        hm2->encoder.num_instances = hm2->config.num_encoders;
    }


    hm2->encoder.instance = (hm2_encoder_instance_t *)hal_malloc(hm2->encoder.num_instances * sizeof(hm2_encoder_instance_t));
    if (hm2->encoder.instance == NULL) {
        ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    hm2->encoder.stride = md->register_stride;
    hm2->encoder.clock_frequency = md->clock_freq;
    hm2->encoder.version = md->version;

    hm2->encoder.counter_addr = md->base_address + (0 * md->register_stride);
    hm2->encoder.latch_control_addr = md->base_address + (1 * md->register_stride);
    hm2->encoder.timestamp_div_addr = md->base_address + (2 * md->register_stride);
    hm2->encoder.timestamp_count_addr = md->base_address + (3 * md->register_stride);
    hm2->encoder.filter_rate_addr = md->base_address + (4 * md->register_stride);

    r = hm2_register_tram_read_region(hm2, hm2->encoder.counter_addr, (hm2->encoder.num_instances * sizeof(u32)), &hm2->encoder.counter_reg);
    if (r < 0) {
        ERR("error registering tram read region for Encoder Counter register (%d)\n", r);
        goto fail0;
    }

    hm2->encoder.control_reg = (u32 *)kmalloc(hm2->encoder.num_instances * sizeof(u32), GFP_KERNEL);
    if (hm2->encoder.control_reg == NULL) {
        ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }


    // export the encoders to HAL
    // FIXME: r hides the r in enclosing function, and it returns the wrong thing
    {
        int i;
        int r;
        char name[HAL_NAME_LEN + 2];

        for (i = 0; i < hm2->encoder.num_instances; i ++) {
            // pins
            rtapi_snprintf(name, HAL_NAME_LEN, "%s.encoder.%02d.rawcounts", hm2->llio->name, i);
            r = hal_pin_s32_new(name, HAL_OUT, &(hm2->encoder.instance[i].hal.pin.rawcounts), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.encoder.%02d.zero_offset", hm2->llio->name, i);
            r = hal_pin_s32_new(name, HAL_OUT, &(hm2->encoder.instance[i].hal.pin.zero_offset), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.encoder.%02d.count", hm2->llio->name, i);
            r = hal_pin_s32_new(name, HAL_OUT, &(hm2->encoder.instance[i].hal.pin.count), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.encoder.%02d.position", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_OUT, &(hm2->encoder.instance[i].hal.pin.position), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.encoder.%02d.velocity", hm2->llio->name, i);
            r = hal_pin_float_new(name, HAL_OUT, &(hm2->encoder.instance[i].hal.pin.velocity), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.encoder.%02d.reset", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IN, &(hm2->encoder.instance[i].hal.pin.reset), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.encoder.%02d.index-enable", hm2->llio->name, i);
            r = hal_pin_bit_new(name, HAL_IO, &(hm2->encoder.instance[i].hal.pin.index_enable), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding pin '%s', aborting\n", name);
                goto fail1;
            }


            // parameters
            rtapi_snprintf(name, HAL_NAME_LEN, "%s.encoder.%02d.scale", hm2->llio->name, i);
            r = hal_param_float_new(name, HAL_RW, &(hm2->encoder.instance[i].hal.param.scale), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding param '%s', aborting\n", name);
                goto fail1;
            }

            rtapi_snprintf(name, HAL_NAME_LEN, "%s.encoder.%02d.index-invert", hm2->llio->name, i);
            r = hal_param_bit_new(name, HAL_RW, &(hm2->encoder.instance[i].hal.param.index_invert), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding param '%s', aborting\n", name);
                goto fail1;
            }


            rtapi_snprintf(name, HAL_NAME_LEN, "%s.encoder.%02d.index-mask", hm2->llio->name, i);
            r = hal_param_bit_new(name, HAL_RW, &(hm2->encoder.instance[i].hal.param.index_mask), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding param '%s', aborting\n", name);
                goto fail1;
            }


            rtapi_snprintf(name, HAL_NAME_LEN, "%s.encoder.%02d.index-mask-invert", hm2->llio->name, i);
            r = hal_param_bit_new(name, HAL_RW, &(hm2->encoder.instance[i].hal.param.index_mask_invert), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding param '%s', aborting\n", name);
                goto fail1;
            }


            rtapi_snprintf(name, HAL_NAME_LEN, "%s.encoder.%02d.counter-mode", hm2->llio->name, i);
            r = hal_param_bit_new(name, HAL_RW, &(hm2->encoder.instance[i].hal.param.counter_mode), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding param '%s', aborting\n", name);
                goto fail1;
            }


            rtapi_snprintf(name, HAL_NAME_LEN, "%s.encoder.%02d.filter", hm2->llio->name, i);
            r = hal_param_bit_new(name, HAL_RW, &(hm2->encoder.instance[i].hal.param.filter), hm2->llio->comp_id);
            if (r != HAL_SUCCESS) {
                ERR("error adding param '%s', aborting\n", name);
                goto fail1;
            }


            //
            // init the hal objects that need it
            // the things not initialized here will be set by hm2_encoder_tram_init()
            //

            *hm2->encoder.instance[i].hal.pin.reset = 0;
            *hm2->encoder.instance[i].hal.pin.index_enable = 0;

            hm2->encoder.instance[i].hal.param.scale = 1.0;
            hm2->encoder.instance[i].hal.param.index_invert = 0;
            hm2->encoder.instance[i].hal.param.index_mask = 0;
            hm2->encoder.instance[i].hal.param.index_mask_invert = 0;
            hm2->encoder.instance[i].hal.param.counter_mode = 0;
            hm2->encoder.instance[i].hal.param.filter = 1;

        }
    }


    // 
    // Set the Timestamp Divisor Register
    // 
    // We want the timestamp to count as quickly as possible, so we get the
    // best temporal resolution.
    // 
    // But we want it to count slow enough that the 16-bit counter doesnt
    // overrun between successive calls to the servo thread (easy), and
    // even slower so that we can do good low-speed velocity estimation
    // (long between roll-overs).
    //
    // A resonably slow servo thread runs at 1 KHz.  A fast one runs at 10
    // KHz.  The actual servo period is unknown at loadtime, and is likely 
    // to fluctuate slightly when the system is under load.
    // 
    // Peter suggests a Quadrature Timestamp clock rate of 1 MHz.  This
    // means that a 1 KHz servo period sees about 1000 clocks per period.
    //     
    //
    // From the HM2 RegMap:
    // 
    //     Timestamp count rate is ClockLow/(TSDiv+2).
    //     Any divisor with MSb set = divide by 1
    // 
    // This gives us:
    // 
    //     rate = 1 MHz = 1e6 = ClockLow / (TSDiv+2)
    // 
    //     TSDiv+2 = ClockLow / 1e6
    // 
    //     TSDiv = (ClockLow / 1e6) - 2
    //
    //     seconds_per_clock = 1 / rate = (TSDiv+2) / ClockLow
    //
    //
    // The 7i43 has a 50 MHz ClockLow, giving TSDiv = 48 and 1 us/clock
    // The PCI cards have a 33 MHz ClockLow, giving TSDiv = 31 and again 1 us/clock
    //

    hm2->encoder.timestamp_div_reg = (hm2->encoder.clock_frequency / 1e6) - 2;
    hm2->encoder.seconds_per_tsdiv_clock = (hal_float_t)(hm2->encoder.timestamp_div_reg + 2) / (hal_float_t)hm2->encoder.clock_frequency;


    return hm2->encoder.num_instances;


fail1:
    kfree(hm2->encoder.control_reg);

fail0:
    hm2->encoder.num_instances = 0;
    return r;
}


void hm2_encoder_tram_init(hostmot2_t *hm2) {
    int i;

    // all the encoders start "stopped where they are"
    for (i = 0; i < hm2->encoder.num_instances; i ++) {
        u16 count, timestamp;

        count = hm2->encoder.counter_reg[i] & 0x0000ffff;
        timestamp = (hm2->encoder.counter_reg[i] >> 16) & 0x0000ffff;

        *hm2->encoder.instance[i].hal.pin.rawcounts = count;
        *hm2->encoder.instance[i].hal.pin.zero_offset = count;

        *hm2->encoder.instance[i].hal.pin.count = 0;
        *hm2->encoder.instance[i].hal.pin.position = 0.0;
        *hm2->encoder.instance[i].hal.pin.velocity = 0.0;

        hm2->encoder.instance[i].prev_reg_count = count;
        hm2->encoder.instance[i].prev_reg_timestamp = timestamp;

        hm2->encoder.instance[i].state = HM2_ENCODER_STOPPED;

        // Note: we dont initialize the other datapoints , because in the
        // "stopped" state they're not used
        hm2->encoder.instance[i].datapoint[1].raw_count = count;
        hm2->encoder.instance[i].datapoint[1].raw_timestamp = timestamp;

    }
}




/**
 * @brief Updates the driver's idea of the encoder's position.
 *
 * Sets these variables:
 *
 *     hal.pin.count
 *     hal.pin.position
 *     hal.pin.rawcounts
 *     datapoint[0].raw_count
 *     (maybe) .index_enabled and .zero_offset
 *
 * Does not update .prev_reg_count or the other datapoints or any
 * timestamps or anything else.
 *
 * This function expects the TRAM read to have just finished, so that
 * counter_reg[i] is up-to-date.
 *
 * May read the Latch register (if searching for the Index pulse).
 *
 * @param hm2 The hostmot2 structure being worked on.
 *
 * @param instance The index of the encoder instance.
 */

static void hm2_encoder_instance_update_position(hostmot2_t *hm2, int instance) {
    u16 reg_count;
    s32 reg_count_diff;

    hm2_encoder_instance_t *e;

    e = &hm2->encoder.instance[instance];


    // get current count from the FPGA (already read)
    reg_count = hm2->encoder.counter_reg[instance] & 0x0000ffff;


    // 
    // figure out current rawcounts accumulated by the driver
    // 

    reg_count_diff = (s32)reg_count - (s32)e->prev_reg_count;
    if (reg_count_diff > 32768) reg_count_diff -= 65536;
    if (reg_count_diff < -32768) reg_count_diff += 65536;

    e->datapoint[0].raw_count = e->datapoint[1].raw_count + reg_count_diff;
    *e->hal.pin.rawcounts = e->datapoint[0].raw_count;


    //
    // if we've told the FPGA we're looking for an index pulse:
    //     read the latch/ctrl register
    //     if it's triggered set zero_offset to the rawcounts version of the latched count
    //

    if (e->prev_control & HM2_ENCODER_LATCH_ON_INDEX) {
        u32 latch_ctrl;

        hm2->llio->read(
            hm2->llio,
            hm2->encoder.latch_control_addr + (instance * sizeof(u32)),
            &latch_ctrl,
            sizeof(u32)
        );

        if (0 == (latch_ctrl & HM2_ENCODER_LATCH_ON_INDEX)) {
            // hm2 reports index event occurred

            u16 latched_count;

            latched_count = (latch_ctrl >> 16) & 0xffff;

            reg_count_diff = (s32)latched_count - (s32)e->prev_reg_count;
            if (reg_count_diff > 32768) reg_count_diff -= 65536;
            if (reg_count_diff < -32768) reg_count_diff += 65536;

            *e->hal.pin.zero_offset = e->datapoint[1].raw_count + reg_count_diff;

            *e->hal.pin.index_enable = 0;
        }
    }


    // 
    // reset count if the user wants us to (by just setting the zero offset to the current rawcounts)
    //

    if (*e->hal.pin.reset) {
        *e->hal.pin.zero_offset = *e->hal.pin.rawcounts;
    }


    // 
    // Now we know the current rawcounts and zero_offset, which together
    // tell us the current count.
    //
    // From that we easily compute count and scaled position.
    //

    *e->hal.pin.count = *e->hal.pin.rawcounts - *e->hal.pin.zero_offset;


    //
    // Now we know the current current .count, from this we easily compute
    // the scaled position.
    //

    *e->hal.pin.position = *e->hal.pin.count / e->hal.param.scale;


    e->prev_reg_count = reg_count;
}




/**
 * @brief Estimates .velocity.
 *
 * This expects the TRAM read to have just finished, and the position
 * information to be updated.
 *
 * The hm2 encoder driver (that's this code!) keeps track of the following
 * information:
 *
 *     * "Current datapoint": The count & timestamp reported most recently
 *       (the calling code just read that from the FPGA).
 *
 *     * "Previous datapoint": The count & timestamp reported last time the
 *       count changed.  This might have been from last time we read it
 *       (ie, the time before this time), or it might have been long before
 *       then (if the encoder is stopped or moving slowly compared to its
 *       resolution).
 *
 *     * "Second-previous datapoint": The count & timestamp reported the
 *       second-most-recent time the count changed.
 *
 *     * "Previous velocity": The velocity between Second-prev and Prev
 *       datapoints.
 *
 * From this we make an estimate of current velocity.
 *
 * If the Current datapoint differs from the Previous datapoint, it's
 * easy, use a Reciprocal Time velocity estimator.  Then push all the
 * datapoints back one, note that we're moving, and we're done.
 *
 * If, on the other hand, the Current datapoint is the *same* as the
 * Previous datapoint, then we're either moving slowly or stopped.
 *
 *     Read the Quadrature Timestamp Count Register, compute a kind of
 *     raw_timestamp.
 *
 *     If difference between the current timestamp and the Prev
 *     datapoint timestamp is greater than some velocity estimation
 *     threshold, set Velocity to zero, note that we're stopped, and
 *     we're done.
 *
 *     Make up a hypothetical "Next datapoint", which has "one encoder
 *     count increment in the same direction as Second-Previous to Previous
 *     motion, one timestamp clock after the current timestamp from the
 *     TSC register."
 *
 *     Use RT estimator to compute "Next velocity" from Prev to Next.
 *
 *     If Next velocity 
 *     
 *
 * @param hm2 The hostmot2 structure being worked on.
 *
 * @param instance The index of the encoder instance.
 */

static void hm2_encoder_instance_estimate_velocity(hostmot2_t *hm2, int instance) {
    u16 reg_timestamp;

    s32 timestamp_diff_tsdiv_clocks;
    hal_float_t timestamp_diff_s;

    hm2_encoder_instance_t *e;

    e = &hm2->encoder.instance[instance];


    //
    // get the timestamp of the most recent encoder count change (recently read from the FPGA)
    //

    reg_timestamp = (hm2->encoder.counter_reg[instance] >> 16) & 0x0000ffff;


    if (0 && (instance == 2)) {
        int di;
        PRINT("entering function, encoder.%02d, state=%d, reg_timestamp=%hu\n", instance, e->state, reg_timestamp);
        for (di = 0; di < 3; di ++) {
            PRINT("    d[%d] = ( %d, %u )\n", di, e->datapoint[di].raw_count, e->datapoint[di].raw_timestamp);
        }
    }


    // 
    // here comes the little state machine for figuring out velocity
    //

    switch (e->state) {

        case HM2_ENCODER_STOPPED: {
            if (e->datapoint[0].raw_count == e->datapoint[1].raw_count) {
                // still stopped, dont need to update datapoints or hal.pin.velocity
                break;
            }

            //
            // If we get here, we *were* stopped but we've started
            // moving since last time through the loop.  We do not have
            // enough information to estimate velocity yet.
            //
            // FIXME: i could make something up here - should i?
            //

            // FIXME: raw vs reg timestamp
            e->datapoint[0].raw_timestamp = reg_timestamp;

            e->datapoint[1] = e->datapoint[0];
            e->datapoint[2] = e->datapoint[0];

            e->state = HM2_ENCODER_MOVING;

            break;
        }

        case HM2_ENCODER_MOVING: {
            s32 raw_counts_diff;

            if (e->datapoint[0].raw_count != e->datapoint[1].raw_count) {
                // moving fast enough to just use the Relative Time estimator

                // FIXME: raw vs reg timestamp
                e->datapoint[0].raw_timestamp = reg_timestamp;


                // 
                // first get the time since the previous encoder change
                // FIXME: if the encoder's been stopped, this might have rolled over
                // 

                timestamp_diff_tsdiv_clocks = e->datapoint[0].raw_timestamp - e->datapoint[1].raw_timestamp;
                if (timestamp_diff_tsdiv_clocks < 0) {
                    timestamp_diff_tsdiv_clocks += 65536;
                } else if (timestamp_diff_tsdiv_clocks == 0) {
                    // this should never happen while we're moving,
                    // unless it's going too fast for the quadrature
                    // timestamp clock
                    ERR("encoder.%02d velocity too high, clipping!\n", instance);

                    {
                        int di;
                        PRINT("encoder.%02d, state=%d\n", instance, e->state);
                        for (di = 0; di < 3; di ++) {
                            PRINT("    d[%d] = ( %d, %u )\n", di, e->datapoint[di].raw_count, e->datapoint[di].raw_timestamp);
                        }
                    }

                    timestamp_diff_tsdiv_clocks = 1;
                }

                timestamp_diff_s = (hal_float_t)timestamp_diff_tsdiv_clocks * hm2->encoder.seconds_per_tsdiv_clock;


                // 
                // this is the RT (dS/dT) velocity estimator
                //

                raw_counts_diff = e->datapoint[0].raw_count - e->datapoint[1].raw_count;
                *e->hal.pin.velocity = (raw_counts_diff / timestamp_diff_s) / e->hal.param.scale;

                e->datapoint[2] = e->datapoint[1];
                e->datapoint[1] = e->datapoint[0];

                break;

            } else {
                // We're moving, but we havent taken a step since last
                // time through the loop.  This is the interesting situation.

                // FIXME: raw vs reg timestamp
                e->datapoint[0].raw_timestamp = reg_timestamp;

                *e->hal.pin.velocity = 0.0;

                e->datapoint[2] = e->datapoint[1];
                e->datapoint[1] = e->datapoint[0];

                e->state = HM2_ENCODER_STOPPED;

                break;
            }

        }


        default: {
            ERR("encoder.%02d is in unknown state %d, switching to stopped\n", instance, e->state);

            // FIXME: raw vs reg timestamp
            e->datapoint[0].raw_timestamp = reg_timestamp;
            e->datapoint[1] = e->datapoint[0];
            e->datapoint[2] = e->datapoint[0];

            *e->hal.pin.velocity = 0.0;

            e->state = HM2_ENCODER_STOPPED;

            break;
        }

    }
}




/**
 * @brief Processes the information received from the QuadratureCounter
 *     (encoder) instance to produce an estimate of position & velocity.
 */

static void hm2_encoder_instance_process_tram_read(hostmot2_t *hm2, int instance) {
    hm2_encoder_instance_t *e;

    e = &hm2->encoder.instance[instance];

    // sanity check
    if (e->hal.param.scale == 0.0) {
        ERR("encoder.%02d.scale == 0.0, bogus, setting to 1.0\n", instance);
        e->hal.param.scale = 1.0;
    }


    // 
    // these two functions do all the work
    //

    hm2_encoder_instance_update_position(hm2, instance);

    hm2_encoder_instance_estimate_velocity(hm2, instance);
}




void hm2_encoder_process_tram_read(hostmot2_t *hm2, long l_period_ns) {
    int i;

    if (hm2->encoder.num_instances == 0) return;

    // read counters & timestamps
    for (i = 0; i < hm2->encoder.num_instances; i ++) {
        hm2_encoder_instance_process_tram_read(hm2, i);
    }
}


void hm2_encoder_cleanup(hostmot2_t *hm2) {
    kfree(hm2->encoder.control_reg);
}


void hm2_encoder_print_module(hostmot2_t *hm2) {
    int i;
    PRINT("Encoders: %d\n", hm2->encoder.num_instances);
    if (hm2->encoder.num_instances <= 0) return;
    PRINT("    clock_frequency: %d Hz (%s MHz)\n", hm2->encoder.clock_frequency, hm2_hz_to_mhz(hm2->encoder.clock_frequency));
    PRINT("    version: %d\n", hm2->encoder.version);
    PRINT("    counter_addr: 0x%04X\n", hm2->encoder.counter_addr);
    PRINT("    latch_control_addr: 0x%04X\n", hm2->encoder.latch_control_addr);
    PRINT("    timestamp_div_addr: 0x%04X\n", hm2->encoder.timestamp_div_addr);
    PRINT("    timestamp_count_addr: 0x%04X\n", hm2->encoder.timestamp_count_addr);
    PRINT("    filter_rate_addr: 0x%04X\n", hm2->encoder.filter_rate_addr);
    PRINT("    timestamp_div: 0x%04X\n", (u16)hm2->encoder.timestamp_div_reg);
    for (i = 0; i < hm2->encoder.num_instances; i ++) {
        PRINT("    instance %d:\n", i);
        PRINT("        hw:\n");
        PRINT("            counter = %04x.%04x\n", (hm2->encoder.counter_reg[i] >> 16), (hm2->encoder.counter_reg[i] & 0xffff));
        PRINT("            latch/control = %04x.%04x\n", (hm2->encoder.control_reg[i] >> 16), (hm2->encoder.control_reg[i] & 0xffff));
        PRINT("            prev_control = %04x.%04x\n", (hm2->encoder.instance[i].prev_control >> 16), (hm2->encoder.instance[i].prev_control & 0xffff));
    }
}

