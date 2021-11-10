/********************************************************************
* Description: t_pulse.c
*   IEC_61131-3 Pulse Time timer for HAL bit signals.
*
*   This is a HAL component that can be used to send a pulse signals
*   for a certain amount of time.
*
*   It supports up to 255 timers.
*
*   Loosely based on work from John Kasunich from debounce.c
*
*********************************************************************
*
* Author: Chad Woitas (aka satiowadahc)
* License: GPL Version 2
* Created on: 2021/06/10
* System: Linux
*
* Copyright (c) 2021 All rights reserved.
*
* Last change: Creation
*
********************************************************************/

#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include "rtapi_errno.h"
#include "rtapi_string.h"

/* module information */
MODULE_AUTHOR("Chad Woitas");
MODULE_DESCRIPTION("IEC TP - Pulse Timer");
MODULE_LICENSE("GPL");

#define NS_SECONDS(X) ((float)X/1000000000)
#define MAX_TIMERS 255

static int cfg[MAX_TIMERS];

RTAPI_MP_ARRAY_INT(cfg, MAX_TIMERS, "Group size for up to 255 timers");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

typedef struct{
    hal_bit_t *in;  // Input
    hal_float_t *pt;  // Pulse Timer
    hal_bit_t *q;   // Output
    hal_float_t *et;  // Elapsed Time since on in High
} t_p_t;

static t_p_t *timer_array;

/* other globals */
static int comp_id;		  /* component ID */

static int num_timers;	/* number of individual timers */


/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

static int export_timer(int num, t_p_t * addr);
static void update(void *arg, long period);


int rtapi_app_main(void){

   int retval, n;

  /* count number of timers */
  num_timers = 0;
  while ((num_timers < MAX_TIMERS) && (cfg[num_timers] != 0)) {
    if ((cfg[num_timers] < 1) || (cfg[num_timers] > MAX_TIMERS)) {
      rtapi_print_msg(RTAPI_MSG_ERR,
                      "IEC_TP: ERROR: bad group size '%d'\n", cfg[num_timers]);
      return -1;
    }
    num_timers += cfg[num_timers];
    num_timers++;
  }
  /* OK, now we've counted everything */
  if (num_timers == 0) {
    rtapi_print_msg(RTAPI_MSG_ERR,
                    "IEC_TP: ERROR: no channels configured\n");
    return -1;
  }

  /* have good config info, connect to the HAL */
  comp_id = hal_init("t_p");
  if (comp_id < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR,
                    "IEC_TP: ERROR: hal_init() failed\n");
    return -1;
  }

  /* allocate shared memory for timer group array */
  timer_array = hal_malloc(num_timers * sizeof(t_p_t));
  if (timer_array == 0) {
    rtapi_print_msg(RTAPI_MSG_ERR,
                    "IEC_TP: ERROR: hal_malloc() failed\n");
    hal_exit(comp_id);
    return -1;
  }

  /* export all vars */
  for (n = 0; n < num_timers; n++) {
    retval = export_timer(n, &(timer_array[n]));
    if (retval != 0) {
      rtapi_print_msg(RTAPI_MSG_ERR,
                      "IEC_TP: ERROR: group %d export failed\n", n);
      hal_exit(comp_id);
      return -1;
    }
  }

  retval = hal_export_funct("t_p.update", update, timer_array, 0, 0, comp_id);

  rtapi_print_msg(RTAPI_MSG_INFO,
                  "IEC_TP: installed %d groups of IEC_TP timers, %d total\n",
                  num_timers, num_timers);

  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void){
  hal_exit(comp_id);
}

static void update(void *arg, long period){
  t_p_t *timer;

  timer = arg;
  if(*(timer->pt) < 0) {
    *(timer->pt) = 0;
    rtapi_print_msg(RTAPI_MSG_WARN,
                    "IEC_TP: Pulse time must be positive, resetting to 0");
  }
  if(*(timer->et) < 0) {
    *(timer->et) = 0;
    rtapi_print_msg(RTAPI_MSG_WARN,
                    "IEC_TP: Elapsed time rolled over, resetting to 0");
  }

  float seconds = NS_SECONDS(period);

  // Check timers
  if(*(timer->in) || *(timer->q)){
    // Update outputs
    if(*(timer->et) < *(timer->pt)){
      *(timer->q) = 1;
      *(timer->et) += seconds;
    }
    else{
      *(timer->q) = 0;
    }
  }
  else{
    // Reset Variables
    *(timer->et) = 0;
    *(timer->q) = 0;
  }

}


/***********************************************************************
*                   LOCAL FUNCTION DEFINITIONS                         *
************************************************************************/

static int export_timer(int num, t_p_t * addr)
{
  int retval, msg;

  /* This function exports a lot of stuff, which results in a lot of
     logging if msg_level is at INFO or ALL. So we save the current value
     of msg_level and restore it later.  If you actually need to log this
     function's actions, change the second line below */
  msg = rtapi_get_msg_level();
  rtapi_set_msg_level(RTAPI_MSG_WARN);

  /* Export Pins */
  retval = hal_pin_bit_newf(HAL_IN, &(addr->in), comp_id,
                              "t_p.%d.in", num);
  if (retval != 0) { return retval; }

  retval = hal_pin_float_newf(HAL_IN, &(addr->pt), comp_id,
                            "t_p.%d.pt", num);
  if (retval != 0) { return retval; }

  retval = hal_pin_bit_newf(HAL_OUT, &(addr->q), comp_id,
                            "t_p.%d.q", num);
  if (retval != 0) { return retval; }

  retval = hal_pin_float_newf(HAL_OUT, &(addr->et), comp_id,
                            "t_p.%d.et", num);
  if (retval != 0) { return retval; }


  *(addr->in) = 0;
  *(addr->pt) = 1;


  /* restore saved message level */
  rtapi_set_msg_level(msg);
  return 0;
}

