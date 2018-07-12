
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

#ifndef __HOSTMOT2_H
#define __HOSTMOT2_H

#include <rtapi_list.h>

#include "rtapi.h"
#include "hal.h"
#include "sserial.h"

#include "hostmot2-lowlevel.h"

#ifndef FIRMWARE_NAME_MAX
    #define FIRMWARE_NAME_MAX  30
#endif

#define HM2_VERSION "0.15"
#define HM2_NAME    "hm2"


//
// Note: HM2_PRINT() and HM2_PRINT_NO_LL() use rtapi_print(), all the others use rtapi_print_msg()
//

#define HM2_PRINT_NO_LL(fmt, args...)  rtapi_print(HM2_NAME ": " fmt, ## args)

#define HM2_ERR_NO_LL(fmt, args...)    rtapi_print_msg(RTAPI_MSG_ERR,  HM2_NAME ": " fmt, ## args)
#define HM2_WARN_NO_LL(fmt, args...)   rtapi_print_msg(RTAPI_MSG_WARN, HM2_NAME ": " fmt, ## args)
#define HM2_INFO_NO_LL(fmt, args...)   rtapi_print_msg(RTAPI_MSG_INFO, HM2_NAME ": " fmt, ## args)
#define HM2_DBG_NO_LL(fmt, args...)    rtapi_print_msg(RTAPI_MSG_DBG,  HM2_NAME ": " fmt, ## args)


#define HM2_PRINT(fmt, args...)  rtapi_print(HM2_NAME "/%s: " fmt, hm2->llio->name, ## args)

#define HM2_ERR(fmt, args...)    rtapi_print_msg(RTAPI_MSG_ERR,  HM2_NAME "/%s: " fmt, hm2->llio->name, ## args)
#define HM2_WARN(fmt, args...)   rtapi_print_msg(RTAPI_MSG_WARN, HM2_NAME "/%s: " fmt, hm2->llio->name, ## args)
#define HM2_INFO(fmt, args...)   rtapi_print_msg(RTAPI_MSG_INFO, HM2_NAME "/%s: " fmt, hm2->llio->name, ## args)
#define HM2_DBG(fmt, args...)    rtapi_print_msg(RTAPI_MSG_DBG,  HM2_NAME "/%s: " fmt, hm2->llio->name, ## args)




// 
// idrom addresses & constants
// 

#define HM2_ADDR_IOCOOKIE  (0x0100)
#define HM2_IOCOOKIE       (0x55AACAFE)

#define HM2_ADDR_CONFIGNAME    (0x0104)
#define HM2_CONFIGNAME         "HOSTMOT2"
#define HM2_CONFIGNAME_LENGTH  (8)

#define HM2_ADDR_IDROM_OFFSET (0x010C)

#define HM2_MAX_MODULE_DESCRIPTORS  (48)
#define HM2_MAX_PIN_DESCRIPTORS     (1000)

// 
// Pin Descriptor constants
// 

#define HM2_PIN_SOURCE_IS_PRIMARY   (0x00000000)
#define HM2_PIN_SOURCE_IS_SECONDARY (0x00000001)

#define HM2_PIN_DIR_IS_INPUT     (0x00000002)
#define HM2_PIN_DIR_IS_OUTPUT    (0x00000004)


// 
// Module Descriptor constants
// 

#define HM2_GTAG_WATCHDOG          (2)
#define HM2_GTAG_IOPORT            (3)
#define HM2_GTAG_ENCODER           (4)
#define HM2_GTAG_STEPGEN           (5)
#define HM2_GTAG_PWMGEN            (6)
#define HM2_GTAG_SPI               (7) // Not supported
#define HM2_GTAG_SSI               (8)
#define HM2_GTAG_UART_TX           (9)
#define HM2_GTAG_UART_RX           (10)
#define HM2_GTAG_PKTUART_TX        (27)  // PktUART uses same addresses as normal UART with 
#define HM2_GTAG_PKTUART_RX        (28) // the assumption you would not use both in one config
#define HM2_GTAG_TRANSLATIONRAM    (11)
#define HM2_GTAG_MUXED_ENCODER     (12)
#define HM2_GTAG_MUXED_ENCODER_SEL (13)
#define HM2_GTAG_BSPI              (14)
#define HM2_GTAG_DBSPI             (15) // Not supported
#define HM2_GTAG_DPLL              (16) // Not supported
#define HM2_GTAG_MUXED_ENCODER_M   (17) // Not supported
#define HM2_GTAG_MUXED_ENC_SEL_M   (18) // Not supported
#define HM2_GTAG_TPPWM             (19)
#define HM2_GTAG_WAVEGEN           (20) // Not supported
#define HM2_GTAG_DAQFIFO           (21) // Not supported
#define HM2_GTAG_BINOSC            (22) // Not supported
#define HM2_GTAG_DDMA              (23) // Not supported
#define HM2_GTAG_BISS              (24) 
#define HM2_GTAG_FABS              (25) 
#define HM2_GTAG_HM2DPLL           (26) 
#define HM2_GTAG_LIOPORT           (64) // Not supported
#define HM2_GTAG_LED               (128)

#define HM2_GTAG_RESOLVER          (192)
#define HM2_GTAG_SMARTSERIAL       (193)
#define HM2_GTAG_TWIDDLER          (194) // Not supported
#define HM2_GTAG_SSR               (195)



//
// IDROM and MD structs
//


