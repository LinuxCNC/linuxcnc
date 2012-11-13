#include "rtapi.h"
#ifdef RTAPI
#include "rtapi_app.h"
#endif
#include "rtapi_string.h"
#include "rtapi_errno.h"
#include "hal.h"

#ifdef MODULE_VERBOSE
MODULE_VERBOSE(emc2, "component:mb2hal:Userspace HAL component to communicate with one or more Modbus devices");
MODULE_VERBOSE(emc2, "license:LGPL");
MODULE_LICENSE("LGPL");
#endif

#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "inifile.h"
#include "emcglb.h"
#include "../modbus.h"

typedef enum { mbtxERR,
               mbtx_02_READ_DISCRETE_INPUTS,
               mbtx_03_READ_HOLDING_REGISTERS,
               mbtx_04_READ_INPUT_REGISTERS,
               mbtx_15_WRITE_MULTIPLE_COILS,
               mbtx_16_WRITE_MULTIPLE_REGISTERS,
               mbtxMAX
             } MB_TX_CODE; //modbus transaction code
typedef enum { debugSILENT, debugERR, debugOK, debugDEBUG, debugMAX
             } DEBUG_TYPE; //message levels
typedef enum { retOK, retOKwithWarning, retERR
             } retCode; //funtions return codes

#define ERR(debug, fmt, args...) if(debug >= debugERR) {fprintf(stderr, "%s %s ERR: "fmt"\n", hal_mod_name, fnct_name, ## args);}
#define OK(debug, fmt, args...) if(debug >= debugOK) {fprintf(stdout, "%s %s OK: "fmt"\n", hal_mod_name, fnct_name, ## args);}
#define DBG(debug, fmt, args...) if(debug >= debugDEBUG) {fprintf(stdout, "%s %s DEBUG: "fmt"\n", hal_mod_name, fnct_name, ## args);}

typedef struct {
    //link_params and lp_* are link connections params
    modbus_param_t cfg_lk_param; //RTU or TCP connection params
    //                           //only for INI config reading purpose
    //                           //then we use the parameters of gbl_mb_links
    char lp_type_com_str[10];   //str version of link_params.type_com (int)
    int  lp_serial_delay_ms;    //if serial line, delay between tx
    int  lp_tcp_port;           //if tcp, port number
    //mb_* are MB protocol related params
    int mb_slave_id;        //MB device id
    MB_TX_CODE mb_tx_code;  //MB protocol id
    int mb_first_addr;      //MB first register
    int mb_nelements;       //MB n registers
    //cfg_* are others INI config params
    int cfg_debug;          //tx debug level
    double cfg_update_rate; //tx update rate
    char cfg_mb_tx_code_name[64]; //str version of mb_tx_code;
    char hal_tx_name[HAL_NAME_LEN + 1];
    //internal processing values
    int mb_tx_num;         //corresponding gbl_mb_tx
    int mb_links_num;       //corresponding gbl_mb_links
    double time_increment; //wait time between tx
    double next_time;      //next time for this tx
    double last_time_ok;   //last OK tx time
    int    num_errors;     //num of acummulated erros (0=last tx OK)
    //hal pins
    hal_float_t **float_value;
    hal_s32_t **int_value;
    //hal_float_t *scale;  //not yet implemented
    //hal_float_t *offset; //not yet implemented
    hal_bit_t **bit;
} mb_tx_t; //transaction

extern const char *gbl_mb_tx_codes[mbtxMAX];
extern char *hal_mod_name;
extern int hal_mod_id;
extern int init_debug;
extern double slowdown;

extern modbus_param_t *gbl_mb_links;
extern int gbl_n_mb_links;
extern int *tmp_mb_links_num; //for thread parameter use only
extern mb_tx_t *gbl_mb_tx;
extern int gbl_n_mb_tx;

//mb2hal.c
void *link_loop_and_logic(void *tmp_mb_links_num);
retCode is_this_tx_available(const int mb_links_num, const int mb_tx_num, int *ret_available);
retCode get_tx_connection(const int mb_tx_num, int *ret_connected);
double get_time();
void quit_signal(int signal);
void quit_cleanup(void);

//mb2hal_init.c
retCode parse_main_args(char **ini_file_path, int argc, char **argv);
retCode parse_ini_file(FILE *ini_file_ptr);
retCode parse_common_section(FILE *ini_file_ptr);
retCode parse_transaction_section(FILE *ini_file_ptr, const int mb_tx_num);
retCode parse_tcp_subsection(FILE *ini_file_ptr, const char *section, const int mb_tx_num);
retCode parse_serial_subsection(FILE *ini_file_ptr, const char *section, const int mb_tx_num);
retCode check_int_in(int n_args, const int int_value, ...);
retCode check_str_in(int n_args, const char *str_value, ...);
retCode init_gbl_mb_links();
retCode init_gbl_mb_tx();

//mb2hal_hal.c
retCode create_HAL_pins();
retCode create_each_mb_tx_hal_pins(mb_tx_t *mb_tx);

//mb2hal_modbus.c
retCode fnct_15_write_multiple_coils(mb_tx_t *mb_tx);
retCode fnct_02_read_discrete_inputs(mb_tx_t *mb_tx);
retCode fnct_04_read_input_registers(mb_tx_t *mb_tx);
retCode fnct_03_read_holding_registers(mb_tx_t *mb_tx);
retCode fnct_16_write_multiple_registers(mb_tx_t *mb_tx);
