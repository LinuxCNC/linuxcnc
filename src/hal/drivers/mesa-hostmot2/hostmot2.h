
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

#ifndef RTAPI
#error This is a realtime component only!
#endif


#include "rtapi.h"
#include "hal.h"

#include "hostmot2-lowlevel.h"


#define HM2_VERSION "0.1"
#define HM2_NAME    "hm2"

#define PRINT_NO_LL(level, fmt, args...)  rtapi_print_msg(level, HM2_NAME ": " fmt, ## args);

#define PRINT(level, fmt, args...)  rtapi_print_msg(level,          HM2_NAME "/%s: " fmt, hm2->llio->name, ## args);
#define ERR(fmt, args...)           rtapi_print_msg(RTAPI_MSG_ERR,  HM2_NAME "/%s: " fmt, hm2->llio->name, ## args);
#define WARN(fmt, args...)          rtapi_print_msg(RTAPI_MSG_WARN, HM2_NAME "/%s: " fmt, hm2->llio->name, ## args);
#define INFO(fmt, args...)          rtapi_print_msg(RTAPI_MSG_INFO, HM2_NAME "/%s: " fmt, hm2->llio->name, ## args);
#define DBG(fmt, args...)           rtapi_print_msg(RTAPI_MSG_DBG,  HM2_NAME "/%s: " fmt, hm2->llio->name, ## args);




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
#define HM2_MAX_PIN_DESCRIPTORS    (128)


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

    // these are how the driver keeps track of them
    int gtag;  // the actual function using this pin

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

typedef struct {

    struct {

        struct {
            hal_s32_t *count;
            hal_float_t *position;
            // hal_float_t *velocity;
            // hal_bit_t *reset;
            // hal_bit_t *index_enable;
        } pin;

        struct {
            hal_float_t scale;
            // hal_float_t max_index_vel;
            // hal_float_t velocity_resolution;
        } param;

    } hal;

    u32 prev_counter;

} hm2_encoder_instance_t;


typedef struct {
    int num_instances;

    hm2_encoder_instance_t *instance;

    u32 clock_frequency;
    u8 version;

    // hw registers
    u32 counter_addr;
    u32 *counter_reg;

    u32 latch_control_addr;
    u32 *control_reg;

    // these regs are set at init-time and then ignored, so we dont keep
    // track of their values (though we do note their addresses)
    u32 timestamp_div_addr;
    u32 timestamp_count_addr;
    u32 filter_rate_addr;
} hm2_encoder_t;




//
// pwmgen
// 

typedef struct {

    struct {

        struct {
            hal_float_t *value;
            hal_bit_t *enable;
        } pin;

        struct {
            hal_float_t scale;
            hal_s32_t output_type;
            int32_t written_output_type;
        } param;

    } hal;

    // these make up the fields of the PWM Mode Register, but they don't appear in the HAL
    // (hal.output_type affects a field here too)
    int pwm_width_select;
    int pwm_mode_select;
    int pwm_double_buffered;
} hm2_pwmgen_instance_t;


typedef struct {
    int num_instances;
    hm2_pwmgen_instance_t *instance;

    u32 clock_frequency;
    u8 version;

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
    u32 *written_ddr;  // FIXME: not a register, but a copy of the most recently written ddr

    u32 alt_source_addr;
    u32 *alt_source_reg;

    u32 open_drain_addr;
    u32 *open_drain_reg;

    u32 output_invert_addr;
    u32 *output_invert_reg;

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
            hal_s32_t *counts;
            hal_float_t *position_fb;
            hal_float_t *velocity_fb;

            // these are just for debugging for now, i'll remove them later
            hal_u32_t *rate;
            hal_float_t *error;
        } pin;

        struct {
            hal_float_t position_scale;

            hal_float_t steplen;
            hal_float_t stepspace;
            hal_float_t dirsetup;
            hal_float_t dirhold;
        } param;

    } hal;

    u32 prev_accumulator;

    float written_steplen;
    float written_stepspace;
    float written_dirsetup;
    float written_dirhold;
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

    // FIXME: these are not supported yet
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
    } config;

    char config_name[HM2_CONFIGNAME_LENGTH + 1];
    u16 idrom_offset;

    hm2_idrom_t idrom;

    hm2_module_descriptor_t md[HM2_MAX_MODULE_DESCRIPTORS];
    int num_mds;

    hm2_pin_t pin[HM2_MAX_PIN_DESCRIPTORS];
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

    struct list_head list;
} hostmot2_t;




