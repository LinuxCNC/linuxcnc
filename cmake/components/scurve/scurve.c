#include "rtapi.h"
#include "rtapi_ctype.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_errno.h"
#include "rtapi_math64.h"
#include <rtapi_io.h>
#include "hal.h"

#include "sc_engine.h"

/* module information */
MODULE_AUTHOR("Skynet");
MODULE_DESCRIPTION("Halmodule test");
MODULE_LICENSE("GPL");

static int comp_idx;

typedef struct {
    bool ok;
} skynet_t;
skynet_t *skynet;

typedef struct {
    hal_float_t *Pin;
} float_data_t;
float_data_t *vi,*si,*ai;

//! Pins
typedef struct {
    hal_bit_t *Pin;
} bit_data_t;
bit_data_t *module, *tool_on;

typedef struct {
    hal_s32_t *Pin;
} s32_data_t;

typedef struct {
    hal_u32_t *Pin;
} u32_data_t;

typedef struct {
    hal_port_t *Pin;
} port_data_t;
port_data_t *port;

//! Params
typedef struct {
    hal_float_t Pin;
} param_float_data_t;

typedef struct {
    hal_bit_t Pin;
} param_bit_data_t;

static int comp_idx; /* component ID */

static void the_function();
static int setup_pins();

struct sc_period p;
size_t size;
struct sc_period *pvec;
int trigger=0;
int finished_trigger=0;
float ttot=0, stot=0;
int finished=0;
struct sc_vsa vsa;
float at_time=0;


int rtapi_app_main(void) {

    int r = 0;
    comp_idx = hal_init("scurve");
    if(comp_idx < 0) return comp_idx;
    r = hal_export_funct("the_function", the_function, &skynet,0,0,comp_idx);

    r+=setup_pins();

    if(r) {
        hal_exit(comp_idx);
    } else {
        hal_ready(comp_idx);
    }
    return 0;
}

void rtapi_app_exit(void){
    hal_exit(comp_idx);
}

//! Perforn's every ms.
static void the_function(){

    at_time+=0.001;

    if(*module->Pin==1){


    }

    if(!trigger){
        set_a_dv(2,10);
        T vm=10;
        p.vo=0;
        p.ve=0;
        p.acs=0;
        p.ace=0;
        p.ncs=50;
        p.nct=0;
        p.id=id_run;

        process_curve(p,vm,&pvec,&size);
        at_time=0;

        // rtapi_print_msg(RTAPI_MSG_ERR,"pvec size: %zu \n",size);
        ttot=to_ttot_pvec(pvec,size);
        stot=to_stot_pvec(pvec,size);
        // rtapi_print_msg(RTAPI_MSG_ERR,"ttot: %f \n",ttot);
        // rtapi_print_msg(RTAPI_MSG_ERR,"stot: %f \n",stot);

        finished_trigger=0;
        trigger=1;
    }

    interpolate_periods(at_time,pvec,size,&vsa,&finished);
    *vi->Pin=vsa.v;
    *si->Pin=vsa.s;
    *ai->Pin=vsa.a;

    if(at_time>ttot && !finished_trigger){
        // rtapi_print_msg(RTAPI_MSG_ERR,"finished.\n");

        at_time=0;
        vsa.s=0;
        vsa.v=0;
        vsa.a=0;
        ttot=0;
        stot=0;

        // cleanup_periods(&pvec,size);
        size=0;
        pvec=NULL;

        // rtapi_print_msg(RTAPI_MSG_ERR,"pvec reset size: %zu \n",size);
        trigger=0;
        finished_trigger=1;
    }

    if(at_time<ttot){
        // rtapi_print_msg(RTAPI_MSG_ERR,"si: %f \n",vsa.s);
    }
}

static int setup_pins(){
    int r=0;

    //! Input pins, type bit.
    module = (bit_data_t*)hal_malloc(sizeof(bit_data_t));
    r+=hal_pin_bit_new("scurve.enable",HAL_IN,&(module->Pin),comp_idx);
    
    //! Output pins, type bit.
    tool_on = (bit_data_t*)hal_malloc(sizeof(bit_data_t));
    r+=hal_pin_bit_new("scurve.tool_on",HAL_OUT,&(tool_on->Pin),comp_idx);
    
    //! Output pins, type float.
    vi = (float_data_t*)hal_malloc(sizeof(float_data_t));
    r+=hal_pin_float_new("scurve.vi",HAL_OUT,&(vi->Pin),comp_idx);

    si = (float_data_t*)hal_malloc(sizeof(float_data_t));
    r+=hal_pin_float_new("scurve.si",HAL_OUT,&(si->Pin),comp_idx);

    ai = (float_data_t*)hal_malloc(sizeof(float_data_t));
    r+=hal_pin_float_new("scurve.ai",HAL_OUT,&(ai->Pin),comp_idx);

    return r;
}







































