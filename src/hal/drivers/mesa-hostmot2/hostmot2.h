
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

// please God where do these live in real life?
#define INT32_MIN (-2147483647-1)
#define INT32_MAX (2147483647)
#define UINT32_MAX (4294967295U)

#include "rtapi.h"
#include "hal.h"

#include "hostmot2-lowlevel.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
void argv_free(char **argv);
char **argv_split(gfp_t gfp, const char *str, int *argcp);
#endif

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

#define HM2_GTAG_WATCHDOG         (2)
#define HM2_GTAG_IOPORT           (3)
#define HM2_GTAG_ENCODER          (4)
#define HM2_GTAG_STEPGEN          (5)
#define HM2_GTAG_PWMGEN           (6)
#define HM2_GTAG_TRANSLATIONRAM  (11)




//
// IDROM and MD structs
//


typedef struct {
    u32 idrom_type;
    u32 offset_to_modules;
    u32 offset_to_pin_desc;
    u8 board_name[8];  // ascii string, but not NULL terminated!
    u32 fpga_size;
    u32 fpga_pins;
    u32 io_ports;
    u32 io_width;
    u32 port_width;
    u32 clock_low;
    u32 clock_high;
    u32 instance_stride_0;
    u32 instance_stride_1;
    u32 register_stride_0;
    u32 register_stride_1;
} hm2_idrom_t;


typedef struct {
    u8 gtag;
    u8 version;
    u8 clock_tag;
    u32 clock_freq;  // this one's not in the MD struct in the device, it's set from clock_tag and the idrom header for our convenience
    u8 instances;
    u16 base_address;

    u8 num_registers;
    u32 register_stride;
    u32 instance_stride;
    u32 multiple_registers;
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
    u8 sec_pin;
    u8 sec_tag;
    u8 sec_unit;
    u8 primary_tag;


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
    s32 written_output_type;

    // this keeps track of the enable bit for this instance that we've told
    // the FPGA, so we know if we need to update it
    s32 written_enable;
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

    u32 clock_frequency;
    u8 version;


    // module-global HAL objects...
    hm2_pwmgen_module_global_t *hal;

    // these keep track of the most recent hal->param.p{d,w}m_frequency
    // that we've told the FPGA about, so we know if we need to update it
    u32 written_pwm_frequency;
    u32 written_pdm_frequency;

    // number of bits of resolution of the PWM signal (PDM is fixed at 12 bits)
    int pwm_bits;


    u32 pwm_value_addr;
    u32 *pwm_value_reg;

    u32 pwm_mode_addr;
    u32 *pwm_mode_reg;

    u32 pwmgen_master_rate_dds_addr;
    u32 pwmgen_master_rate_dds_reg;  // one register for the whole Function

    u32 pdmgen_master_rate_dds_addr;
    u32 pdmgen_master_rate_dds_reg;  // one register for the whole Function

    u32 enable_addr;
    u32 enable_reg;  // one register for the whole Function
} hm2_pwmgen_t;




// 
// ioport
// 

typedef struct {
    int num_instances;

    // register buffers
    // NOTE: there is just one data register for both reading and writing,
    // but the hostmot2 driver's TRAM support can't deal with that so we
    // need two copies...
    u32 data_addr;
    u32 *data_read_reg;
    u32 *data_write_reg;

    u32 ddr_addr;
    u32 *ddr_reg;
    u32 *written_ddr;  // not a register, but a copy of the most recently written value

    u32 alt_source_addr;
    u32 *alt_source_reg;

    u32 open_drain_addr;
    u32 *open_drain_reg;
    u32 *written_open_drain;  // not a register, but a copy of the most recently written value

    u32 output_invert_addr;
    u32 *output_invert_reg;
    u32 *written_output_invert;  // not a register, but a copy of the most recently written value

    u32 clock_frequency;
    u8 version;
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
        } param;

    } hal;

    // this variable holds the previous position command, for
    // computing the feedforward velocity
    hal_float_t old_position_cmd;

    u32 prev_accumulator;

    // this is a 48.16 signed fixed-point representation of the current
    // stepgen position (16 bits of sub-step resolution)
    s64 subcounts;

    u32 written_steplen;
    u32 written_stepspace;
    u32 written_dirsetup;
    u32 written_dirhold;

    u32 written_step_type;
} hm2_stepgen_instance_t;