typedef struct {
    rtapi_u32 idrom_type;
    rtapi_u32 offset_to_modules;
    rtapi_u32 offset_to_pin_desc;
    rtapi_u8 board_name[8];  // ascii string, but not NULL terminated!
    rtapi_u32 fpga_size;
    rtapi_u32 fpga_pins;
    rtapi_u32 io_ports;
    rtapi_u32 io_width;
    rtapi_u32 port_width;
    rtapi_u32 clock_low;
    rtapi_u32 clock_high;
    rtapi_u32 instance_stride_0;
    rtapi_u32 instance_stride_1;
    rtapi_u32 register_stride_0;
    rtapi_u32 register_stride_1;
} hm2_idrom_t;


typedef struct {
    rtapi_u8 gtag;
    rtapi_u8 version;
    rtapi_u8 clock_tag;
    rtapi_u32 clock_freq;  // this one's not in the MD struct in the device, it's set from clock_tag and the idrom header for our convenience
    rtapi_u8 instances;
    rtapi_u16 base_address;

    rtapi_u8 num_registers;
    rtapi_u32 register_stride;
    rtapi_u32 instance_stride;
    rtapi_u32 multiple_registers;
} hm2_module_descriptor_t;




// 
// these structures keep track of the FPGA's I/O pins; and for I/O pins
// used as GPIOs, keep track of the HAL state of the pins
//
// Pins and GPIOs are closely tied to the IOPort Function
//

typedef struct {
    struct {

        struct {
            hal_bit_t *in;
            hal_bit_t *in_not;
            hal_bit_t *out;
        } pin;

        struct {
            hal_bit_t is_output;
            hal_bit_t is_opendrain;
            hal_bit_t invert_output;
        } param;

    } hal;
} hm2_gpio_instance_t;


typedef struct {
    // these are from the Pin Descriptor in the HM2 IDROM
    rtapi_u8 sec_pin;
    rtapi_u8 sec_tag;
    rtapi_u8 sec_unit;
    rtapi_u8 primary_tag;
    rtapi_u8 port_num;
    rtapi_u8 port_pin;
    rtapi_u8 bit_num;


    //
    // below here is how the driver keeps track of each pin
    //

    // the actual function using this pin
    int gtag;

    // either HM2_PIN_DIR_IS_INPUT or HM2_PIN_DIR_IS_OUTPUT
    // if gtag != gpio, how the owning module instance configured it at load-time
    // if gtag == gpio, this gets copied from the .is_output parameter
    int direction;

    // if the driver decides to make this pin a gpio, it'll allocate the
    // instance struct to manage it, otherwise instance is NULL
    hm2_gpio_instance_t *instance;
} hm2_pin_t;




//
// these structures translate between HostMot2 Functions and HAL objects
//


//
// encoders
//

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
#define HM2_ENCODER_INPUT_INDEX         (1<<2)
#define HM2_ENCODER_INPUT_B             (1<<1)
#define HM2_ENCODER_INPUT_A             (1<<0)

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
            hal_bit_t *quadrature_error;
            hal_bit_t *quadrature_error_enable;
            hal_bit_t *input_a;
            hal_bit_t *input_b;
            hal_bit_t *input_idx;
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

    rtapi_s32 zero_offset;  // *hal.pin.counts == (*hal.pin.rawcounts - zero_offset)

    rtapi_u16 prev_reg_count;  // from this and the current count in the register we compute a change-in-counts, which we add to rawcounts

    rtapi_s32 prev_dS_counts;  // last time the function ran, it saw this many counts from the time before *that*

    rtapi_u32 prev_control;

    // these two are the datapoint last time we moved (only valid if state == HM2_ENCODER_MOVING)
    rtapi_s32 prev_event_rawcounts;
    rtapi_u16 prev_event_reg_timestamp;

    rtapi_s32 tsc_num_rollovers;
    rtapi_u16 prev_time_of_interest;

    enum { HM2_ENCODER_STOPPED, HM2_ENCODER_MOVING } state;

} hm2_encoder_instance_t;


// these hal params affect all encoder instances
typedef struct {
    struct {
        hal_u32_t *sample_frequency;
        hal_u32_t *skew;
        hal_s32_t *dpll_timer_num;
    } pin;
} hm2_encoder_module_global_t;

typedef struct {
    int num_instances;

    hm2_encoder_instance_t *instance;

    rtapi_u32 stride;
    rtapi_u32 clock_frequency;
    rtapi_u8 version;

    // module-global HAL objects...
    hm2_encoder_module_global_t *hal;
    rtapi_u32 written_sample_frequency;
    int has_skew;
    rtapi_u32 written_skew;
    uint32_t desired_dpll_timer_reg, written_dpll_timer_reg;

    // hw registers
    rtapi_u32 counter_addr;
    rtapi_u32 *counter_reg;

    rtapi_u32 latch_control_addr;
    rtapi_u32 *control_reg;
    rtapi_u32 *read_control_reg;

    rtapi_u32 timestamp_div_addr;
    rtapi_u32 timestamp_div_reg;  // one register for the whole Function
    hal_float_t seconds_per_tsdiv_clock;

    rtapi_u32 timestamp_count_addr;
    rtapi_u32 *timestamp_count_reg;
    rtapi_u32 prev_timestamp_count_reg;

    rtapi_u32 filter_rate_addr;

    rtapi_u32 dpll_timer_num_addr;
} hm2_encoder_t;

//
// absolute encoder
//

#define MAX_ABSENCS (32)
#define MAX_ABSENC_LEN (128)

typedef struct {
    int gtag;
    int index;
    char string[MAX_ABSENC_LEN];
    struct rtapi_list_head list;
} hm2_absenc_format_t;

