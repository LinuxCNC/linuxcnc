/*
 * mb2hal.h
 * Userspace HAL component to communicate with one or more Modbus devices.
 * Migrated to cmod plugin API for in-process launcher operation.
 *
 * Original: Copyright (C) 2012 Victor Rocco <victor_rocco AT hotmail DOT com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301-1307
 * USA.
 */

#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>

#include "rtapi.h"
#include "rtapi_string.h"
#include "rtapi_errno.h"
#include "inifile.h"

#include <modbus.h>

#include "launcher/pkg/cmodule/gomc_env.h"

#define MB2HAL_MAX_LINKS            32
#define MB2HAL_MAX_DEVICE_LENGTH    32
#define MB2HAL_DEFAULT_TCP_PORT    502
#define MB2HAL_DEFAULT_MB_RESPONSE_TIMEOUT_MS 500
#define MB2HAL_DEFAULT_MB_BYTE_TIMEOUT_MS     500
#define MB2HAL_MAX_FNCT01_ELEMENTS 100
#define MB2HAL_MAX_FNCT02_ELEMENTS 100
#define MB2HAL_MAX_FNCT03_ELEMENTS 100
#define MB2HAL_MAX_FNCT04_ELEMENTS 100
#define MB2HAL_MAX_FNCT05_ELEMENTS 1
#define MB2HAL_MAX_FNCT06_ELEMENTS 1
#define MB2HAL_MAX_FNCT15_ELEMENTS 100
#define MB2HAL_MAX_FNCT16_ELEMENTS 100

// Forward declaration
typedef struct mb2hal_module mb2hal_module;

typedef enum { linkRTU,
               linkTCP
             } link_type_t;

typedef enum { mbtxERR,
               mbtx_02_READ_DISCRETE_INPUTS,
               mbtx_03_READ_HOLDING_REGISTERS,
               mbtx_04_READ_INPUT_REGISTERS,
               mbtx_06_WRITE_SINGLE_REGISTER,
               mbtx_15_WRITE_MULTIPLE_COILS,
               mbtx_16_WRITE_MULTIPLE_REGISTERS,
               mbtx_01_READ_COILS,
               mbtx_05_WRITE_SINGLE_COIL,
               mbtxMAX
             } mb_tx_fnct; //modbus transaction code
typedef enum { debugSILENT, debugERR, debugOK, debugDEBUG, debugMAX
             } DEBUG_TYPE; //message levels
typedef enum { retOK, retOKwithWarning, retERR
             } retCode; //functions return codes

// Logging macros — route through the cmod env callbacks.
// Each takes a module pointer as the first argument.
#define ERR(m, debug, fmt, args...) do { if((debug) >= debugERR) { \
    gomc_log_errorf((m)->env->log, (m)->name, "%s ERR: " fmt, fnct_name, ## args); \
    } } while(0)

#define OK(m, debug, fmt, args...) do { if((debug) >= debugOK) { \
    gomc_log_infof((m)->env->log, (m)->name, "%s OK: " fmt, fnct_name, ## args); \
    } } while(0)

#define DBG(m, debug, fmt, args...) do { if((debug) >= debugDEBUG) { \
    gomc_log_debugf((m)->env->log, (m)->name, "%s DEBUG: " fmt, fnct_name, ## args); \
    } } while(0)

#define DBGMAX(m, debug, fmt, args...) do { if((debug) >= debugMAX) { \
    gomc_log_debugf((m)->env->log, (m)->name, "%s DEBUGMAX: " fmt, fnct_name, ## args); \
    } } while(0)

//Modbus transaction structure (mb_tx_t)
//Store each transaction defined in INI config file
//Plus HAL and run time parameters for each transaction
typedef struct {
    //cfg_* are link params only for INI config reading purpose
    //then we use the parameters of mb_link_t
    link_type_t cfg_link_type; //RTU (serial) or TCP
    char cfg_link_type_str[10]; //str version of lp_link_type
    char cfg_serial_device[MB2HAL_MAX_DEVICE_LENGTH]; //device: example "/dev/ttyS0"
    int  cfg_serial_baud;      //bauds
    char cfg_serial_parity[5]; //parity: "even", "odd", "none"
    int  cfg_serial_data_bit;  //data bit
    int  cfg_serial_stop_bit;  //stop bit
    int  cfg_serial_delay_ms;  //delay between tx in serial lines
    char cfg_tcp_ip[17];       //tcp address
    int  cfg_tcp_port;         //tcp port number
    //mb_* are Modbus transaction protocol related params
    int        mb_tx_slave_id; //MB device id
    mb_tx_fnct mb_tx_fnct;     //MB function code id
    char       mb_tx_fnct_name[64]; //str version of mb_tx_fnct
    int        mb_tx_1st_addr; //MB first register
    int        mb_tx_nelem;    //MB n registers
    char **    mb_tx_names;    //MB register pin names. NULL if no names are specified
    double *   pin_scale;      //per-pin scale factor (default 1.0), float pins only
    double *   pin_offset;     //per-pin offset (default 0.0), float pins only
    int        mb_response_timeout_ms; //MB response timeout
    int        mb_byte_timeout_ms;     //MB byte timeout
    //cfg_* are others INI config params
    double cfg_update_rate;    //tx update rate
    int    cfg_debug;          //tx debug level (program, may be also protocol)
    //Modbus protocol debug
    int  protocol_debug;       //Flag debug Modbus protocol
    //internal processing values
    int mb_tx_num;         //each tx know it's own number
    int mb_link_num;       //each tx know it's own link
    //internal processing values
    double time_increment; //wait time between tx
    double next_time;      //next time for this tx
    double last_time_ok;   //last OK tx time
    //HAL related params
    char hal_tx_name[GOMC_HAL_NAME_LEN + 1];
    gomc_hal_float_t **float_value;
    gomc_hal_s32_t **int_value;
    gomc_hal_bit_t **bit;
    gomc_hal_bit_t **bit_inv;
    gomc_hal_u32_t **num_errors;     //num of acummulated errors (0=last tx OK)
} mb_tx_t;

