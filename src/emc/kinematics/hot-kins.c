/********************************************************************
* Description: hotkins.c
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author: Chad Woitas
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2022 RMD Engineering Inc.
*
* Last change:
********************************************************************/

#include "motion.h"
#include "hal.h"
#include "rtapi_math.h"
#include "rtapi_app.h"
#include "kinematics.h"
#include "posemath.h"
#include "stdlib.h"
#include "stdio.h"

#define HW_JX 0
#define HW_JY 1
#define HW_JU 2
#define HW_JV 3
#define HW_JB 4

enum kins_setting{
    KINS_UNSYNCHRONIZED,
    KINS_SYNCHRONIZED
};

static kparms kp; // kinematics parms (common all types)

static struct hotwire_swdata {
    hal_bit_t   *kinstype_is_sync;
    hal_bit_t   *kinstype_is_unsync;
    hal_float_t *u_offset;
    hal_float_t *v_offset;

    hal_u32_t   *switchkins_type;
    hal_u32_t   *switchkins_type_old;
    hal_bit_t   *switchkins_update;


    hal_bit_t   *error;
    hal_bit_t   *reset_error;
} *hotwire_swdata;;

#define HOTWIRE_MAX_TYPES KINS_SYNCHRONIZED
static bool use_lastpose[HOTWIRE_MAX_TYPES] = {0};



static int sync_KinematicsForward(const double *joints,
                                  EmcPose * pos,
                                  const KINEMATICS_FORWARD_FLAGS * fflags,
                                  KINEMATICS_INVERSE_FLAGS * iflags){

  pos->tran.x = joints[HW_JX];
  pos->tran.y = joints[HW_JY];
  pos->u = joints[HW_JU];
  pos->v = joints[HW_JV];
  pos->b = joints[HW_JB];

  return 0;
} // sync_KinematicsForward

static int sync_KinematicsInverse(EmcPose * pos,
                                  double *joints,
                                  const KINEMATICS_INVERSE_FLAGS * iflags,
                                  KINEMATICS_FORWARD_FLAGS * fflags){

  if(use_lastpose[*hotwire_swdata->switchkins_type]){

    *hotwire_swdata->u_offset = joints[HW_JX] - joints[HW_JU];
    *hotwire_swdata->v_offset = joints[HW_JY] - joints[HW_JV];

    use_lastpose[*hotwire_swdata->switchkins_type] = 0;
  }

  joints[HW_JX] = pos->tran.x;
  joints[HW_JY] = pos->tran.y;
  joints[HW_JU] = pos->tran.x - *hotwire_swdata->u_offset;
  joints[HW_JV] = pos->tran.y - *hotwire_swdata->v_offset;

  joints[HW_JB] = pos->b;

  return 0;
} // sync_KinematicsInverse


static int unsync_KinematicsForward(const double *joints,
                                    EmcPose * pos,
                                    const KINEMATICS_FORWARD_FLAGS * fflags,
                                    KINEMATICS_INVERSE_FLAGS * iflags){

  pos->tran.x = joints[HW_JX];
  pos->tran.y = joints[HW_JY];
  pos->u = joints[HW_JU];
  pos->v = joints[HW_JV];
  pos->b = joints[HW_JB];

  return 0;
} //unsync_KinematicsForward

static int unsync_KinematicsInverse(EmcPose * pos,
                                    double *joints,
                                    const KINEMATICS_INVERSE_FLAGS * iflags,
                                    KINEMATICS_FORWARD_FLAGS * fflags){

  if(use_lastpose[*hotwire_swdata->switchkins_type]){
    use_lastpose[*hotwire_swdata->switchkins_type] = 0;
    pos->u = joints[HW_JU];
    pos->v = joints[HW_JV];
    *hotwire_swdata->u_offset = 0;
    *hotwire_swdata->v_offset = 0;
  }

  joints[HW_JX] = pos->tran.x;
  joints[HW_JY] = pos->tran.y;
  joints[HW_JU] = pos->u;
  joints[HW_JV] = pos->v;
  joints[HW_JB] = pos->b;

  return 0;
} //unsync_KinematicsInverse

//*********************************************************************

int kinematicsForward(const double *joint,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags){

  if(*hotwire_swdata->switchkins_type_old != *hotwire_swdata->switchkins_type){
    use_lastpose[*hotwire_swdata->switchkins_type] = 1;
    *hotwire_swdata->switchkins_update = !*hotwire_swdata->switchkins_update;
  }

  *hotwire_swdata->switchkins_type_old = *hotwire_swdata->switchkins_type;

  if(*hotwire_swdata->switchkins_type == KINS_SYNCHRONIZED){
    sync_KinematicsForward(joint, pos, fflags, iflags);
  }
  else{
    unsync_KinematicsForward(joint, pos, fflags, iflags);
  }

  return 0;
} // KinematicsForward()