/* The absolute encoder protocols, with a bit field containing many
 * different data points end up looking so much like the smart-serial
 * protocol that the module uses the smart-serial data structure and
 * much of the same functions
 */

typedef struct {
    int num_chans;
    hm2_sserial_remote_t *chans;

    rtapi_u32 clock_frequency;
    rtapi_u8 ssi_version;
    rtapi_u8 biss_version;
    rtapi_u8 fanuc_version;
    rtapi_u32 ssi_global_start_addr;
    rtapi_u32 fabs_global_start_addr;
    rtapi_u32 biss_global_start_addr;
    rtapi_u32 *biss_busy_flags;
    rtapi_u32 *ssi_busy_flags;
    rtapi_u32 *fabs_busy_flags;
} hm2_absenc_t;

//
// resolver
//

typedef struct {

    struct {

        struct {
            hal_s32_t *rawcounts;
            hal_s32_t *count;
            hal_float_t *angle;
            hal_float_t *position;
            hal_float_t *velocity;
            hal_bit_t *reset;
            hal_bit_t *index_enable;
            hal_bit_t *error;
            hal_float_t *joint_pos_fb;
        } pin;

        struct {
            hal_float_t scale;
            hal_float_t vel_scale;
            hal_u32_t index_div;
            hal_bit_t use_abs;
        } param;

    } hal;
    
    rtapi_s64 accum;
    rtapi_s64 offset;
    rtapi_u32 old_reg;
    rtapi_u32 index_cnts;

} hm2_resolver_instance_t;

typedef struct {
    struct {
        hal_float_t excitation_khz;
    } param;
} hm2_resolver_global_t;

typedef struct {
    int num_instances;
    int num_resolvers;

    hm2_resolver_global_t *hal;
    hm2_resolver_instance_t *instance;

    rtapi_u32 stride;
    rtapi_u32 clock_frequency;
    rtapi_u8 version;

    // hw registers
    rtapi_u32 status_addr;
    rtapi_u32 *status_reg;
    
    rtapi_u32 command_addr;
    
    rtapi_u32 data_addr;
    
    rtapi_u32 position_addr;
    rtapi_u32 *position_reg;

    rtapi_u32 velocity_addr;
    rtapi_s32 *velocity_reg;
    
    hal_float_t written_khz;
    hal_float_t kHz;
    
} hm2_resolver_t;


//
// pwmgen
// 

#define HM2_PWMGEN_OUTPUT_TYPE_PWM          1  // this is the same value that the software pwmgen component uses
#define HM2_PWMGEN_OUTPUT_TYPE_UP_DOWN      2  // this is the same value that the software pwmgen component uses
#define HM2_PWMGEN_OUTPUT_TYPE_PDM          3  // software pwmgen does not support pdm as an output type
#define HM2_PWMGEN_OUTPUT_TYPE_PWM_SWAPPED  4  // software pwmgen does not support pwm/swapped output type because it doesnt need to 

typedef struct {

    struct {

        struct {
            hal_float_t *value;
            hal_bit_t *enable;
        } pin;

        struct {
            hal_float_t scale;
            hal_s32_t output_type;
        } param;

    } hal;

    // this keeps track of the output_type that we've told the FPGA, so we
    // know if we need to update it
    rtapi_s32 written_output_type;

    // this keeps track of the enable bit for this instance that we've told
    // the FPGA, so we know if we need to update it
    rtapi_s32 written_enable;
} hm2_pwmgen_instance_t;


// these hal params affect all pwmgen instances
typedef struct {
    struct {
        hal_u32_t pwm_frequency;
        hal_u32_t pdm_frequency;
    } param;
} hm2_pwmgen_module_global_t;


typedef struct {
    int num_instances;
    hm2_pwmgen_instance_t *instance;

    rtapi_u32 clock_frequency;
    rtapi_u8 version;


    // module-global HAL objects...
    hm2_pwmgen_module_global_t *hal;

    // these keep track of the most recent hal->param.p{d,w}m_frequency
    // that we've told the FPGA about, so we know if we need to update it
    rtapi_u32 written_pwm_frequency;
    rtapi_u32 written_pdm_frequency;

    // number of bits of resolution of the PWM signal (PDM is fixed at 12 bits)
    int pwm_bits;


    rtapi_u32 pwm_value_addr;
    rtapi_u32 *pwm_value_reg;

    rtapi_u32 pwm_mode_addr;
    rtapi_u32 *pwm_mode_reg;

    rtapi_u32 pwmgen_master_rate_dds_addr;
    rtapi_u32 pwmgen_master_rate_dds_reg;  // one register for the whole Function

    rtapi_u32 pdmgen_master_rate_dds_addr;
    rtapi_u32 pdmgen_master_rate_dds_reg;  // one register for the whole Function

    rtapi_u32 enable_addr;
    rtapi_u32 enable_reg;  // one register for the whole Function
} hm2_pwmgen_t;


//
// 3-Phase pwmgen
//


typedef struct {

    struct {

        struct {
            hal_float_t *Avalue;
            hal_float_t *Bvalue;
            hal_float_t *Cvalue;
            hal_bit_t *fault;
            hal_bit_t *enable;
        } pin;

        struct {
            hal_float_t scale;
            hal_float_t deadzone;
            hal_bit_t faultpolarity;
            hal_float_t sampletime;
        } param;

    } hal;

    // these keep track of the written values of each register so we
    // know if an update-write is needed
    // enable is a little more complicated and is based on the read-back
    // of the fault/enable register
    float written_deadzone;
    int written_faultpolarity;
    float written_sampletime;
} hm2_tp_pwmgen_instance_t;