//Modbus link structure (mb_link_t)
//Group common transaction's links in one unique link
//Store each run time Modbus link parameters here
typedef struct {
    //lp_* are real link params for real time use
    link_type_t lp_link_type; //RTU (serial) or TCP
    char lp_type_com_str[10]; //str version of lp_type_com
    char lp_serial_device[MB2HAL_MAX_DEVICE_LENGTH]; //device: example "/dev/ttyS0"
    int  lp_serial_baud;      //bauds
    char lp_serial_parity;    //parity: 'E', 'O', 'N'
    //NOTE in mb_tx is "even", "odd", "none"
    int  lp_serial_data_bit;  //data bit
    int  lp_serial_stop_bit;  //stop bit
    int  lp_serial_delay_ms;  //delay between tx in serial lines
    char lp_tcp_ip[17];       //tcp address
    int  lp_tcp_port;         //tcp port number
    //run time processing values
    int mb_link_num;       //corresponding number of this link/thread
    modbus_t *modbus;
    pthread_t thrd;
    mb2hal_module *m;      //back-pointer to parent module
} mb_link_t;

//Per-instance module state (replaces the old global gbl_t).
//Each cmod instance gets its own mb2hal_module.
struct mb2hal_module {
    cmod_t base;                   // must be first — cmod lifecycle vtable
    const cmod_env_t *env;         // launcher-provided environment
    char name[64];                 // instance name (HAL component name)

    //INI config file (app-specific mb2hal INI, NOT the main LinuxCNC INI)
    FILE *ini_file_ptr;
    char *ini_file_path;

    //INI config, common section
    int   init_dbg;
    int   version;
    double slowdown;

    //HAL related
    int   hal_mod_id;

    //mb_tx
    mb_tx_t *mb_tx;
    int tot_mb_tx;

    //mb_links
    mb_link_t *mb_links;
    int   tot_mb_links;

    //others
    const char *mb_tx_fncts[mbtxMAX];
    volatile int done;             // set by Stop() to signal threads to quit
};

//mb2hal.c
void *link_loop_and_logic(void *arg);
retCode is_this_tx_ready(mb2hal_module *m, const int this_mb_link_num, const int this_mb_tx_num, int *ret_available);
retCode get_tx_connection(mb2hal_module *m, const int mb_tx_num, int *ret_connected);
double get_time(void);

//mb2hal_init.c
retCode parse_args(mb2hal_module *m, int argc, const char **argv);
retCode parse_ini_file(mb2hal_module *m);
retCode parse_common_section(mb2hal_module *m);
retCode parse_transaction_section(mb2hal_module *m, const int mb_tx_num);
retCode parse_tcp_subsection(mb2hal_module *m, const char *section, const int mb_tx_num);
retCode parse_serial_subsection(mb2hal_module *m, const char *section, const int mb_tx_num);
retCode check_int_in(int n_args, const int int_value, ...);
retCode check_str_in(int n_args, const char *str_value, ...);
retCode init_mb_links(mb2hal_module *m);
retCode init_mb_tx(mb2hal_module *m);

//mb2hal_hal.c
retCode create_HAL_pins(mb2hal_module *m);
retCode create_each_mb_tx_hal_pins(mb2hal_module *m, mb_tx_t *mb_tx);

//mb2hal_modbus.c
retCode fnct_01_read_coils(mb2hal_module *m, mb_tx_t *this_mb_tx, mb_link_t *this_mb_link);
retCode fnct_02_read_discrete_inputs(mb2hal_module *m, mb_tx_t *this_mb_tx, mb_link_t *this_mb_link);
retCode fnct_03_read_holding_registers(mb2hal_module *m, mb_tx_t *this_mb_tx, mb_link_t *this_mb_link);
retCode fnct_04_read_input_registers(mb2hal_module *m, mb_tx_t *this_mb_tx, mb_link_t *this_mb_link);
retCode fnct_05_write_single_coil(mb2hal_module *m, mb_tx_t *this_mb_tx, mb_link_t *this_mb_link);
retCode fnct_06_write_single_register(mb2hal_module *m, mb_tx_t *this_mb_tx, mb_link_t *this_mb_link);
retCode fnct_15_write_multiple_coils(mb2hal_module *m, mb_tx_t *this_mb_tx, mb_link_t *this_mb_link);
retCode fnct_16_write_multiple_registers(mb2hal_module *m, mb_tx_t *this_mb_tx, mb_link_t *this_mb_link);
