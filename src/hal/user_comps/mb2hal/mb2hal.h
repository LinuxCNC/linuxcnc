#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "rtapi.h"
#ifdef RTAPI
#include "rtapi_app.h"
#endif
#include "rtapi_string.h"
#include "rtapi_errno.h"
#include "hal.h"
#include "inifile.h"
#include "emcglb.h"

#include <modbus.h>

#define MB2HAL_MAX_LINKS            32
#define MB2HAL_MAX_DEVICE_LENGTH    32
#define MB2HAL_DEFAULT_TCP_PORT    502
#define MB2HAL_DEFAULT_MB_RESPONSE_TIMEOUT_MS 500
#define MB2HAL_DEFAULT_MB_BYTE_TIMEOUT_MS     500
#define MB2HAL_DEFAULT_TCP_PORT    502
#define MB2HAL_MAX_FNCT02_ELEMENTS 100
#define MB2HAL_MAX_FNCT03_ELEMENTS 100
#define MB2HAL_MAX_FNCT04_ELEMENTS 100
#define MB2HAL_MAX_FNCT05_ELEMENTS 100
#define MB2HAL_MAX_FNCT15_ELEMENTS 100
#define MB2HAL_MAX_FNCT16_ELEMENTS 100

#ifdef MODULE_VERBOSE
MODULE_VERBOSE(emc2, "component:mb2hal:Userspace HAL component to communicate with one or more Modbus devices");
MODULE_VERBOSE(emc2, "license:LGPL");
MODULE_LICENSE("LGPL");
#endif

typedef enum { linkRTU,
               linkTCP
             } link_type_t;

typedef enum { mbtxERR,
               mbtx_02_READ_DISCRETE_INPUTS,
               mbtx_03_READ_HOLDING_REGISTERS,
               mbtx_04_READ_INPUT_REGISTERS,
               mbtx_15_WRITE_MULTIPLE_COILS,
               mbtx_16_WRITE_MULTIPLE_REGISTERS,
               mbtxMAX
             } mb_tx_fnct; //modbus transaction code
typedef enum { debugSILENT, debugERR, debugOK, debugDEBUG, debugMAX
             } DEBUG_TYPE; //message levels
typedef enum { retOK, retOKwithWarning, retERR
             } retCode; //funtions return codes

#define ERR(debug, fmt, args...) if(debug >= debugERR) {fprintf(stderr, "%s %s ERR: "fmt"\n", gbl.hal_mod_name, fnct_name, ## args);}
#define OK(debug, fmt, args...) if(debug >= debugOK) {fprintf(stdout, "%s %s OK: "fmt"\n", gbl.hal_mod_name, fnct_name, ## args);}
#define DBG(debug, fmt, args...) if(debug >= debugDEBUG) {fprintf(stdout, "%s %s DEBUG: "fmt"\n", gbl.hal_mod_name, fnct_name, ## args);}

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
    char hal_tx_name[HAL_NAME_LEN + 1];
    hal_float_t **float_value;
    hal_s32_t **int_value;
    //hal_float_t *scale;  //not yet implemented
    //hal_float_t *offset; //not yet implemented
    hal_bit_t **bit;
    hal_u32_t **num_errors;     //num of acummulated errors (0=last tx OK)
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
} mb_link_t;

//Structure of global data (gbl_t)
//Reduce functions parameters using this common global structure.
typedef struct {
    //INI config file
    FILE *ini_file_ptr;
    char *ini_file_path;
    //INI config, common section
    int    init_dbg;
    double slowdown;
    //HAL related
    int   hal_mod_id;
    char *hal_mod_name;
    //mb_tx
    mb_tx_t *mb_tx;
    int tot_mb_tx;
    //mb_links
    mb_link_t *mb_links;
    int   tot_mb_links;
    //others
    const char *mb_tx_fncts[mbtxMAX];
    int quit_flag;
} gbl_t;

extern gbl_t gbl;

//mb2hal.c
void *link_loop_and_logic(void *thrd_link_num);
retCode is_this_tx_ready(const int this_mb_link_num, const int this_mb_tx_num, int *ret_available);
retCode get_tx_connection(const int mb_tx_num, int *ret_connected);
void set_init_gbl_params();
double get_time();
void quit_signal(int signal);
void quit_cleanup(void);

//mb2hal_init.c
retCode parse_main_args(int argc, char **argv);
retCode parse_ini_file();
retCode parse_common_section();
retCode parse_transaction_section(const int mb_tx_num);
retCode parse_tcp_subsection(const char *section, const int mb_tx_num);
retCode parse_serial_subsection(const char *section, const int mb_tx_num);
retCode check_int_in(int n_args, const int int_value, ...);
retCode check_str_in(int n_args, const char *str_value, ...);
retCode init_mb_links();
retCode init_mb_tx();

//mb2hal_hal.c
retCode create_HAL_pins();
retCode create_each_mb_tx_hal_pins(mb_tx_t *mb_tx);

//mb2hal_modbus.c
retCode fnct_15_write_multiple_coils(mb_tx_t *this_mb_tx, mb_link_t *this_mb_link);
retCode fnct_02_read_discrete_inputs(mb_tx_t *this_mb_tx, mb_link_t *this_mb_link);
retCode fnct_04_read_input_registers(mb_tx_t *this_mb_tx, mb_link_t *this_mb_link);
retCode fnct_03_read_holding_registers(mb_tx_t *this_mb_tx, mb_link_t *this_mb_link);
retCode fnct_16_write_multiple_registers(mb_tx_t *this_mb_tx, mb_link_t *this_mb_link);