typedef struct {
    struct {
        hal_u32_t pwm_frequency; // One PWM rate for all instances
    } param;
} hm2_tp_pwmgen_global_hal_t;

typedef struct {
    int num_instances;

    hm2_tp_pwmgen_instance_t *instance;

    hm2_tp_pwmgen_global_hal_t *hal;

    rtapi_u32 clock_frequency;
    rtapi_u8 version;

    // these keep track of the most recent hal->param.p{d,w}m_frequency
    // that we've told the FPGA about, so we know if we need to update it
    rtapi_u32 written_pwm_frequency;

    rtapi_u32 pwm_value_addr; // All three phases share a register (10 bits each)
    rtapi_u32 *pwm_value_reg; // Pointer to a memory block that holds the set.

    rtapi_u32 setup_addr; // holds dead-time, fault polarity and ADC sample time
    rtapi_u32 *setup_reg;

    rtapi_u32 enable_addr; // a 32-bit enable register for each tp_pwmgen. Seems excessive
    rtapi_u32 *enable_reg;

    rtapi_u32 pwmgen_master_rate_dds_addr;
    rtapi_u32 pwmgen_master_rate_dds_reg;  // one register for the whole Function

} hm2_tp_pwmgen_t;


// 
// ioport
// 

typedef struct {
    int num_instances;

    // register buffers
    // NOTE: there is just one data register for both reading and writing,
    // but the hostmot2 driver's TRAM support can't deal with that so we
    // need two copies...
    rtapi_u32 data_addr;
    rtapi_u32 *data_read_reg;
    rtapi_u32 *data_write_reg;

    rtapi_u32 ddr_addr;
    rtapi_u32 *ddr_reg;
    rtapi_u32 *written_ddr;  // not a register, but a copy of the most recently written value

    rtapi_u32 alt_source_addr;
    rtapi_u32 *alt_source_reg;

    rtapi_u32 open_drain_addr;
    rtapi_u32 *open_drain_reg;
    rtapi_u32 *written_open_drain;  // not a register, but a copy of the most recently written value

    rtapi_u32 output_invert_addr;
    rtapi_u32 *output_invert_reg;
    rtapi_u32 *written_output_invert;  // not a register, but a copy of the most recently written value

    rtapi_u32 clock_frequency;
    rtapi_u8 version;
} hm2_ioport_t;




// 
// stepgen
// 

typedef struct {
    struct {

        struct {
            hal_float_t *position_cmd;
            hal_float_t *velocity_cmd;
            hal_s32_t *counts;
            hal_float_t *position_fb;
            hal_float_t *velocity_fb;
            hal_bit_t *enable;
            hal_bit_t *control_type;  // 0="position control", 1="velocity control"

            // debug pins
            hal_float_t *dbg_ff_vel;
            hal_float_t *dbg_vel_error;
            hal_float_t *dbg_s_to_match;
            hal_float_t *dbg_err_at_match;
            hal_s32_t *dbg_step_rate;
            hal_float_t *dbg_pos_minus_prev_cmd;
        } pin;

        struct {
            hal_float_t position_scale;
            hal_float_t maxvel;
            hal_float_t maxaccel;

            hal_u32_t steplen;
            hal_u32_t stepspace;
            hal_u32_t dirsetup;
            hal_u32_t dirhold;

            hal_u32_t step_type;
            hal_u32_t table[5]; // the Fifth Element is used as a very crude hash
        } param;

    } hal;

    // this variable holds the previous position command, for
    // computing the feedforward velocity
    hal_float_t old_position_cmd;

    rtapi_u32 prev_accumulator;

    // this is a 48.16 signed fixed-point representation of the current
    // stepgen position (16 bits of sub-step resolution)
    rtapi_s64 subcounts;

    rtapi_u32 written_steplen;
    rtapi_u32 written_stepspace;
    rtapi_u32 written_dirsetup;
    rtapi_u32 written_dirhold;
    rtapi_u32 written_step_type;
    rtapi_u32 table_width;
    
} hm2_stepgen_instance_t;


// these hal params affect all stepgen instances
typedef struct {
    struct {
        hal_s32_t *dpll_timer_num;
    } pin;
} hm2_stepgen_module_global_t;

typedef struct {
    int num_instances;
    hm2_stepgen_instance_t *instance;

    rtapi_u32 clock_frequency;
    rtapi_u8 version;

    // module-global HAL objects...
    hm2_stepgen_module_global_t *hal;
    rtapi_u32 written_dpll_timer_num;

    // write this (via TRAM) every hm2_<foo>.write
    rtapi_u32 step_rate_addr;
    rtapi_u32 *step_rate_reg;

    // read this (via TRAM) every hm2_<foo>.read
    rtapi_u32 accumulator_addr;
    rtapi_u32 *accumulator_reg;

    rtapi_u32 mode_addr;
    rtapi_u32 *mode_reg;

    rtapi_u32 dir_setup_time_addr;
    rtapi_u32 *dir_setup_time_reg;

    rtapi_u32 dir_hold_time_addr;
    rtapi_u32 *dir_hold_time_reg;

    rtapi_u32 pulse_width_addr;
    rtapi_u32 *pulse_width_reg;

    rtapi_u32 pulse_idle_width_addr;
    rtapi_u32 *pulse_idle_width_reg;

    // FIXME: these two are not supported yet
    rtapi_u32 table_sequence_data_setup_addr;
    rtapi_u32 table_sequence_length_addr;

    rtapi_u32 master_dds_addr;
    
    rtapi_u32 dpll_timer_num_addr;
} hm2_stepgen_t;

