#include "rtapi.h"
#include "rtapi_ctype.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_errno.h"
#include "rtapi_math64.h"
#include <rtapi_io.h>
#include "hal.h"


#include "rtapi.h"
#ifdef RTAPI
#include "rtapi_app.h"
#endif
#include "rtapi_string.h"
#include "rtapi_errno.h"
#include "hal.h"
#include "rtapi_math64.h"

///********************************************************************
//* Description: tp.comp
//*   IEC_61131-3 Pulse Time timer for LinuxCNC HAL bit signals.
//*
//*   This is a HAL component that can be used to send a pulse signal
//*   for a certain amount of time.
//*
//*********************************************************************
//*
//* Author: Chad Woitas (aka satiowadahc)
//* License: GPL Version 2
//* Created on: 2021/06/10
//* System: Linux
//*
//* Copyright (c) 2021 All rights reserved.
//*
//* Last change: 2021-11-02 - Conversion to comp format
/* module information */
MODULE_AUTHOR("Chad Woitas (aka satiowadahc), Skynet");
MODULE_DESCRIPTION("IEC TP timer - generate a high pulse of defined duration on rising edge");
MODULE_LICENSE("GPL2");

static int comp_idx;

typedef struct {
    bool ok;
} skynet_t;
skynet_t *skynet;

typedef struct {
    hal_float_t *Pin;
} float_data_t;
float_data_t *et;   //! "Elapsed time since start of pulse in seconds";

//! Pins
typedef struct {
    hal_bit_t *Pin;
} bit_data_t;
bit_data_t *q;      //! "Output signal"
bit_data_t *in;     //! "Input signal"

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
param_float_data_t *pt; //! param rw float pt "Pulse time in seconds"

typedef struct {
    hal_bit_t Pin;
} param_bit_data_t;

static int comp_idx; /* component ID */

static bool in_old; //! Value of in on last cycle, for rising edge detection

static void fp();
static int setup_pins();
//! float totalnsec, totalseconds;
//! static long period = 1000000;	/* thread period - default = 1ms thread */

int rtapi_app_main(void) {

    int r = 0;
    comp_idx = hal_init("tp");
    if(comp_idx < 0) return comp_idx;
    r = hal_export_funct("fp", fp, &skynet,0,0,comp_idx);

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
static void fp(){
    if(pt->Pin < 0) {
        pt->Pin = 0;
        rtapi_print_msg(RTAPI_MSG_WARN,
                        "tp: Pulse time must be positive, resetting to 0");
    }
    if(et->Pin < 0) {
        *et->Pin = 0;
        rtapi_print_msg(RTAPI_MSG_WARN,
                        "tp: Elapsed time rolled over, resetting to 0");
    }

    // Check timers
    if((in && !in_old) || q){
        // Update outputs
        if(*et->Pin < pt->Pin){
            *q->Pin = 1;

            //! totalnsec = totalnsec + period;
            //! totalseconds = totalnsec * 0.000000001;
            *et->Pin += 0.001;
        }
        else{
            q = 0;
        }
    }
    else{
        // Reset Variables
        *et->Pin = 0;
        q = 0;
    }
    in_old = in;
}

static int setup_pins(){
    int r=0;

    //! Input pins, type bit.
    in = (bit_data_t*)hal_malloc(sizeof(bit_data_t));
    r+=hal_pin_bit_new("tp.in",HAL_IN,&(in->Pin),comp_idx);

    //! Output pins, type bit.
    q = (bit_data_t*)hal_malloc(sizeof(bit_data_t));
    r+=hal_pin_bit_new("tp.q",HAL_OUT,&(q->Pin),comp_idx);

    //! Output pins, float
    et = (float_data_t*)hal_malloc(sizeof(float_data_t));
    r+=hal_pin_float_new("tp.et",HAL_OUT,&(et->Pin),comp_idx);

    //! Param, float
    pt = (param_float_data_t*)hal_malloc(sizeof(param_float_data_t));
    r+=hal_param_float_new("tp.pt",HAL_RW,&(pt->Pin),comp_idx);

    return r;
}


