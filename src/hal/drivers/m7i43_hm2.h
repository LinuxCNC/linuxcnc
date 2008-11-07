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

#ifndef RTAPI
#error This is a realtime component only!
#endif


#define M7I43_HM2_VERSION "0.4"

#define M7I43_HM2_ID "m7i43_hm2: "
#define  PRINT(level, fmt, args...)  rtapi_print_msg(level, M7I43_HM2_ID fmt, ## args);
#define  DEBUG(enable, fmt, args...)  if (enable) { rtapi_print_msg(RTAPI_MSG_DBG, fmt, ## args); }

static void m7i43_nanosleep(unsigned long int nanoseconds);


// 
// EPP stuff
// 

#define M7I43_EPP_STATUS_OFFSET   (1)
#define M7I43_EPP_CONTROL_OFFSET  (2)
#define M7I43_EPP_ADDRESS_OFFSET  (3)
#define M7I43_EPP_DATA_OFFSET     (4)

#define M7I43_ECP_CONFIG_A_HIGH_OFFSET  (0)
#define M7I43_ECP_CONFIG_B_HIGH_OFFSET  (1)
#define M7I43_ECP_CONTROL_HIGH_OFFSET   (2)


// 
// Mesa 7i43 HostMot2 stuff
// 

#define M7I43_HM2_ADDR_AUTOINCREMENT (0x8000)

#define M7I43_HM2_ADDR_IOCOOKIE  (0x0100)
#define M7I43_HM2_IOCOOKIE       (0x55AACAFE)

#define M7I43_HM2_ADDR_CONFIGNAME    (0x0104)
#define M7I43_HM2_CONFIGNAME         "HOSTMOT2"
#define M7I43_HM2_CONFIGNAME_LENGTH  (8)

#define M7I43_HM2_ADDR_IDROM_OFFSET (0x010C)

#define M7I43_HM2_MAX_MODULE_DESCRIPTORS  (48)
#define M7I43_HM2_MAX_PIN_DESCRIPTORS    (128)

#define HM2_PIN_SOURCE_IS_PRIMARY   (0x00000000)
#define HM2_PIN_SOURCE_IS_SECONDARY (0x00000001)

#define HM2_PIN_DIR_IS_INPUT     (0x00000002)
#define HM2_PIN_DIR_IS_OUTPUT    (0x00000004)


#define HM2_GTAG_WATCHDOG        (2)
#define HM2_GTAG_IOPORT          (3)
#define HM2_GTAG_ENCODER         (4)
#define HM2_GTAG_STEPGEN         (5)
#define HM2_GTAG_PWMGEN          (6)
#define HM2_GTAG_TRANSLATIONRAM (11)




//
// HostMot2 IDROM structs
//


typedef struct {
    __u8 gtag;
    __u8 version;
    __u8 clock_tag;
    __u8 instances;
    __u16 base_address;

    __u8 num_registers;
    __u32 register_stride;
    __u32 instance_stride;
    __u32 multiple_registers;
} hm2_module_descriptor_t;


typedef struct {
    __u32 idrom_type;
    __u32 offset_to_modules;
    __u32 offset_to_pin_desc;
    __u8 board_name[9];  // we just read 8 bytes from the IDRom, but then we add a NULL
    __u32 board_name_low;
    __u32 board_name_high;
    __u32 fpga_size;
    __u32 fpga_pins;
    __u32 io_ports;
    __u32 io_width;
    __u32 port_width;
    __u32 clock_low;
    __u32 clock_high;
    __u32 register_stride_0;
    __u32 register_stride_1;
    __u32 instance_stride_0;
    __u32 instance_stride_1;
} hm2_idrom_t;




// 
// these structures keep track of the FPGA's I/O pins; and for I/O pins
// used as GPIOs, keep track of the HAL state of the pins
//

typedef struct {
    struct {
        hal_bit_t *in;
        hal_bit_t *in_not;
        hal_bit_t *out;
    } pin;

    struct {
        hal_bit_t is_output;  // hm2 calls this "ddr"
        hal_bit_t invert_output;
    } param;
} hal_gpio_t;


typedef struct {
    // these are from the Pin Descriptor in the HM2 IDROM
    __u8 sec_pin;
    __u8 sec_tag;
    __u8 sec_unit;
    __u8 primary_tag;

    // these are how the driver keeps track of them
    int gtag;  // the actual function using this pin

    // if the driver decides to make this pin a gpio, it'll allocate the hal struct to manage it
    // otherwise hal is NULL
    hal_gpio_t *hal;
} hm2_pin_t;