//
// Buffered SPI transciever
// 

typedef struct {
    rtapi_u32 cd[16];
    rtapi_u16 addr[16];
    int conf_flag[16];
    rtapi_u16 cd_addr;
    rtapi_u16 count_addr;
    hal_u32_t *count;
    int num_frames;
    rtapi_u32 clock_freq;
    rtapi_u16 base_address;
    rtapi_u32 register_stride;
    rtapi_u32 instance_stride;
    char name[HAL_NAME_LEN+1];
    int (*read_function)(void*);
    int (*write_function)(void*);
    void *subdata;
} hm2_bspi_instance_t;

typedef struct {
    int version;
    int num_instances;
    hm2_bspi_instance_t *instance;
    rtapi_u8 instances;
    rtapi_u8 num_registers;
} hm2_bspi_t;

//
// UART
// 

typedef struct {
    rtapi_u32 clock_freq;
    rtapi_u32 bitrate;
    rtapi_u32 tx_fifo_count_addr;
    rtapi_u32 tx_fifo_count;
    rtapi_u32 tx_bitrate_addr;
    rtapi_u32 tx1_addr;
    rtapi_u32 tx2_addr;
    rtapi_u32 tx3_addr;
    rtapi_u32 tx4_addr;
    rtapi_u32 tx_mode_addr;
    rtapi_u32 rx_fifo_count_addr;
    rtapi_u32 rx_bitrate_addr;
    rtapi_u32 rx1_addr;
    rtapi_u32 rx2_addr;
    rtapi_u32 rx3_addr;
    rtapi_u32 rx4_addr;
    rtapi_u32 rx_mode_addr;
    char name[HAL_NAME_LEN+1];
} hm2_uart_instance_t;

typedef struct {
    int version;
    int num_instances;
    hm2_uart_instance_t *instance;
    rtapi_u8 instances;
    rtapi_u8 num_registers;
} hm2_uart_t;

//
// PktUART
// 

typedef struct {
    rtapi_u32 clock_freq;
    rtapi_u32 bitrate;
    rtapi_u32 tx_fifo_count_addr;
    rtapi_u32 tx_bitrate_addr;
    rtapi_u32 tx_addr;
    rtapi_u32 tx_mode_addr;
    rtapi_u32 rx_fifo_count_addr;
    rtapi_u32 rx_bitrate_addr;
    rtapi_u32 rx_addr;
    rtapi_u32 rx_mode_addr;
    char name[HAL_NAME_LEN+1];
} hm2_pktuart_instance_t;

typedef struct {
    int version;
    int num_instances;
    hm2_pktuart_instance_t *instance;
    rtapi_u8 instances;
    rtapi_u8 num_registers;
    struct rtapi_heap *heap;
} hm2_pktuart_t;
//
// HM2DPLL
//

typedef struct {
    hal_float_t *time1_us;
    hal_float_t *time2_us;
    hal_float_t *time3_us;
    hal_float_t *time4_us;
    hal_float_t *base_freq;
    hal_float_t *phase_error;
    hal_u32_t *plimit;
    hal_u32_t *ddssize;
    hal_u32_t *time_const;
    hal_u32_t *prescale;
} hm2_dpll_pins_t ;

typedef struct {

    int num_instances ;
    hm2_dpll_pins_t *pins ;

    rtapi_u32 base_rate_addr;
    rtapi_u32 base_rate_written;
    rtapi_u32 phase_err_addr;
    rtapi_u32 control_reg0_addr;
    rtapi_u32 control_reg0_written;
    rtapi_u32 control_reg1_addr;
    rtapi_u32 control_reg1_written;
    rtapi_u32 *control_reg1_read;
    rtapi_u32 timer_12_addr;
    rtapi_u32 timer_12_written;
    rtapi_u32 timer_34_addr;
    rtapi_u32 timer_34_written;
    rtapi_u32 hm2_dpll_sync_addr;
    rtapi_u32 *hm2_dpll_sync_reg;
    rtapi_u32 clock_frequency;

} hm2_dpll_t ;


// 
// watchdog
// 

typedef struct {
    struct {

        struct {
            hal_bit_t *has_bit;
        } pin;

        struct {
            hal_u32_t timeout_ns;
        } param;

    } hal;

    rtapi_u32 written_timeout_ns;

    int enable;  // gets set to 0 at load time, gets set to 1 at first pet_watchdog
    int written_enable;
} hm2_watchdog_instance_t;


typedef struct {
    int num_instances;
    hm2_watchdog_instance_t *instance;

    rtapi_u32 clock_frequency;
    rtapi_u8 version;

    rtapi_u32 timer_addr;
    rtapi_u32 *timer_reg;

    rtapi_u32 status_addr;
    rtapi_u32 *status_reg;

    rtapi_u32 reset_addr;
    rtapi_u32 *reset_reg;
} hm2_watchdog_t;

//
// On-board LEDs
//

typedef struct {
        hal_bit_t *led;
    } hm2_led_instance_t ;

typedef struct {

    int num_instances ;

    hm2_led_instance_t *instance ;

    rtapi_u32 written_buff ;

    rtapi_u32 led_addr;
    rtapi_u32 *led_reg;

} hm2_led_t ;


//
// SSR
//

