
//    Copyright (C) 2012 Michael Geszkiewicz
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

#ifndef __ENCODER_H
#define __ENCODER_H

#define HM2_ENCODER_QUADRATURE_ERROR    (1<<15)
#define HM2_ENCODER_AB_MASK_POLARITY    (1<<14)
#define HM2_ENCODER_LATCH_ON_PROBE      (1<<13)
#define HM2_ENCODER_PROBE_POLARITY      (1<<12)
#define HM2_ENCODER_FILTER              (1<<11)
#define HM2_ENCODER_COUNTER_MODE        (1<<10)
#define HM2_ENCODER_INDEX_MASK          (1<<9)
#define HM2_ENCODER_INDEX_MASK_POLARITY (1<<8)
#define HM2_ENCODER_INDEX_JUSTONCE      (1<<6)
#define HM2_ENCODER_CLEAR_ON_INDEX      (1<<5)
#define HM2_ENCODER_LATCH_ON_INDEX      (1<<4)
#define HM2_ENCODER_INDEX_POLARITY      (1<<3)

#define HM2_ENCODER_CONTROL_MASK  (0x0000ffff)


#define hm2_encoder_get_reg_count(hm2, instance)     (hm2->encoder.counter_reg[instance] & 0x0000ffff)
#define hm2_encoder_get_reg_timestamp(hm2, instance) ((hm2->encoder.counter_reg[instance] >> 16) & 0x0000ffff)
#define hm2_encoder_get_reg_tsc(hm2)                 ((*hm2->encoder.timestamp_count_reg) & 0xFFFF)

typedef struct {

    struct {

        struct {
            hal_s32_t *rawcounts;    // raw encoder counts
            hal_s32_t *rawlatch;     // raw encoder of latch
            hal_s32_t *count;        // (rawcounts - zero_offset)
            hal_s32_t *count_latch;  // (rawlatch - zero_offset)
            hal_float_t *position;
            hal_float_t *position_latch;
            hal_float_t *velocity;
            hal_bit_t *reset;
            hal_bit_t *index_enable;
            hal_bit_t *latch_enable;
            hal_bit_t *latch_polarity;
        } pin;

        struct {
            hal_float_t scale;
            hal_bit_t index_invert;
            hal_bit_t index_mask;
            hal_bit_t index_mask_invert;
            hal_bit_t counter_mode;
            hal_bit_t filter;
            hal_float_t vel_timeout;
        } param;

    } hal;

    s32 zero_offset;  // *hal.pin.counts == (*hal.pin.rawcounts - zero_offset)

    u16 prev_reg_count;  // from this and the current count in the register we compute a change-in-counts, which we add to rawcounts

    s32 prev_dS_counts;  // last time the function ran, it saw this many counts from the time before *that*

    u32 prev_control;

    // these two are the datapoint last time we moved (only valid if state == HM2_ENCODER_MOVING)
    s32 prev_event_rawcounts;
    u16 prev_event_reg_timestamp;

    s32 tsc_num_rollovers;
    u16 prev_time_of_interest;

    enum { HM2_ENCODER_STOPPED, HM2_ENCODER_MOVING } state;

} hm2_encoder_instance_t;


typedef struct {
    int num_instances;

    hm2_encoder_instance_t *instance;

    u32 stride;
    u32 clock_frequency;
    u8 version;

    // hw registers
    u32 counter_addr;
    u32 *counter_reg;

    u32 latch_control_addr;
    u32 *control_reg;

    u32 timestamp_div_addr;
    u32 timestamp_div_reg;  // one register for the whole Function
    hal_float_t seconds_per_tsdiv_clock;

    u32 timestamp_count_addr;
    u32 *timestamp_count_reg;
    u32 prev_timestamp_count_reg;

    u32 filter_rate_addr;
} hm2_encoder_t;

#endif