int kinematicsInverse(const EmcPose * pos,
                      double *joint,
                      const KINEMATICS_INVERSE_FLAGS * iflags,
                      KINEMATICS_FORWARD_FLAGS * fflags){

  if(*hotwire_swdata->switchkins_type_old != *hotwire_swdata->switchkins_type){
    use_lastpose[*hotwire_swdata->switchkins_type] = 1;
    *hotwire_swdata->switchkins_update = !*hotwire_swdata->switchkins_update;
  }
  *hotwire_swdata->switchkins_type_old = *hotwire_swdata->switchkins_type;

  if(*hotwire_swdata->switchkins_type == KINS_SYNCHRONIZED){
    sync_KinematicsInverse((EmcPose *) pos, joint, iflags, fflags);
  }
  else{
    unsync_KinematicsInverse((EmcPose *) pos, joint, iflags, fflags);
  }

  return 0;
} // kinematicsInverse()



//*********************************************************************
int kinematicsSwitchable() {return 1;}

int kinematicsSwitch(int new_switchkins_type){

  for (int k=0; k< HOTWIRE_MAX_TYPES; k++) { use_lastpose[k] = 0;}

  rtapi_print_msg(RTAPI_MSG_INFO, "kinematicsSwitch:TYPE %d\n", *hotwire_swdata->switchkins_type);

  *hotwire_swdata->switchkins_type = new_switchkins_type;

  *hotwire_swdata->kinstype_is_sync = (*hotwire_swdata->switchkins_type == KINS_SYNCHRONIZED) ? 1 : 0;
  *hotwire_swdata->kinstype_is_unsync = (*hotwire_swdata->switchkins_type == KINS_UNSYNCHRONIZED) ? 1 : 0;

  if(( *hotwire_swdata->kinstype_is_sync + *hotwire_swdata->kinstype_is_unsync ) != 1){
    rtapi_print_msg(RTAPI_MSG_ERR, "Kins switch invalid value: %d",   *hotwire_swdata->switchkins_type);
    *hotwire_swdata->kinstype_is_sync = 0;
    *hotwire_swdata->kinstype_is_unsync = 0;
    return -1; // FAIL
  }

  use_lastpose[new_switchkins_type] = 1;

  return 0; // 0==> no error
} // kinematicsSwitch()

KINEMATICS_TYPE kinematicsType()
{
  return KINEMATICS_BOTH;
}


//*********************************************************************

EXPORT_SYMBOL(kinematicsSwitchable);
EXPORT_SYMBOL(kinematicsSwitch);
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
MODULE_LICENSE("GPL");

static int    comp_id;

void rtapi_app_error( char* emsg) {

  rtapi_print_msg(RTAPI_MSG_ERR,
                  "\nhot Kins FAIL %s:<%s>\n",kp.kinsname,emsg);
  hal_exit(comp_id);
  exit(-1);
}

//*********************************************************************
int rtapi_app_main(void)
{

  int res =0;

  kp.kinsname    = "hot-kins"; // !!! must agree with filename
  kp.halprefix   = "hot-kins"; // hal pin names
  kp.required_coordinates = "XYUVB";
  kp.allow_duplicates     = 1;
  kp.max_joints           = EMCMOT_MAX_JOINTS;


  if (kp.max_joints <= 0 || kp.max_joints > EMCMOT_MAX_JOINTS) {
    rtapi_app_error("Invalid max_joints");
  }


  comp_id = hal_init(kp.kinsname);
  if(comp_id < 0) rtapi_app_error("Hal Component Failed");

  hotwire_swdata = hal_malloc(sizeof(struct hotwire_swdata));
  if (!hotwire_swdata) rtapi_app_error("Hal Component Failed");;


  rtapi_print_msg(RTAPI_MSG_ERR, "Loading pins setup\n");
  res += hal_pin_bit_newf(HAL_OUT,    &(hotwire_swdata->kinstype_is_sync),    comp_id, "%s.is-sync",          kp.halprefix);
  res += hal_pin_bit_newf(HAL_OUT,    &(hotwire_swdata->kinstype_is_unsync),  comp_id, "%s.is-unsync",        kp.halprefix);
  res += hal_pin_float_newf(HAL_OUT,  &(hotwire_swdata->u_offset),            comp_id, "%s.u-offset",         kp.halprefix);
  res += hal_pin_float_newf(HAL_OUT,  &(hotwire_swdata->v_offset),            comp_id, "%s.v-offset",         kp.halprefix);

  res += hal_pin_u32_newf(HAL_IN,     &(hotwire_swdata->switchkins_type),     comp_id, "%s.switch-type",      kp.halprefix);
  res += hal_pin_u32_newf(HAL_IN,     &(hotwire_swdata->switchkins_type_old), comp_id, "%s.switch-type-old",  kp.halprefix);
  res += hal_pin_bit_newf(HAL_OUT,    &(hotwire_swdata->switchkins_update) ,  comp_id, "%s.switch-update",    kp.halprefix);
  res += hal_pin_bit_newf(HAL_OUT,    &(hotwire_swdata->error),               comp_id, "%s.error",            kp.halprefix);
  res += hal_pin_bit_newf(HAL_IN,     &(hotwire_swdata->reset_error),         comp_id, "%s.reset-error",      kp.halprefix);

  if(res != 0){
    return res;
  }

  *hotwire_swdata->switchkins_type = 1; // startup with default type
  *hotwire_swdata->switchkins_type_old = 1;

  kinematicsSwitch(KINS_SYNCHRONIZED);

  hal_ready(comp_id);

  return 0;

} // rtapi_app_main()

void rtapi_app_exit(void) { hal_exit(comp_id); }
