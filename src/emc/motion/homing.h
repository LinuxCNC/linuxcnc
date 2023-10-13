#ifndef HOMING_H
#define HOMING_H

#include <rtapi_bool.h>

/* HOME_* flags (typ set in emc/task/taskintf.cc) */
#define HOME_IGNORE_LIMITS            1
#define HOME_USE_INDEX                2
#define HOME_IS_SHARED                4
#define HOME_UNLOCK_FIRST             8
#define HOME_ABSOLUTE_ENCODER        16
#define HOME_NO_REHOME               32
#define HOME_NO_FINAL_MOVE           64
#define HOME_INDEX_NO_ENCODER_RESET 128

//---------------------------------------------------------------------
// INTERFACE routines

// per-joint interface parameters (one-time setup)
void set_joint_homing_params(int    jno,
                             double offset,
                             double home,
                             double home_final_vel,
                             double home_search_vel,
                             double home_latch_vel,
                             int    home_flags,
                             int    home_sequence,
                             bool   volatile_home
                             );

// updateable interface params (for inihal pin changes typically):
void update_joint_homing_params (int    jno,
                                 double home_offset,
                                 double home_home,
                                 int    home_sequence
                                );

//---------------------------------------------------------------------
// CONTROL routines

// one-time initialization (return 0 if ok):
int  homing_init(int id,
                 double servo_period,
                 int n_joints,            // total no of joints
                 int n_extrajoints,       // extra joints (non-kins)
                 emcmot_joint_t* pjoints
                 );

// once-per-servo-period functions:
void read_homing_in_pins(int njoints);
bool do_homing(void);  //return 1 if allhomed
void write_homing_out_pins(int njoints);

// responses to EMCMOT_JOINT_HOME message:
void do_home_joint(int jno);
// per-joint controls
void do_cancel_homing(int jno);
void set_unhomed(int jno,motion_state_t motstate);

//---------------------------------------------------------------------
// QUERIES

// overall status:
bool get_allhomed(void);
bool get_homing_is_active(void);

// per-joint information:
int  get_home_sequence(int jno); //return s
bool get_homing(int jno);
bool get_homed(int jno);
bool get_index_enable(int jno);
bool get_home_needs_unlock_first(int jno);
bool get_home_is_idle(int jno);
bool get_home_is_synchronized(int jno);
bool get_homing_at_index_search_wait(int jno);

//---------------------------------------------------------------------
// Module interface
// motmod provided ptrs for functions called by homing:
void homeMotFunctions(void(*pSetRotaryUnlock)(int,int)
                     ,int( *pGetRotaryUnlock)(int)
                     );

#endif /* HOMING_H */
