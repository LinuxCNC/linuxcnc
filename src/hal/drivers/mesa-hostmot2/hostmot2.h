
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

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include "gomc_env.h"
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

#define HM2_PRINT_NO_LL(fmt, args...)  gomc_log_infof(hm2_log, HM2_NAME, fmt, ## args)

#define HM2_ERR_NO_LL(fmt, args...)    gomc_log_errorf(hm2_log, HM2_NAME, fmt, ## args)
#define HM2_WARN_NO_LL(fmt, args...)   gomc_log_warnf(hm2_log, HM2_NAME, fmt, ## args)
#define HM2_INFO_NO_LL(fmt, args...)   gomc_log_infof(hm2_log, HM2_NAME, fmt, ## args)
#define HM2_DBG_NO_LL(fmt, args...)    gomc_log_debugf(hm2_log, HM2_NAME, fmt, ## args)


#define HM2_PRINT(fmt, args...)  gomc_log_infof(hm2->llio->log, hm2->llio->name, fmt, ## args)

#define HM2_ERR(fmt, args...)    gomc_log_errorf(hm2->llio->log, hm2->llio->name, fmt, ## args)
#define HM2_WARN(fmt, args...)   gomc_log_warnf(hm2->llio->log, hm2->llio->name, fmt, ## args)
#define HM2_INFO(fmt, args...)   gomc_log_infof(hm2->llio->log, hm2->llio->name, fmt, ## args)
#define HM2_DBG(fmt, args...)    gomc_log_debugf(hm2->llio->log, hm2->llio->name, fmt, ## args)




// 
// idrom addresses & constants
// 

#define HM2_ADDR_IOCOOKIE  (0x0100)
#define HM2_IOCOOKIE       (0x55AACAFE)

#define HM2_ADDR_CONFIGNAME    (0x0104)
#define HM2_CONFIGNAME         "HOSTMOT2"
#define HM2_CONFIGNAME_LENGTH  (8)

#define HM2_ADDR_IDROM_OFFSET (0x010C)

#define HM2_ADDR_WATCHDOG (0x0C00)

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
#define HM2_GTAG_INMUX             (30) 
#define HM2_GTAG_INM               (35) 
#define HM2_GTAG_DPAINTER          (42) 
#define HM2_GTAG_XY2MOD            (43) 
#define HM2_GTAG_RCPWMGEN          (44) 
#define HM2_GTAG_OUTM              (45) 
#define HM2_GTAG_LIOPORT           (64) // Not supported
#define HM2_GTAG_LED               (128)

#define HM2_GTAG_RESOLVER          (192)
#define HM2_GTAG_SMARTSERIAL       (193)
#define HM2_GTAG_TWIDDLER          (194) // Not supported
#define HM2_GTAG_SSR               (195)
#define HM2_GTAG_SMARTSERIALB      (198) // smart-serial with 224 data bits
#define HM2_GTAG_ONESHOT           (199) // One shot


//
// IDROM and MD structs
//


typedef struct {
    uint32_t idrom_type;
    uint32_t offset_to_modules;
    uint32_t offset_to_pin_desc;
    uint8_t board_name[8];  // ascii string, but not NULL terminated!
    uint32_t fpga_size;
    uint32_t fpga_pins;
    uint32_t io_ports;
    uint32_t io_width;
    uint32_t port_width;
    uint32_t clock_low;
    uint32_t clock_high;
    uint32_t instance_stride_0;
    uint32_t instance_stride_1;
    uint32_t register_stride_0;
    uint32_t register_stride_1;
} hm2_idrom_t;


typedef struct {
    uint8_t gtag;
    uint8_t version;
    uint8_t clock_tag;
    uint32_t clock_freq;  // this one's not in the MD struct in the device, it's set from clock_tag and the idrom header for our convenience
    uint8_t instances;
    uint16_t base_address;

    uint8_t num_registers;
    uint32_t register_stride;
    uint32_t instance_stride;
    uint32_t multiple_registers;
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
            gomc_hal_bit_t *in;
            gomc_hal_bit_t *in_not;
            gomc_hal_bit_t *out;
        } pin;

        struct {
            gomc_hal_bit_t is_output;
            gomc_hal_bit_t is_opendrain;
            gomc_hal_bit_t invert_output;
        } param;

    } hal;
} hm2_gpio_instance_t;


typedef struct {
    // these are from the Pin Descriptor in the HM2 IDROM
    uint8_t sec_pin;
    uint8_t sec_tag;
    uint8_t sec_unit;
    uint8_t primary_tag;
    uint8_t port_num;
    uint8_t port_pin;
    uint8_t bit_num;


    //
    // below here is how the driver keeps track of each pin
    //

    // the actual function using this pin
    int gtag;

    // either HM2_PIN_DIR_IS_INPUT or HM2_PIN_DIR_IS_OUTPUT
    // if gtag != gpio, how the owning module instance configured it at load-time
    // if gtag == gpio, this gets copied from the .is_output parameter
    // If the "at start" value differs from the "direction" value, this occurs
    // the first time the write function is executed
    int direction;
    int direction_at_start;

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
            gomc_hal_s32_t *rawcounts;    // raw encoder counts
            gomc_hal_s32_t *rawlatch;     // raw encoder of latch
            gomc_hal_s32_t *count;        // (rawcounts - zero_offset)
            gomc_hal_s32_t *count_latch;  // (rawlatch - zero_offset)
            gomc_hal_float_t *position;
            gomc_hal_float_t *position_latch;
            gomc_hal_float_t *velocity;
            gomc_hal_float_t *velocity_rpm;
            gomc_hal_bit_t *reset;
            gomc_hal_bit_t *index_enable;
            gomc_hal_bit_t *latch_enable;
            gomc_hal_bit_t *latch_polarity;
            gomc_hal_bit_t *quadrature_error;
            gomc_hal_bit_t *quadrature_error_enable;
            gomc_hal_bit_t *input_a;
            gomc_hal_bit_t *input_b;
            gomc_hal_bit_t *input_idx;
        } pin;

        struct {
            gomc_hal_float_t scale;
            gomc_hal_bit_t index_invert;
            gomc_hal_bit_t index_mask;
            gomc_hal_bit_t index_mask_invert;
            gomc_hal_bit_t counter_mode;
            gomc_hal_bit_t filter;
            gomc_hal_float_t vel_timeout;


        } param;

    } hal;

    int32_t zero_offset;  // *hal.pin.counts == (*hal.pin.rawcounts - zero_offset)

    uint16_t prev_reg_count;  // from this and the current count in the register we compute a change-in-counts, which we add to rawcounts

    int32_t prev_dS_counts;  // last time the function ran, it saw this many counts from the time before *that*

    uint32_t prev_control;

    gomc_hal_bit_t prev_quadrature_error_enable; // shadow for detecting rising edge on the quadrature_error_enable
    gomc_hal_bit_t reset_quadrature_error; // bit to indicate if we want to reset the quadrature error


    // these two are the datapoint last time we moved (only valid if state == HM2_ENCODER_MOVING)
    int32_t prev_event_rawcounts;
    uint16_t prev_event_reg_timestamp;

    int32_t tsc_num_rollovers;
    uint16_t prev_time_of_interest;

    enum { HM2_ENCODER_STOPPED, HM2_ENCODER_MOVING } state;

} hm2_encoder_instance_t;


