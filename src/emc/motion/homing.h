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
//
// A homing module (default: homemod) provides all functions declared
// below.  motmod resolves the symbols at load time, so a custom module
// named by [EMCMOT]HOMEMOD must export every one of them.  The
// homecomp component (src/hal/components/homecomp.comp) is a buildable
// template implementing this interface.

// per-joint interface parameters (one-time setup)
// Called once per joint with the homing values from the INI file
// [JOINT_N] section (HOME, HOME_OFFSET, velocities, HOME_FLAGS,
// HOME_SEQUENCE, VOLATILE_HOME).
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
// Runtime update of the subset of homing parameters that may change
// after setup (offset, home position, sequence).
void update_joint_homing_params (int    jno,
                                 double home_offset,
                                 double home_home,
                                 int    home_sequence
                                );

//---------------------------------------------------------------------
// CONTROL routines

// one-time initialization (return 0 if ok):
// Called from motmod's rtapi_app_main().  'id' is motmod's HAL
// component id, so pins created here are owned by motmod.  'pjoints'
// points at motmod's joint array.  The module creates its HAL pins
// (joint.N.home-sw-in, joint.N.homed, ...) here.
int  homing_init(int id,
                 double servo_period,
                 int n_joints,            // total no of joints
                 int n_extrajoints,       // extra joints (non-kins)
                 emcmot_joint_t* pjoints
                 );

// once-per-servo-period functions:
// read_homing_in_pins(): called at the start of every servo period;
// latch HAL input pins (home switches, index-enable, custom inputs).
void read_homing_in_pins(int njoints);
// do_homing(): called every servo period while motion is in FREE
// mode; advances the homing sequence and the per-joint state
// machines.  Returns 1 on the transition to all-homed so motmod can
// switch from FREE to teleop mode.
bool do_homing(void);  //return 1 if allhomed
// write_homing_out_pins(): called at the end of every servo period;
// push internal state to HAL output pins (homed, homing, home-state).
void write_homing_out_pins(int njoints);

// responses to EMCMOT_JOINT_HOME message:
// jno == -1 requests home-all (start the homing sequence).
void do_home_joint(int jno);
// per-joint controls
// Abort an in-progress homing of joint jno.
void do_cancel_homing(int jno);
// Mark joint(s) unhomed.  jno == -1 unhomes all joints, jno == -2
// unhomes joints with VOLATILE_HOME set.  motstate guards against
// unhoming an extrajoint while motion is enabled.
void set_unhomed(int jno,motion_state_t motstate);

//---------------------------------------------------------------------
// QUERIES

// overall status:
// get_allhomed(): true if every active joint is homed.
bool get_allhomed(void);
// get_homing_is_active(): true while any homing sequence or per-joint
// homing state machine is in progress.
bool get_homing_is_active(void);

// per-joint information:
int  get_home_sequence(int jno); //return s
// get_homing(): joint jno is currently running its homing state machine.
bool get_homing(int jno);
// get_homed(): joint jno has completed homing.
bool get_homed(int jno);
// get_index_enable(): state of the index-enable handshake for joint
// jno (set by the homing module, cleared by the encoder driver).
bool get_index_enable(int jno);
// get_home_needs_unlock_first(): joint jno has HOME_UNLOCK_FIRST set
// (rotary axis must be unlocked before homing).
bool get_home_needs_unlock_first(int jno);
// get_home_is_idle(): joint jno's homing state machine is HOME_IDLE.
bool get_home_is_idle(int jno);
// get_home_is_synchronized(): joint jno homes synchronized with other
// joints (shares a negative home_sequence).
bool get_home_is_synchronized(int jno);
// get_homing_at_index_search_wait(): joint jno's state machine waits
// at HOME_INDEX_SEARCH_WAIT (used for index handling).
bool get_homing_at_index_search_wait(int jno);

//---------------------------------------------------------------------
// Module interface
// motmod provided ptrs for functions called by homing:
// Called once by motmod before homing_init() to hand the homing
// module the rotary unlock/lock callbacks, so the module can unlock
// rotaries without including motion internals.
void homeMotFunctions(void(*pSetRotaryUnlock)(int,int)
                     ,int( *pGetRotaryUnlock)(int)
                     );

#endif /* HOMING_H */