typedef struct {
    struct {

        struct {
            hal_u32_t *rate;
            hal_bit_t *out[32];
        } pin;

    } hal;

    rtapi_u32 written_data;
    rtapi_u32 written_rate;
} hm2_ssr_instance_t;

typedef struct {
    int num_instances;
    hm2_ssr_instance_t *instance;

    rtapi_u8 version;
    rtapi_u32 clock_freq;

    rtapi_u32 data_addr;
    rtapi_u32 *data_reg;

    rtapi_u32 rate_addr;
    rtapi_u32 *rate_reg;
} hm2_ssr_t;


// 
// raw peek/poke access
//

typedef struct {
    struct {
        struct {
            hal_u32_t *read_address;
            hal_u32_t *read_data;

            hal_u32_t *write_address;
            hal_u32_t *write_data;
            hal_bit_t *write_strobe;

            hal_bit_t *dump_state;
        } pin;
    } hal;
} hm2_raw_t;




// 
// this struct hold an entry in our Translation RAM region list
//

typedef struct {
    rtapi_u16 addr;
    rtapi_u16 size;
    rtapi_u32 **buffer;
    struct rtapi_list_head list;
} hm2_tram_entry_t;




// 
// this struct holds a HostMot2 instance
//

typedef struct {
    hm2_lowlevel_io_t *llio;

    struct {
        int num_encoders;
        int num_absencs;
        struct rtapi_list_head absenc_formats;
        int num_resolvers;
        int num_pwmgens;
        int num_tp_pwmgens;
        int num_stepgens;
        int stepgen_width;
        int num_leds;
        int num_sserials;
        int num_bspis;
        int num_uarts;
        int num_pktuarts;
        int num_dplls;
        int num_ssrs;
        char sserial_modes[4][8];
        int enable_raw;
        char *firmware;
    } config;

    char config_name[HM2_CONFIGNAME_LENGTH + 1];
    rtapi_u16 idrom_offset;

    hm2_idrom_t idrom;

    hm2_module_descriptor_t md[HM2_MAX_MODULE_DESCRIPTORS];
    int num_mds;

    int dpll_module_present;
    int use_serial_numbers;
    
    hm2_pin_t *pin;
    int num_pins;

    // this keeps track of all the tram entries
    struct rtapi_list_head tram_read_entries;
    rtapi_u32 *tram_read_buffer;
    rtapi_u16 tram_read_size;

    struct rtapi_list_head tram_write_entries;
    rtapi_u32 *tram_write_buffer;
    rtapi_u16 tram_write_size;

    // the hostmot2 "Functions"
    hm2_encoder_t encoder;
    hm2_absenc_t absenc;
    hm2_resolver_t resolver;
    hm2_pwmgen_t pwmgen;
    hm2_tp_pwmgen_t tp_pwmgen;
    hm2_stepgen_t stepgen;
    hm2_sserial_t sserial;
    hm2_bspi_t bspi;
    hm2_uart_t uart;
    hm2_pktuart_t pktuart;
    hm2_ioport_t ioport;
    hm2_watchdog_t watchdog;
    hm2_dpll_t dpll;
    hm2_led_t led;
    hm2_ssr_t ssr;

    hm2_raw_t *raw;

    struct rtapi_list_head list;
} hostmot2_t;


//
// misc little helper functions
//

// this one just returns TRUE if the MD is good, FALSE if not
int hm2_md_is_consistent(
    hostmot2_t *hm2,
    int md_index,
    rtapi_u8 version,
    rtapi_u8 num_registers,
    rtapi_u32 instance_stride,
    rtapi_u32 multiple_registers
);

// this one prints a warning message about the unexpected MD,
// *then* returns TRUE if the MD is good, FALSE if not
int hm2_md_is_consistent_or_complain(
    hostmot2_t *hm2,
    int md_index,
    rtapi_u8 version,
    rtapi_u8 num_registers,
    rtapi_u32 instance_stride,
    rtapi_u32 multiple_registers
);

const char *hm2_get_general_function_name(int gtag);

const char *hm2_hz_to_mhz(rtapi_u32 freq_hz);

void hm2_print_modules(hostmot2_t *hm2);

// functions to get handles to components by name
hm2_sserial_remote_t *hm2_get_sserial(hostmot2_t **hm2, char *name);
int hm2_get_bspi(hostmot2_t **hm2, char *name);
int hm2_get_uart(hostmot2_t **hm2, char *name);
int hm2_get_pktuart(hostmot2_t **hm2, char *name);


//
// Translation RAM functions
//

int hm2_register_tram_read_region(hostmot2_t *hm2, rtapi_u16 addr, rtapi_u16 size, rtapi_u32 **buffer);
int hm2_register_tram_write_region(hostmot2_t *hm2, rtapi_u16 addr, rtapi_u16 size, rtapi_u32 **buffer);
int hm2_allocate_tram_regions(hostmot2_t *hm2);
int hm2_tram_read(hostmot2_t *hm2);
int hm2_finish_read(hostmot2_t *hm2);
int hm2_queue_read(hostmot2_t *hm2);
int hm2_tram_write(hostmot2_t *hm2);
int hm2_finish_write(hostmot2_t *hm2);
void hm2_tram_cleanup(hostmot2_t *hm2);




//
// functions for dealing with Pin Descriptors and pins
//

int hm2_read_pin_descriptors(hostmot2_t *hm2);
void hm2_configure_pins(hostmot2_t *hm2);
void hm2_print_pin_usage(hostmot2_t *hm2);
void hm2_set_pin_direction(hostmot2_t *hm2, int pin_number, int direction);  // gpio needs this
void hm2_set_pin_source(hostmot2_t *hm2, int pin_number, int source);