// these hal pins affect all encoder instances
typedef struct {
    struct {
        gomc_hal_u32_t *sample_frequency;
        gomc_hal_u32_t *skew;
        gomc_hal_s32_t *dpll_timer_num;
	gomc_hal_bit_t *hires_timestamp;

    } pin;
} hm2_encoder_module_global_t;

typedef struct {
    int num_instances;
    int firmware_supports_probe;

    hm2_encoder_instance_t *instance;

    uint32_t stride;
    uint32_t clock_frequency;
    uint8_t version;

    // module-global HAL objects...
    hm2_encoder_module_global_t *hal;
    uint32_t written_sample_frequency;
    int has_skew;
    uint32_t written_skew;
    uint32_t written_hires_timestamp;
    uint32_t desired_dpll_timer_reg, written_dpll_timer_reg;

    // hw registers
    uint32_t counter_addr;
    uint32_t *counter_reg;

    uint32_t latch_control_addr;
    uint32_t *control_reg;
    uint32_t *read_control_reg;

    uint32_t timestamp_div_addr;
    uint32_t timestamp_div_reg;  // one register for the whole Function
    gomc_hal_float_t seconds_per_tsdiv_clock;

    uint32_t timestamp_count_addr;
    uint32_t *timestamp_count_reg;
    uint32_t prev_timestamp_count_reg;

    uint32_t filter_rate_addr;

    uint32_t dpll_timer_num_addr;
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

    uint32_t clock_frequency;
    uint8_t ssi_version;
    uint8_t biss_version;
    uint8_t fanuc_version;
    uint32_t ssi_global_start_addr;
    uint32_t fabs_global_start_addr;
    uint32_t biss_global_start_addr;
    uint32_t *biss_busy_flags;
    uint32_t *ssi_busy_flags;
    uint32_t *fabs_busy_flags;
} hm2_absenc_t;

//
// resolver
//

typedef struct {

    struct {

        struct {
            gomc_hal_s32_t *rawcounts;
            gomc_hal_s32_t *count;
            gomc_hal_float_t *angle;
            gomc_hal_float_t *position;
            gomc_hal_float_t *velocity;
            gomc_hal_float_t *velocity_rpm;
            gomc_hal_bit_t *reset;
            gomc_hal_bit_t *index_enable;
            gomc_hal_bit_t *error;
            gomc_hal_float_t *joint_pos_fb;
        } pin;

        struct {
            gomc_hal_float_t scale;
            gomc_hal_float_t vel_scale;
            gomc_hal_u32_t index_div;
            gomc_hal_bit_t use_abs;
        } param;

    } hal;
    
    int64_t accum;
    int64_t offset;
    uint32_t old_reg;
    uint32_t index_cnts;

} hm2_resolver_instance_t;

typedef struct {
    struct {
        gomc_hal_float_t excitation_khz;
    } param;
} hm2_resolver_global_t;

typedef struct {
    int num_instances;
    int num_resolvers;

    hm2_resolver_global_t *hal;
    hm2_resolver_instance_t *instance;

    uint32_t stride;
    uint32_t clock_frequency;
    uint8_t version;

    // hw registers
    uint32_t status_addr;
    uint32_t *status_reg;
    
    uint32_t command_addr;
    
    uint32_t data_addr;
    
    uint32_t position_addr;
    uint32_t *position_reg;

    uint32_t velocity_addr;
    int32_t *velocity_reg;
    
    gomc_hal_float_t written_khz;
    gomc_hal_float_t kHz;
    
} hm2_resolver_t;


//
// pwmgen
// 

#define HM2_PWMGEN_OUTPUT_TYPE_PWM          1  // this is the same value that the software pwmgen component uses
#define HM2_PWMGEN_OUTPUT_TYPE_UP_DOWN      2  // this is the same value that the software pwmgen component uses
#define HM2_PWMGEN_OUTPUT_TYPE_PDM          3  // software pwmgen doesn't support pdm as an output type
#define HM2_PWMGEN_OUTPUT_TYPE_PWM_SWAPPED  4  // software pwmgen doesn't support pwm/swapped output type because it doesn't need to 

typedef struct {

    struct {

        struct {
            gomc_hal_float_t *value;
            gomc_hal_bit_t *enable;
        } pin;

        struct {
            gomc_hal_float_t scale;
            gomc_hal_bit_t offset_mode;
            gomc_hal_s32_t output_type; 
            gomc_hal_bit_t dither;            
        } param;

    } hal;

    // this keeps track of the output_type that we've told the FPGA, so we
    // know if we need to update it
    int32_t written_output_type;

    // this keeps track of the offset_mode that we've set , so we
    // know if we need to update it
    uint32_t written_offset_mode;

    // this keeps track of the enable bit for this instance that we've told
    // the FPGA, so we know if we need to update it
    uint32_t written_enable;
    
    // this keeps track of the dither bit for this instance that we've told
    // the FPGA, so we know if we need to update it
    uint32_t written_dither;
    
} hm2_pwmgen_instance_t;