typedef struct {
    int num_instances;
    hm2_stepgen_instance_t *instance;

    u32 clock_frequency;
    u8 version;

    // write this (via TRAM) every hm2_<foo>.write
    u32 step_rate_addr;
    u32 *step_rate_reg;

    // read this (via TRAM) every hm2_<foo>.read
    u32 accumulator_addr;
    u32 *accumulator_reg;

    u32 mode_addr;
    u32 *mode_reg;

    u32 dir_setup_time_addr;
    u32 *dir_setup_time_reg;

    u32 dir_hold_time_addr;
    u32 *dir_hold_time_reg;

    u32 pulse_width_addr;
    u32 *pulse_width_reg;

    u32 pulse_idle_width_addr;
    u32 *pulse_idle_width_reg;

    // FIXME: these two are not supported yet
    u32 table_sequence_data_setup_addr;
    u32 table_sequence_length_addr;

    u32 master_dds_addr;
} hm2_stepgen_t;




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

    u32 written_timeout_ns;
} hm2_watchdog_instance_t;


typedef struct {
    int num_instances;
    hm2_watchdog_instance_t *instance;

    u32 clock_frequency;
    u8 version;

    u32 timer_addr;
    u32 *timer_reg;

    u32 status_addr;
    u32 *status_reg;

    u32 reset_addr;
    u32 *reset_reg;
} hm2_watchdog_t;




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
    u16 addr;
    u16 size;
    u32 **buffer;
    struct list_head list;
} hm2_tram_entry_t;




// 
// this struct holds a HostMot2 instance
//

typedef struct {
    hm2_lowlevel_io_t *llio;

    struct {
        int num_encoders;
        int num_pwmgens;
        int num_stepgens;
        int enable_raw;
        char *firmware;
    } config;

    char config_name[HM2_CONFIGNAME_LENGTH + 1];
    u16 idrom_offset;

    hm2_idrom_t idrom;

    hm2_module_descriptor_t md[HM2_MAX_MODULE_DESCRIPTORS];
    int num_mds;

    hm2_pin_t *pin;
    int num_pins;

    // this keeps track of all the tram entries
    struct list_head tram_read_entries;
    u32 *tram_read_buffer;
    u16 tram_read_size;

    struct list_head tram_write_entries;
    u32 *tram_write_buffer;
    u16 tram_write_size;

    // the hostmot2 "Functions"
    hm2_encoder_t encoder;
    hm2_pwmgen_t pwmgen;
    hm2_stepgen_t stepgen;
    hm2_ioport_t ioport;
    hm2_watchdog_t watchdog;

    hm2_raw_t *raw;

    struct list_head list;
} hostmot2_t;




//
// misc little helper functions
//

// this one just returns TRUE if the MD is good, FALSE if not
int hm2_md_is_consistent(
    hostmot2_t *hm2,
    int md_index,
    u8 version,
    u8 num_registers,
    u32 instance_stride,
    u32 multiple_registers
);

// this one prints a warning message about the unexpected MD,
// *then* returns TRUE if the MD is good, FALSE if not
int hm2_md_is_consistent_or_complain(
    hostmot2_t *hm2,
    int md_index,
    u8 version,
    u8 num_registers,
    u32 instance_stride,
    u32 multiple_registers
);

const char *hm2_get_general_function_name(int gtag);

const char *hm2_hz_to_mhz(u32 freq_hz);

void hm2_print_modules(hostmot2_t *hm2);




//
// Translation RAM functions
//

int hm2_register_tram_read_region(hostmot2_t *hm2, u16 addr, u16 size, u32 **buffer);
int hm2_register_tram_write_region(hostmot2_t *hm2, u16 addr, u16 size, u32 **buffer);
int hm2_allocate_tram_regions(hostmot2_t *hm2);
int hm2_tram_read(hostmot2_t *hm2);
int hm2_tram_write(hostmot2_t *hm2);
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
// pwmgen functions
//

int hm2_pwmgen_parse_md(hostmot2_t *hm2, int md_index);
void hm2_pwmgen_print_module(hostmot2_t *hm2);
void hm2_pwmgen_cleanup(hostmot2_t *hm2);
void hm2_pwmgen_write(hostmot2_t *hm2);
void hm2_pwmgen_force_write(hostmot2_t *hm2);
void hm2_pwmgen_prepare_tram_write(hostmot2_t *hm2);




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
// watchdog functions
// 

int hm2_watchdog_parse_md(hostmot2_t *hm2, int md_index);
void hm2_watchdog_print_module(hostmot2_t *hm2);
void hm2_watchdog_cleanup(hostmot2_t *hm2);
void hm2_watchdog_write(hostmot2_t *hm2);
void hm2_watchdog_force_write(hostmot2_t *hm2);




// 
// the raw interface lets you peek and poke the hostmot2 instance from HAL
//

int hm2_raw_setup(hostmot2_t *hm2);
void hm2_raw_read(hostmot2_t *hm2);
void hm2_raw_write(hostmot2_t *hm2);




// write all settings out to the FPGA
// used by hm2_register() to initialize and by hm2_pet_watchdog() to recover from io errors and watchdog errors
void hm2_force_write(hostmot2_t *hm2);