//
// ioport functions
// this includes the gpio stuff exported to hal
//

int hm2_ioport_parse_md(hostmot2_t *hm2, int md_index);
void hm2_ioport_cleanup(hostmot2_t *hm2);
void hm2_ioport_force_write(hostmot2_t *hm2);
void hm2_ioport_write(hostmot2_t *hm2);
void hm2_ioport_print_module(hostmot2_t *hm2);
void hm2_ioport_gpio_tram_write_init(hostmot2_t *hm2);

int hm2_ioport_gpio_export_hal(hostmot2_t *hm2);
void hm2_ioport_gpio_process_tram_read(hostmot2_t *hm2);
void hm2_ioport_gpio_prepare_tram_write(hostmot2_t *hm2);

void hm2_ioport_gpio_read(hostmot2_t *hm2);
void hm2_ioport_gpio_write(hostmot2_t *hm2);




//
// encoder functions
//

int hm2_encoder_parse_md(hostmot2_t *hm2, int md_index);
void hm2_encoder_tram_init(hostmot2_t *hm2);
void hm2_encoder_process_tram_read(hostmot2_t *hm2, long l_period_ns);
void hm2_encoder_write(hostmot2_t *hm2);
void hm2_encoder_cleanup(hostmot2_t *hm2);
void hm2_encoder_print_module(hostmot2_t *hm2);
void hm2_encoder_force_write(hostmot2_t *hm2);


//
// absolute encoder functions
//


int hm2_absenc_parse_md(hostmot2_t *hm2, int md_index);
int hm2_absenc_register_tram(hostmot2_t *hm2);
void hm2_absenc_process_tram_read(hostmot2_t *hm2, long period);
void hm2_absenc_cleanup(hostmot2_t *hm2);
void hm2_absenc_print_module(hostmot2_t *hm2);
void hm2_absenc_write(hostmot2_t *hm2);

//
// resolver functions
//


int hm2_resolver_parse_md(hostmot2_t *hm2, int md_index);
void hm2_resolver_process_tram_read(hostmot2_t *hm2, long period);
void hm2_resolver_cleanup(hostmot2_t *hm2);
void hm2_resolver_print_module(hostmot2_t *hm2);
void hm2_resolver_write(hostmot2_t *hm2, long period);

//
// pwmgen functions
//

int hm2_pwmgen_parse_md(hostmot2_t *hm2, int md_index);
void hm2_pwmgen_print_module(hostmot2_t *hm2);
void hm2_pwmgen_cleanup(hostmot2_t *hm2);
void hm2_pwmgen_write(hostmot2_t *hm2);
void hm2_pwmgen_force_write(hostmot2_t *hm2);
void hm2_pwmgen_prepare_tram_write(hostmot2_t *hm2);


//
// Three Phase pwmgen functions
//

int  hm2_tp_pwmgen_parse_md(hostmot2_t *hm2, int md_index);
void hm2_tp_pwmgen_print_module(hostmot2_t *hm2);
void hm2_tp_pwmgen_cleanup(hostmot2_t *hm2);
void hm2_tp_pwmgen_write(hostmot2_t *hm2);
void hm2_tp_pwmgen_force_write(hostmot2_t *hm2);
void hm2_tp_pwmgen_prepare_tram_write(hostmot2_t *hm2);
void hm2_tp_pwmgen_queue_read(hostmot2_t *hm2);
void hm2_tp_pwmgen_process_read(hostmot2_t *hm2);


//
// stepgen functions
//

int hm2_stepgen_parse_md(hostmot2_t *hm2, int md_index);
void hm2_stepgen_print_module(hostmot2_t *hm2);
void hm2_stepgen_force_write(hostmot2_t *hm2);
void hm2_stepgen_write(hostmot2_t *hm2);
void hm2_stepgen_tram_init(hostmot2_t *hm2);
void hm2_stepgen_prepare_tram_write(hostmot2_t *hm2, long period);
void hm2_stepgen_process_tram_read(hostmot2_t *hm2, long period);
void hm2_stepgen_allocate_pins(hostmot2_t *hm2);

//
// Smart Serial functions
//

int  hm2_sserial_parse_md(hostmot2_t *hm2, int md_index);
void hm2_sserial_print_module(hostmot2_t *hm2);
void hm2_sserial_force_write(hostmot2_t *hm2);
void hm2_sserial_prepare_tram_write(hostmot2_t *hm2, long period);
int hm2_sserial_read_pins(hm2_sserial_remote_t *chan);
void hm2_sserial_process_tram_read(hostmot2_t *hm2, long period);
void hm2_sserial_cleanup(hostmot2_t *hm2);
int hm2_sserial_waitfor(hostmot2_t *hm2, rtapi_u32 addr, rtapi_u32 mask, int ms);
int hm2_sserial_check_local_errors(hostmot2_t *hm2, hm2_sserial_instance_t *inst);
int hm2_sserial_check_remote_errors(hostmot2_t *hm2, hm2_sserial_instance_t *inst);
int hm2_sserial_setup_channel(hostmot2_t *hm2, hm2_sserial_instance_t *inst, int index);
int hm2_sserial_setup_remotes(hostmot2_t *hm2, hm2_sserial_instance_t *inst, hm2_module_descriptor_t *md);
void hm2_sserial_setmode(hostmot2_t *hm2, hm2_sserial_instance_t *inst);
int hm2_sserial_create_pins(hostmot2_t *hm2, hm2_sserial_remote_t *chan);
int hm2_sserial_register_tram(hostmot2_t *hm2, hm2_sserial_remote_t *chan);
int hm2_sserial_read_configs(hostmot2_t *hm2, hm2_sserial_remote_t *chan);