// these hal params affect all pwmgen instances
typedef struct {
    struct {
        gomc_hal_u32_t pwm_frequency;
        gomc_hal_u32_t pdm_frequency;
    } param;
} hm2_pwmgen_module_global_t;


typedef struct {
    int num_instances;
    hm2_pwmgen_instance_t *instance;

    uint32_t clock_frequency;
    uint8_t version;


    // module-global HAL objects...
    hm2_pwmgen_module_global_t *hal;

    // these keep track of the most recent hal->param.p{d,w}m_frequency
    // that we've told the FPGA about, so we know if we need to update it
    uint32_t written_pwm_frequency;
    uint32_t written_pdm_frequency;

    // number of bits of resolution of the PWM signal (PDM is fixed at 12 bits)
    int pwm_bits;
    int firmware_supports_dither;

    uint32_t pwm_value_addr;
    uint32_t *pwm_value_reg;

    uint32_t pwm_mode_addr;
    uint32_t *pwm_mode_reg;

    uint32_t pwmgen_master_rate_dds_addr;
    uint32_t pwmgen_master_rate_dds_reg;  // one register for the whole Function

    uint32_t pdmgen_master_rate_dds_addr;
    uint32_t pdmgen_master_rate_dds_reg;  // one register for the whole Function

    uint32_t enable_addr;
    uint32_t enable_reg;  // one register for the whole Function
} hm2_pwmgen_t;


//
// oneshot
// 


typedef struct {

    struct {

        struct {
            gomc_hal_float_t *width1;
            gomc_hal_float_t *width2; 
            gomc_hal_float_t *filter1;
            gomc_hal_float_t *filter2;
            gomc_hal_float_t *rate;
            gomc_hal_u32_t *trigselect1;
            gomc_hal_u32_t *trigselect2;
            gomc_hal_bit_t *trigrise1;
            gomc_hal_bit_t *trigrise2;
            gomc_hal_bit_t *trigfall1;
            gomc_hal_bit_t *trigfall2;
            gomc_hal_bit_t *retrig1;
            gomc_hal_bit_t *retrig2;
            gomc_hal_bit_t *enable1;
            gomc_hal_bit_t *enable2;
            gomc_hal_bit_t *reset1;
            gomc_hal_bit_t *reset2;
            gomc_hal_bit_t *swtrig1;
            gomc_hal_bit_t *swtrig2;
            gomc_hal_bit_t *exttrig1;
            gomc_hal_bit_t *exttrig2;
            gomc_hal_bit_t *out1;
            gomc_hal_bit_t *out2;
            
            gomc_hal_s32_t *dpll_timer_num;
        } pin;

    } hal;

} hm2_oneshot_instance_t;



typedef struct {
    int num_instances;
    hm2_oneshot_instance_t *instance;

    uint32_t clock_frequency;
    uint8_t version;

    uint32_t width1_addr;
    uint32_t *width1_reg;

    uint32_t width2_addr;
    uint32_t *width2_reg;

    uint32_t filter1_addr;
    uint32_t *filter1_reg;

    uint32_t filter2_addr;
    uint32_t *filter2_reg;

    uint32_t rate_addr;
    uint32_t *rate_reg;

    uint32_t control_addr;
    uint32_t *control_reg;

    uint32_t control_read_addr;
    uint32_t *control_read_reg;

} hm2_oneshot_t;

//
// rcpwmgen pwmgen optimized for RC servos
// 

typedef struct {

    struct {

        struct {
            gomc_hal_float_t *width;
            gomc_hal_float_t *scale;
            gomc_hal_float_t *offset;
        } pin;

    } hal;
} hm2_rcpwmgen_instance_t;


// this hal pin affects all rcpwmgen instances
typedef struct {
    struct {
        gomc_hal_float_t *rate;
    } pin;
} hm2_rcpwmgen_module_global_t;


typedef struct {
    int num_instances;
    hm2_rcpwmgen_instance_t *instance;

    uint32_t clock_frequency;
    uint8_t version;

    // module-global HAL objects...
    hm2_rcpwmgen_module_global_t *hal;

    uint32_t width_addr;
    uint32_t *width_reg;

    uint32_t rate_addr;
    uint32_t rate_reg;
 
    double written_rate;
    uint32_t error_throttle;

} hm2_rcpwmgen_t;


//
// inmux
// 


typedef struct {

    struct {

        struct {
            gomc_hal_bit_t *filt_data[32];
            gomc_hal_bit_t *raw_data[32];
            gomc_hal_bit_t *filt_data_not[32];
            gomc_hal_bit_t *raw_data_not[32];
            gomc_hal_bit_t *slow[32] ;
            gomc_hal_s32_t *enc0_count; 
            gomc_hal_s32_t *enc1_count; 
            gomc_hal_s32_t *enc2_count; 
            gomc_hal_s32_t *enc3_count;
            gomc_hal_bit_t *enc0_reset; 
            gomc_hal_bit_t *enc1_reset; 
            gomc_hal_bit_t *enc2_reset; 
            gomc_hal_bit_t *enc3_reset; 
             		
        } pin;

        struct {
            gomc_hal_u32_t scan_rate;
            gomc_hal_u32_t slow_scans; 
            gomc_hal_u32_t fast_scans; 		
            gomc_hal_bit_t enc0_mode; 
            gomc_hal_bit_t enc1_mode; 
            gomc_hal_bit_t enc2_mode; 
            gomc_hal_bit_t enc3_mode; 
            gomc_hal_u32_t scan_width; 	    	
        } param;

    } hal;

    //scanwidth for this instance	
    uint32_t scanwidth;	

    //previous MPG counts for this instance	
    int8_t prev_enc0_count;	
    int8_t prev_enc1_count;	
    int8_t prev_enc2_count;	
    int8_t prev_enc3_count;	

    // this keeps track of the control register that we've told the FPGA, so we
    // know if we need to update it
    uint32_t written_control_reg;

    // this keeps track of the filter register that we've told the FPGA, so we
    // know if we need to update it
    uint32_t written_filter_reg;

    // this keeps track of the mpg mode register that we've told the FPGA, so we
    // know if we need to update it
    uint32_t written_mpg_mode_reg;

} hm2_inmux_instance_t;