//
// these structures translate between HostMot2 Functions and HAL objects
//


typedef struct {
    struct {
        __u16 count;
        __u16 prev_count;

        __u16 timestamp;
        __u16 prev_timestamp;

        __u16 control;
    } hw;

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
} hm2_encoder_instance_t;


typedef struct {
    int num_instances;
    hm2_encoder_instance_t *instance;

    __u32 clock_frequency;
    __u8 version;

    __u32 counter_addr;
    __u32 latch_control_addr;
    __u32 timestamp_div_addr;
    __u32 timestamp_count_addr;
    __u32 filter_rate_addr;
} hm2_encoder_t;




typedef struct {
    struct {
        __u32 pwm_val;
        __u32 pwm_mode;
    } hw;

    struct {

        struct {
            hal_float_t *value;
            double written_value;

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

    __u32 clock_frequency;
    __u8 version;

    __u32 pwmgen_master_rate_dds;
    __u32 pdmgen_master_rate_dds;

    // value currently in the enable register
    // we only re-write this register when it changes
    __u32 enable;

    __u32 pwm_value_addr;
    __u32 pwm_mode_addr;
    __u32 pwmgen_master_rate_dds_addr;
    __u32 pdmgen_master_rate_dds_addr;
    __u32 enable_addr;
} hm2_pwmgen_t;




typedef struct {
    __u32 data;
    __u32 ddr;
    __u32 alt_source;
    __u32 open_drain;
    __u32 output_invert;

    __u32 written_ddr;
} hm2_ioport_instance_t;


typedef struct {
    int num_instances;
    hm2_ioport_instance_t *instance;

    __u32 clock_frequency;
    __u8 version;

    __u32 data_addr;
    __u32 ddr_addr;
    __u32 alt_source_addr;
    __u32 open_drain_addr;
    __u32 output_invert_addr;
} hm2_ioport_t;




typedef struct {
    struct {
        __u32 rate;
        __u32 accumulator;
        __u32 mode;
        __u32 dir_setup_time;
        __u32 dir_hold_time;
        __u32 pulse_width;
        __u32 pulse_idle_width;
        __u32 table_sequence_data_setup;
        __u32 table_sequence_length;

        __u32 prev_accumulator;
    } hw;

    struct {

        struct {
            hal_float_t *position_cmd;

            hal_s32_t *counts;
            s32 prev_counts;

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

            double written_steplen;
            double written_stepspace;
            double written_dirsetup;
            double written_dirhold;
        } param;

    } hal;
} hm2_stepgen_instance_t;


typedef struct {
    int num_instances;
    hm2_stepgen_instance_t *instance;

    __u32 clock_frequency;
    __u8 version;

    // stepgen has lots of registers
    __u32 step_rate_addr;
    __u32 accumulator_addr;
    __u32 mode_addr;
    __u32 dir_setup_time_addr;
    __u32 dir_hold_time_addr;
    __u32 pulse_width_addr;
    __u32 pulse_idle_width_addr;
    __u32 table_sequence_data_setup_addr;
    __u32 table_sequence_length_addr;
    __u32 master_dds_addr;
} hm2_stepgen_t;




typedef struct {
    struct {
        __u32 timer;
    } hw;

    struct {

        struct {
            hal_bit_t *has_bit;
        } pin;

    } hal;

    __u32 timeout_ns;
} hm2_watchdog_instance_t;


typedef struct {
    int num_instances;
    hm2_watchdog_instance_t *instance;

    __u32 clock_frequency;
    __u8 version;

    __u32 timer_addr;
    __u32 status_addr;
    __u32 reset_addr;
} hm2_watchdog_t;




//
// Mesa 7i43 structs
//


typedef struct {
    int fpga_size;
} cpld_t;


typedef struct {
    hal_u32_t *epp_errors;

    cpld_t cpld;

    char config_name[M7I43_HM2_CONFIGNAME_LENGTH + 1];
    __u16 idrom_offset;

    hm2_idrom_t idrom;

    hm2_module_descriptor_t md[M7I43_HM2_MAX_MODULE_DESCRIPTORS];
    int num_mds;

    hm2_pin_t pin[M7I43_HM2_MAX_PIN_DESCRIPTORS];
    int num_pins;

    // the hostmot2 "Functions"
    hm2_encoder_t encoder;
    hm2_pwmgen_t pwmgen;
    hm2_stepgen_t stepgen;
    hm2_ioport_t ioport;
    hm2_watchdog_t watchdog;
} m7i43_t;