//
// Buffered SPI functions
//

int  hm2_bspi_parse_md(hostmot2_t *hm2, int md_index);
void hm2_bspi_print_module(hostmot2_t *hm2);
void hm2_bspi_cleanup(hostmot2_t *hm2);
void hm2_bspi_write(hostmot2_t *hm2);
void hm2_bspi_force_write(hostmot2_t *hm2);
void hm2_bspi_prepare_tram_write(hostmot2_t *hm2, long period);
void hm2_bspi_process_tram_read(hostmot2_t *hm2, long period);
int hm2_allocate_bspi_tram(char* name);
int hm2_bspi_write_chan(char* name, int chan, rtapi_u32 val);
int hm2_allocate_bspi_tram(char* name);
int hm2_tram_add_bspi_frame(char *name, int chan, rtapi_u32 **wbuff, rtapi_u32 **rbuff);
int hm2_bspi_setup_chan(char *name, int chan, int cs, int bits, float mhz, 
                        int delay, int cpol, int cpha, int noclear, int noecho,
                        int samplelate);
int hm2_bspi_set_read_function(char *name, int (*func)(void *subdata), void *subdata);
int hm2_bspi_set_write_function(char *name, int (*func)(void *subdata), void *subdata);

//
// UART functions
//

int  hm2_uart_parse_md(hostmot2_t *hm2, int md_index);
void hm2_uart_print_module(hostmot2_t *hm2);
void hm2_uart_cleanup(hostmot2_t *hm2);
void hm2_uart_write(hostmot2_t *hm2);
void hm2_uart_force_write(hostmot2_t *hm2);
void hm2_uart_prepare_tram_write(hostmot2_t *hm2, long period);
void hm2_uart_process_tram_read(hostmot2_t *hm2, long period);
int hm2_uart_setup(char *name, int bitrate, rtapi_s32 tx_mode, rtapi_s32 rx_mode);
int hm2_uart_send(char *name, unsigned char data[], int count);
int hm2_uart_read(char *name, unsigned char data[]);
//
// PktUART functions
//

int  hm2_pktuart_parse_md(hostmot2_t *hm2, int md_index);
void hm2_pktuart_print_module(hostmot2_t *hm2);
void hm2_pktuart_cleanup(hostmot2_t *hm2);
void hm2_pktuart_write(hostmot2_t *hm2);
void hm2_pktuart_force_write(hostmot2_t *hm2); // ?? 
void hm2_pktuart_prepare_tram_write(hostmot2_t *hm2, long period); //??
void hm2_pktuart_process_tram_read(hostmot2_t *hm2, long period);  //  ??
int hm2_pktuart_setup(char *name, int bitrate, rtapi_s32 tx_mode, rtapi_s32 rx_mode, int txclear, int rxclear);
int hm2_pktuart_send(char *name,  unsigned char data[], rtapi_u8 *num_frames, rtapi_u16 frame_sizes[]);
int hm2_pktuart_read(char *name, unsigned char data[],  rtapi_u8 *num_frames, rtapi_u16 *max_frame_length, rtapi_u16 frame_sizes[]);

//
// hm2dpll functions
//

void hm2_dpl_cleanup(hostmot2_t *hm2);
int hm2_dpll_parse_md(hostmot2_t *hm2, int md_index);
void hm2_dpll_process_tram_read(hostmot2_t *hm2, long period);
void hm2_dpll_write(hostmot2_t *hm2, long period);

// 
// watchdog functions
// 

int hm2_watchdog_parse_md(hostmot2_t *hm2, int md_index);
void hm2_watchdog_print_module(hostmot2_t *hm2);
void hm2_watchdog_cleanup(hostmot2_t *hm2);
void hm2_watchdog_prepare_tram_write(hostmot2_t *hm2);
void hm2_watchdog_write(hostmot2_t *hm2, long period);
void hm2_watchdog_force_write(hostmot2_t *hm2);
void hm2_watchdog_process_tram_read(hostmot2_t *hm2);




// 
// LED functions
//

int hm2_led_parse_md(hostmot2_t *hm2, int md_index);
void hm2_led_write(hostmot2_t *hm2);
void hm2_led_cleanup(hostmot2_t *hm2);


//
// SSR functions
//

int hm2_ssr_parse_md(hostmot2_t *hm2, int md_index);
void hm2_ssr_cleanup(hostmot2_t *hm2);
void hm2_ssr_write(hostmot2_t *hm2);
void hm2_ssr_force_write(hostmot2_t *hm2);
void hm2_ssr_prepare_tram_write(hostmot2_t *hm2);
void hm2_ssr_print_module(hostmot2_t *hm2);


//
// the raw interface lets you peek and poke the hostmot2 instance from HAL
//

int hm2_raw_setup(hostmot2_t *hm2);
void hm2_raw_queue_read(hostmot2_t *hm2);
void hm2_raw_write(hostmot2_t *hm2);




// write all settings out to the FPGA
// used by hm2_register() to initialize and by hm2_pet_watchdog() to recover from io errors and watchdog errors
void hm2_force_write(hostmot2_t *hm2);


// items related to pin naming
const char *hm2_get_general_function_hal_name(int gtag);
const char *hm2_get_pin_secondary_hal_name(const hm2_pin_t *pin);
#endif