typedef struct {
    int num_instances;
    hm2_inmux_instance_t *instance;

    uint32_t clock_frequency;
    uint8_t version;


    // module-global HAL objects...

    uint32_t control_addr;
    uint32_t *control_reg;

    uint32_t filter_addr;
    uint32_t *filter_reg;

    uint32_t filt_data_addr;
    uint32_t *filt_data_reg; 

    uint32_t raw_data_addr;
    uint32_t *raw_data_reg; 

    uint32_t mpg_read_addr;
    uint32_t *mpg_read_reg;  

    uint32_t mpg_mode_addr;
    uint32_t *mpg_mode_reg; 

} hm2_inmux_t;


//
// inm   basicall a clone of InMux
// 


typedef struct {

    struct {

        struct {
            gomc_hal_bit_t *filt_data[32];
            gomc_hal_bit_t *raw_data[32];
            gomc_hal_bit_t *filt_data_not[32];
            gomc_hal_bit_t *raw_data_not[32];
            gomc_hal_bit_t *slow[32] ;
            gomc_hal_s32_t *enc0_count; 
            gomc_hal_s32_t *enc1_count; 
            gomc_hal_s32_t *enc2_count; 
            gomc_hal_s32_t *enc3_count; 		
            gomc_hal_bit_t *enc0_reset; 
            gomc_hal_bit_t *enc1_reset; 
            gomc_hal_bit_t *enc2_reset; 
            gomc_hal_bit_t *enc3_reset; 
        } pin;

        struct {
            gomc_hal_u32_t scan_rate;
            gomc_hal_u32_t slow_scans; 
            gomc_hal_u32_t fast_scans; 		
            gomc_hal_bit_t enc0_mode; 
            gomc_hal_bit_t enc1_mode; 
            gomc_hal_bit_t enc2_mode; 
            gomc_hal_bit_t enc3_mode; 
            gomc_hal_u32_t scan_width; 	    	
        } param;

    } hal;

    //scanwidth for this instance	
    uint32_t scanwidth;	

    //previous MPG counts for this instance	
    int8_t prev_enc0_count;	
    int8_t prev_enc1_count;	
    int8_t prev_enc2_count;	
    int8_t prev_enc3_count;	

    // this keeps track of the control register that we've told the FPGA, so we
    // know if we need to update it
    uint32_t written_control_reg;

    // this keeps track of the filter register that we've told the FPGA, so we
    // know if we need to update it
    uint32_t written_filter_reg;

    // this keeps track of the mpg mode register that we've told the FPGA, so we
    // know if we need to update it
    uint32_t written_mpg_mode_reg;

} hm2_inm_instance_t;



typedef struct {
    int num_instances;
    hm2_inm_instance_t *instance;

    uint32_t clock_frequency;
    uint8_t version;


    // module-global HAL objects...

    uint32_t control_addr;
    uint32_t *control_reg;

    uint32_t filter_addr;
    uint32_t *filter_reg;

    uint32_t filt_data_addr;
    uint32_t *filt_data_reg; 

    uint32_t raw_data_addr;
    uint32_t *raw_data_reg; 

    uint32_t mpg_read_addr;
    uint32_t *mpg_read_reg;  

    uint32_t mpg_mode_addr;
    uint32_t *mpg_mode_reg; 

} hm2_inm_t;


//
// xy2mod
// 


typedef struct {

    struct {

        struct {
            gomc_hal_float_t *accx_cmd;
            gomc_hal_float_t *accy_cmd;
            gomc_hal_float_t *velx_cmd;
            gomc_hal_float_t *vely_cmd;
            gomc_hal_float_t *posx_cmd;
            gomc_hal_float_t *posy_cmd;
            gomc_hal_float_t *velx_fb;
            gomc_hal_float_t *vely_fb;
            gomc_hal_float_t *posx_fb;
            gomc_hal_float_t *posy_fb;
            gomc_hal_float_t *posx_scale;
            gomc_hal_float_t *posy_scale;
            gomc_hal_bit_t 	*enable;
            gomc_hal_u32_t 	*controlx;
            gomc_hal_u32_t 	*controly;
            gomc_hal_u32_t 	*commandx;
            gomc_hal_u32_t 	*commandy;
            gomc_hal_bit_t 	*mode18bitx;
            gomc_hal_bit_t 	*mode18bity;
            gomc_hal_bit_t 	*commandmodex;
            gomc_hal_bit_t 	*commandmodey;
            gomc_hal_u32_t 	*status;
            gomc_hal_bit_t 	*posx_overflow;
            gomc_hal_bit_t 	*posy_overflow;
            gomc_hal_bit_t 	*velx_overflow;
            gomc_hal_bit_t 	*vely_overflow;
        } pin;

    } hal;


    //previous MPG counts for this instance	
    gomc_hal_float_t prev_accx_cmd;	
    gomc_hal_float_t prev_accy_cmd;	
    gomc_hal_float_t prev_velx_cmd;	
    gomc_hal_float_t prev_vely_cmd;	
    gomc_hal_float_t prev_posx_cmd;	
    gomc_hal_float_t prev_posy_cmd;	

 
} hm2_xy2mod_instance_t;

// these hal params affect all xy2mod instances
typedef struct {
    struct {
        gomc_hal_s32_t *dpll_rtimer_num;
        gomc_hal_s32_t *dpll_wtimer_num;
    } pin;
} hm2_xy2mod_module_global_t;