//
// misc little helper functions
//

int hm2_md_is_consistent(
    hostmot2_t *hm2,
    int md_index,
    u8 version,
    u8 num_registers,
    u32 instance_stride,
    u32 multiple_registers
);

const char *hm2_get_general_function_name(int gtag);

const char *hm2_hz_to_mhz(u32 freq_hz);




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
void hm2_print_pin_usage(int msg_level, hostmot2_t *hm2);
void hm2_set_pin_direction(hostmot2_t *hm2, int pin_number, int direction);  // gpio needs this




//
// ioport functions
// this includes the gpio stuff exported to hal
//

int hm2_ioport_parse_md(hostmot2_t *hm2, int md_index);
void hm2_ioport_cleanup(hostmot2_t *hm2);
void hm2_ioport_force_write(hostmot2_t *hm2);
void hm2_ioport_write(hostmot2_t *hm2);
void hm2_ioport_print_module(int msg_level, hostmot2_t *hm2);

int hm2_ioport_gpio_export_hal(hostmot2_t *hm2);
void hm2_ioport_gpio_process_tram_read(hostmot2_t *hm2);
void hm2_ioport_gpio_prepare_tram_write(hostmot2_t *hm2);




//
// encoder functions
//

int hm2_encoder_parse_md(hostmot2_t *hm2, int md_index);
void hm2_encoder_tram_init(hostmot2_t *hm2);
void hm2_encoder_process_tram_read(hostmot2_t *hm2);
void hm2_encoder_cleanup(hostmot2_t *hm2);
void hm2_encoder_print_module(int msg_level, hostmot2_t *hm2);
void hm2_encoder_force_write(hostmot2_t *hm2);




//
// pwmgen functions
//

int hm2_pwmgen_parse_md(hostmot2_t *hm2, int md_index);
void hm2_pwmgen_print_module(int msg_level, hostmot2_t *hm2);
void hm2_pwmgen_cleanup(hostmot2_t *hm2);
void hm2_pwmgen_write(hostmot2_t *hm2);
void hm2_pwmgen_force_write(hostmot2_t *hm2);
void hm2_pwmgen_prepare_tram_write(hostmot2_t *hm2);




//
// stepgen functions
//

int hm2_stepgen_parse_md(hostmot2_t *hm2, int md_index);
void hm2_stepgen_print_module(int msg_level, hostmot2_t *hm2);
void hm2_stepgen_force_write(hostmot2_t *hm2);
void hm2_stepgen_write(hostmot2_t *hm2);
void hm2_stepgen_tram_init(hostmot2_t *hm2);
void hm2_stepgen_prepare_tram_write(hostmot2_t *hm2, long period);
void hm2_stepgen_process_tram_read(hostmot2_t *hm2, long period);




// 
// watchdog functions

int hm2_watchdog_parse_md(hostmot2_t *hm2, int md_index);
void hm2_watchdog_print_module(int msg_level, hostmot2_t *hm2);
void hm2_watchdog_cleanup(hostmot2_t *hm2);
void hm2_watchdog_write(hostmot2_t *hm2);
void hm2_watchdog_force_write(hostmot2_t *hm2);




// write all settings out to the FPGA
// used by hm2_register() to initialize and by hm2_pet_watchdog() to recover from io errors and watchdog errors
void hm2_force_write(hostmot2_t *hm2);