typedef struct {
    int num_instances;
    hm2_xy2mod_instance_t *instance;

    uint32_t clock_frequency;
    uint8_t version;


    // module-global HAL objects...

    hm2_xy2mod_module_global_t *hal;
    uint32_t written_dpll_rtimer_num;
    uint32_t written_dpll_wtimer_num;


    uint32_t accx_addr;
    uint32_t *accx_reg;

    uint32_t accy_addr;
    uint32_t *accy_reg;

    uint32_t velx_addr;
    uint32_t *velx_reg;

    uint32_t vely_addr;
    uint32_t *vely_reg;

    uint32_t posx_addr;
    uint32_t *posx_reg;

    uint32_t posy_addr;
    uint32_t *posy_reg;

    uint32_t mode_addr;
    uint32_t *mode_reg;

    uint32_t status_addr;
    uint32_t *status_reg;

    uint32_t command_addr;
    uint32_t *command_reg;

    uint32_t dpll_rtimer_num_addr;
    uint32_t *dpll_rtimer_num_reg;

    uint32_t dpll_wtimer_num_addr;
    uint32_t *dpll_wtimer_num_reg;


} hm2_xy2mod_t;


//
// 3-Phase pwmgen
//


typedef struct {

    struct {

        struct {
            gomc_hal_float_t *Avalue;
            gomc_hal_float_t *Bvalue;
            gomc_hal_float_t *Cvalue;
            gomc_hal_bit_t *fault;
            gomc_hal_bit_t *enable;
        } pin;

        struct {
            gomc_hal_float_t scale;
            gomc_hal_float_t deadzone;
            gomc_hal_bit_t faultpolarity;
            gomc_hal_float_t sampletime;
        } param;

    } hal;

    // these keep track of the written values of each register so we
    // know if an update-write is needed
    // enable is a little more complicated and is based on the read-back
    // of the fault/enable register
    double written_deadzone;
    unsigned written_faultpolarity;
    double written_sampletime;
} hm2_tp_pwmgen_instance_t;

typedef struct {
    struct {
        gomc_hal_u32_t pwm_frequency; // One PWM rate for all instances
    } param;
} hm2_tp_pwmgen_global_hal_t;

typedef struct {
    int num_instances;

    hm2_tp_pwmgen_instance_t *instance;

    hm2_tp_pwmgen_global_hal_t *hal;

    uint32_t clock_frequency;
    uint8_t version;

    // these keep track of the most recent hal->param.p{d,w}m_frequency
    // that we've told the FPGA about, so we know if we need to update it
    uint32_t written_pwm_frequency;

    uint32_t pwm_value_addr; // All three phases share a register (10 bits each)
    uint32_t *pwm_value_reg; // Pointer to a memory block that holds the set.

    uint32_t setup_addr; // holds dead-time, fault polarity and ADC sample time
    uint32_t *setup_reg;

    uint32_t enable_addr; // a 32-bit enable register for each tp_pwmgen. Seems excessive
    uint32_t *enable_reg;

    uint32_t pwmgen_master_rate_dds_addr;
    uint32_t pwmgen_master_rate_dds_reg;  // one register for the whole Function

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
    uint32_t data_addr;
    uint32_t *data_read_reg;
    uint32_t *data_write_reg;

    uint32_t ddr_addr;
    uint32_t *ddr_reg;
    uint32_t *written_ddr;  // not a register, but a copy of the most recently written value

    uint32_t alt_source_addr;
    uint32_t *alt_source_reg;

    uint32_t open_drain_addr;
    uint32_t *open_drain_reg;
    uint32_t *written_open_drain;  // not a register, but a copy of the most recently written value

    uint32_t output_invert_addr;
    uint32_t *output_invert_reg;
    uint32_t *written_output_invert;  // not a register, but a copy of the most recently written value

    uint32_t clock_frequency;
    uint8_t version;
} hm2_ioport_t;




// 
// stepgen
// 
#define HM2_STEPGEN_SWAP_STEP_DIR       (1<<2)
#define HM2_STEPGEN_LATCH_ON_INDEX      (1<<4)
#define HM2_STEPGEN_INDEX_POLARITY      (1<<5)
#define HM2_STEPGEN_LATCH_ON_PROBE      (1<<6)
#define HM2_STEPGEN_PROBE_POLARITY      (1<<7)

#define HM2_STEPGEN_LATCH_MASK  (0xffffff00)
#define HM2_STEPGEN_MODE_MASK   (0x000000ff)

typedef struct {
    struct {

        struct {
            gomc_hal_float_t *position_cmd;
            gomc_hal_float_t *velocity_cmd;
            gomc_hal_s32_t *counts;
            gomc_hal_float_t *position_fb;
            gomc_hal_float_t *position_latch;
            gomc_hal_float_t *velocity_fb;
            gomc_hal_bit_t *enable;
            gomc_hal_bit_t *control_type;   // 0="position control", 1="velocity control"
            gomc_hal_bit_t *position_reset; // reset position when true
            gomc_hal_bit_t *index_enable;	
            gomc_hal_bit_t *index_polarity;
            gomc_hal_bit_t *latch_enable;
            gomc_hal_bit_t *latch_polarity;

            // debug pins
            gomc_hal_float_t *dbg_ff_vel;
            gomc_hal_float_t *dbg_vel_error;
            gomc_hal_float_t *dbg_s_to_match;
            gomc_hal_float_t *dbg_err_at_match;
            gomc_hal_s32_t *dbg_step_rate;
            gomc_hal_float_t *dbg_pos_minus_prev_cmd;
        } pin;

        struct {
            gomc_hal_float_t position_scale;
            gomc_hal_float_t maxvel;
            gomc_hal_float_t maxaccel;

            gomc_hal_u32_t steplen;
            gomc_hal_u32_t stepspace;
            gomc_hal_u32_t dirsetup;
            gomc_hal_u32_t dirhold;

            gomc_hal_u32_t step_type;
            gomc_hal_bit_t swap_step_dir;
            gomc_hal_u32_t table[5]; // the Fifth Element is used as a very crude hash
        } param;

    } hal;

    // this variable holds the previous position command, for
    // computing the feedforward velocity
    gomc_hal_float_t old_position_cmd;

    uint32_t prev_accumulator;

    // this is a 48.16 signed fixed-point representation of the current
    // stepgen position (16 bits of sub-step resolution)
    int64_t subcounts;
    int32_t zero_offset;

    uint32_t written_steplen;
    uint32_t written_stepspace;
    uint32_t written_dirsetup;
    uint32_t written_dirhold;
    uint32_t written_step_type;
    uint32_t written_swap_step_dir;
    uint32_t written_index_enable; 
    uint32_t written_probe_enable;
    uint32_t written_index_polarity; 
    uint32_t written_probe_polarity;

    uint32_t table_width;
    
} hm2_stepgen_instance_t;


// these hal params affect all stepgen instances
typedef struct {
    struct {
        gomc_hal_s32_t *dpll_timer_num;
    } pin;
} hm2_stepgen_module_global_t;

typedef struct {
    int num_instances;
    hm2_stepgen_instance_t *instance;

    uint32_t clock_frequency;
    uint8_t version;
    int firmware_supports_swap;
    int firmware_supports_index;

    // module-global HAL objects...
    hm2_stepgen_module_global_t *hal;
    uint32_t written_dpll_timer_num;

    // write this (via TRAM) every hm2_<foo>.write
    uint32_t step_rate_addr;
    uint32_t *step_rate_reg;

    // read this (via TRAM) every hm2_<foo>.read
    uint32_t accumulator_addr;
    uint32_t *accumulator_reg;

    uint32_t mode_addr;
    uint32_t *mode_reg;

    uint32_t dir_setup_time_addr;
    uint32_t *dir_setup_time_reg;

    uint32_t dir_hold_time_addr;
    uint32_t *dir_hold_time_reg;

    uint32_t pulse_width_addr;
    uint32_t *pulse_width_reg;

    uint32_t pulse_idle_width_addr;
    uint32_t *pulse_idle_width_reg;

    uint32_t table_sequence_data_setup_addr;
    uint32_t table_sequence_length_addr;

    uint32_t master_dds_addr;
    
    uint32_t dpll_timer_num_addr;
} hm2_stepgen_t;



//
// Buffered SPI transceiver
//

typedef struct {
    uint32_t cd[16];
    uint16_t addr[16];
    int conf_flag[16];
    uint16_t cd_addr;
    uint16_t count_addr;
    gomc_hal_u32_t *count;
    int num_frames;
    uint32_t clock_freq;
    uint16_t base_address;
    uint32_t register_stride;
    uint32_t instance_stride;
    char name[GOMC_HAL_NAME_LEN+1];
    int (*read_function)(void*);
    int (*write_function)(void*);
    void *subdata;
} hm2_bspi_instance_t;

typedef struct {
    int version;
    int num_instances;
    hm2_bspi_instance_t *instance;
    uint8_t instances;
    uint8_t num_registers;
} hm2_bspi_t;

//
// UART
// 

typedef struct {
    uint32_t clock_freq;
    uint32_t bitrate;
    uint32_t tx_fifo_count_addr;
    uint32_t tx_fifo_count;
    uint32_t tx_bitrate_addr;
    uint32_t tx1_addr;
    uint32_t tx2_addr;
    uint32_t tx3_addr;
    uint32_t tx4_addr;
    uint32_t tx_mode_addr;
    uint32_t rx_fifo_count_addr;
    uint32_t rx_bitrate_addr;
    uint32_t rx1_addr;
    uint32_t rx2_addr;
    uint32_t rx3_addr;
    uint32_t rx4_addr;
    uint32_t rx_mode_addr;
    char name[GOMC_HAL_NAME_LEN+1];
} hm2_uart_instance_t;

typedef struct {
    int version;
    int num_instances;
    hm2_uart_instance_t *instance;
    uint8_t instances;
    uint8_t num_registers;
} hm2_uart_t;

//
// PktUART
// 

typedef struct {
    uint32_t clock_freq;
    uint32_t tx_bitrate;
    uint32_t rx_bitrate;
    uint32_t tx_fifo_count_addr;
    uint32_t tx_bitrate_addr;
    uint32_t tx_addr;
    uint32_t tx_mode_addr;
    uint32_t tx_mode;
    uint32_t rx_fifo_count_addr;
    uint32_t rx_bitrate_addr;
    uint32_t rx_addr;
    uint32_t rx_mode_addr;
    uint32_t rx_mode;
    char name[256];
} hm2_pktuart_instance_t;

typedef struct {
    int version;
    int tx_version;
    int rx_version;
    int num_instances;
    hm2_pktuart_instance_t *instance;
    uint32_t *tx_status_reg;
    uint32_t *rx_status_reg;
    uint8_t instances;
    uint8_t num_registers;
    struct rtapi_heap *heap;
} hm2_pktuart_t;
//
// HM2DPLL
//

typedef struct {
    gomc_hal_float_t *time1_us;
    gomc_hal_float_t *time2_us;
    gomc_hal_float_t *time3_us;
    gomc_hal_float_t *time4_us;
    gomc_hal_float_t *base_freq;
    gomc_hal_float_t *phase_error;
    gomc_hal_u32_t *plimit;
    gomc_hal_u32_t *ddssize;
    gomc_hal_u32_t *time_const;
    gomc_hal_u32_t *prescale;
} hm2_dpll_pins_t ;

typedef struct {

    int num_instances ;
    hm2_dpll_pins_t *pins ;

    uint32_t base_rate_addr;
    uint32_t base_rate_written;
    uint32_t phase_err_addr;
    uint32_t control_reg0_addr;
    uint32_t control_reg0_written;
    uint32_t control_reg1_addr;
    uint32_t control_reg1_written;
    uint32_t *control_reg1_read;
    uint32_t timer_12_addr;
    uint32_t timer_12_written;
    uint32_t timer_34_addr;
    uint32_t timer_34_written;
    uint32_t hm2_dpll_sync_addr;
    uint32_t *hm2_dpll_sync_reg;
    uint32_t clock_frequency;

} hm2_dpll_t ;


// 
// watchdog
// 

typedef struct {
    struct {

        struct {
            gomc_hal_bit_t *has_bit;
        } pin;

        struct {
            gomc_hal_u32_t timeout_ns;
        } param;

    } hal;

    uint32_t written_timeout_ns;

    int enable;  // gets set to 0 at load time, gets set to 1 at first pet_watchdog
    int written_enable;
} hm2_watchdog_instance_t;


typedef struct {
    int num_instances;
    hm2_watchdog_instance_t *instance;

    uint32_t clock_frequency;
    uint8_t version;

    uint32_t timer_addr;
    uint32_t *timer_reg;

    uint32_t status_addr;
    uint32_t *status_reg;

    uint32_t reset_addr;
    uint32_t *reset_reg;
} hm2_watchdog_t;

//
// On-board LEDs
//

typedef struct {
        gomc_hal_bit_t *led;
    } hm2_led_instance_t ;

typedef struct {

    int num_instances ;

    hm2_led_instance_t *instance ;

    uint32_t written_buff ;

    uint32_t led_addr;
    uint32_t *led_reg;

} hm2_led_t ;


//
// SSR
//

typedef struct {
    struct {

        struct {
            gomc_hal_u32_t *rate;
            gomc_hal_bit_t *out[32];
            gomc_hal_bit_t *invert[32];
        } pin;

    } hal;

    uint32_t written_data;
    uint32_t written_rate;
} hm2_ssr_instance_t;

typedef struct {
    int num_instances;
    hm2_ssr_instance_t *instance;

    uint8_t version;
    uint32_t clock_freq;

    uint32_t data_addr;
    uint32_t *data_reg;

    uint32_t rate_addr;
    uint32_t *rate_reg;
} hm2_ssr_t;

//
// OUTM
//

typedef struct {
    struct {

        struct {
            gomc_hal_bit_t *out[32];
            gomc_hal_bit_t *invert[32];
        } pin;

    } hal;

    uint32_t written_data;

} hm2_outm_instance_t;

typedef struct {
    int num_instances;
    hm2_outm_instance_t *instance;

    uint8_t version;
    uint32_t clock_freq;

    uint32_t data_addr;
    uint32_t *data_reg;

} hm2_outm_t;


// 
// raw peek/poke access
//

typedef struct {
    struct {
        struct {
            gomc_hal_u32_t *read_address;
            gomc_hal_u32_t *read_data;

            gomc_hal_u32_t *write_address;
            gomc_hal_u32_t *write_data;
            gomc_hal_bit_t *write_strobe;

            gomc_hal_bit_t *dump_state;
        } pin;
    } hal;
} hm2_raw_t;




// 
// this struct hold an entry in our Translation RAM region list
//

typedef struct {
    uint16_t addr;
    uint16_t size;
    uint32_t **buffer;
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
        int num_rcpwmgens;
        int num_tp_pwmgens;
        int num_stepgens;
        int stepgen_width;
        int num_leds;
        int num_sserials;
        int num_bspis;
        int num_uarts;
        int num_pktuarts;
        int num_dplls;
        int num_inmuxs;
        int num_inms;
        int num_xy2mods;
        int num_ssrs;
        int num_outms;
        int num_oneshots;
        char sserial_modes[4][8];
        int enable_raw;
        char *firmware;
    } config;

    char config_name[HM2_CONFIGNAME_LENGTH + 1];
    uint16_t idrom_offset;

    hm2_idrom_t idrom;

    hm2_module_descriptor_t md[HM2_MAX_MODULE_DESCRIPTORS];
    int num_mds;

    int dpll_module_present;
    int use_serial_numbers;
    
    hm2_pin_t *pin;
    int num_pins;

    // this keeps track of all the tram entries
    struct rtapi_list_head tram_read_entries;
    uint32_t *tram_read_buffer;
    uint16_t tram_read_size;

    struct rtapi_list_head tram_write_entries;
    uint32_t *tram_write_buffer;
    uint16_t tram_write_size;

    // the hostmot2 "Functions"
    hm2_encoder_t encoder;
    hm2_absenc_t absenc;
    hm2_resolver_t resolver;
    hm2_pwmgen_t pwmgen;
    hm2_rcpwmgen_t rcpwmgen;
    hm2_tp_pwmgen_t tp_pwmgen;
    hm2_stepgen_t stepgen;
    hm2_sserial_t sserial;
    hm2_bspi_t bspi;
    hm2_uart_t uart;
    hm2_pktuart_t pktuart;
    hm2_ioport_t ioport;
    hm2_watchdog_t watchdog;
    hm2_dpll_t dpll;
    hm2_inmux_t inmux;
    hm2_inm_t inm;
    hm2_xy2mod_t xy2mod;
    hm2_led_t led;
    hm2_ssr_t ssr;
    hm2_outm_t outm;
    hm2_oneshot_t oneshot;

    hm2_raw_t *raw;

    bool ddr_initialized;

    struct rtapi_list_head list;
} hostmot2_t;


//
// misc little helper functions
//

// this one just returns TRUE if the MD is good, FALSE if not
int hm2_md_is_consistent(
    hostmot2_t *hm2,
    int md_index,
    uint8_t version,
    uint8_t num_registers,
    uint32_t instance_stride,
    uint32_t multiple_registers
);

// this one prints a warning message about the unexpected MD,
// *then* returns TRUE if the MD is good, FALSE if not
int hm2_md_is_consistent_or_complain(
    hostmot2_t *hm2,
    int md_index,
    uint8_t version,
    uint8_t num_registers,
    uint32_t instance_stride,
    uint32_t multiple_registers
);

const char *hm2_get_general_function_name(int gtag);

const char *hm2_hz_to_mhz(uint32_t freq_hz);

void hm2_print_modules(hostmot2_t *hm2);

// functions to get handles to components by name
hm2_sserial_remote_t *hm2_get_sserial(hostmot2_t **hm2, const char *name);
int hm2_get_bspi(hostmot2_t **hm2, const char *name);
int hm2_get_uart(hostmot2_t **hm2, const char *name);
int hm2_get_pktuart(hostmot2_t **hm2, const char *name);


//
// Translation RAM functions
//

int hm2_register_tram_read_region(hostmot2_t *hm2, uint16_t addr, uint16_t size, uint32_t **buffer);
int hm2_register_tram_write_region(hostmot2_t *hm2, uint16_t addr, uint16_t size, uint32_t **buffer);
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
void hm2_set_pin_direction_immediate(hostmot2_t *hm2, int pin_number, int direction);  // gpio needs this
void hm2_set_pin_direction_at_start(hostmot2_t *hm2, int pin_number, int direction);  // gpio needs this
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
void hm2_ioport_initialize_ddr(hostmot2_t *hm2);

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
// oneshot functions
//

int hm2_oneshot_parse_md(hostmot2_t *hm2, int md_index);
void hm2_oneshot_print_module(hostmot2_t *hm2);
void hm2_oneshot_cleanup(hostmot2_t *hm2);
void hm2_oneshot_write(hostmot2_t *hm2);
void hm2_oneshot_force_write(hostmot2_t *hm2);
void hm2_oneshot_prepare_tram_write(hostmot2_t *hm2);
void hm2_oneshot_process_tram_read(hostmot2_t *hm2);

//
// rcpwmgen functions
//

int hm2_rcpwmgen_parse_md(hostmot2_t *hm2, int md_index);
void hm2_rcpwmgen_print_module(hostmot2_t *hm2);
void hm2_rcpwmgen_cleanup(hostmot2_t *hm2);
void hm2_rcpwmgen_write(hostmot2_t *hm2);
void hm2_rcpwmgen_force_write(hostmot2_t *hm2);
void hm2_rcpwmgen_prepare_tram_write(hostmot2_t *hm2);


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
int hm2_sserial_waitfor(hostmot2_t *hm2, uint32_t addr, uint32_t mask, int ms);
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
int hm2_bspi_clear_fifo(char * name);
void hm2_bspi_write(hostmot2_t *hm2);
void hm2_bspi_force_write(hostmot2_t *hm2);
void hm2_bspi_prepare_tram_write(hostmot2_t *hm2, long period);
void hm2_bspi_process_tram_read(hostmot2_t *hm2, long period);
int hm2_allocate_bspi_tram(char* name);
int hm2_bspi_write_chan(char* name, int chan, uint32_t val);
int hm2_allocate_bspi_tram(char* name);
int hm2_tram_add_bspi_frame(char *name, int chan, uint32_t **wbuff, uint32_t **rbuff);
int hm2_bspi_setup_chan(char *name, int chan, int cs, int bits, double mhz,
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
int hm2_uart_setup(char *name, int bitrate, int32_t tx_mode, int32_t rx_mode);
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

//
// hm2dpll functions
//

void hm2_dpl_cleanup(hostmot2_t *hm2);
int hm2_dpll_parse_md(hostmot2_t *hm2, int md_index);
void hm2_dpll_process_tram_read(hostmot2_t *hm2, long period);
void hm2_dpll_write(hostmot2_t *hm2, long period);


//
// InMux functions
//

int  hm2_inmux_parse_md(hostmot2_t *hm2, int md_index);
void hm2_inmux_print_module(hostmot2_t *hm2);
void hm2_inmux_cleanup(hostmot2_t *hm2);
void hm2_inmux_write(hostmot2_t *hm2);
void hm2_inmux_force_write(hostmot2_t *hm2);
void hm2_inmux_prepare_tram_write(hostmot2_t *hm2);
void hm2_inmux_queue_read(hostmot2_t *hm2);
void hm2_inmux_process_tram_read(hostmot2_t *hm2);

//
// InM functions
//

int  hm2_inm_parse_md(hostmot2_t *hm2, int md_index);
void hm2_inm_print_module(hostmot2_t *hm2);
void hm2_inm_cleanup(hostmot2_t *hm2);
void hm2_inm_write(hostmot2_t *hm2);
void hm2_inm_force_write(hostmot2_t *hm2);
void hm2_inm_prepare_tram_write(hostmot2_t *hm2);
void hm2_inm_queue_read(hostmot2_t *hm2);
void hm2_inm_process_tram_read(hostmot2_t *hm2);

//
// xy2mod functions
//

int  hm2_xy2mod_parse_md(hostmot2_t *hm2, int md_index);
void hm2_xy2mod_print_module(hostmot2_t *hm2);
void hm2_xy2mod_cleanup(hostmot2_t *hm2);
void hm2_xy2mod_write(hostmot2_t *hm2);
void hm2_xy2mod_force_write(hostmot2_t *hm2);
void hm2_xy2mod_queue_read(hostmot2_t *hm2);
void hm2_xy2mod_process_tram_read(hostmot2_t *hm2);


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
// OUTM functions
//

int hm2_outm_parse_md(hostmot2_t *hm2, int md_index);
void hm2_outm_cleanup(hostmot2_t *hm2);
void hm2_outm_force_write(hostmot2_t *hm2);
void hm2_outm_prepare_tram_write(hostmot2_t *hm2);
void hm2_outm_print_module(hostmot2_t *hm2);

//
// ONESHOT functions
//

int hm2_oneshot_parse_md(hostmot2_t *hm2, int md_index);
void hm2_oneshot_cleanup(hostmot2_t *hm2);
void hm2_oneshot_force_write(hostmot2_t *hm2);
void hm2_oneshot_prepare_tram_write(hostmot2_t *hm2);
void hm2_oneshot_print_module(hostmot2_t *hm2);


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
