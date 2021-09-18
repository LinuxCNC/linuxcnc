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

// SEQUENCE states
typedef enum {
  HOME_SEQUENCE_IDLE = 0,        // valid start state
  HOME_SEQUENCE_START,           // valid start state
  HOME_SEQUENCE_DO_ONE_JOINT,    // valid start state
  HOME_SEQUENCE_DO_ONE_SEQUENCE, // valid start state
  HOME_SEQUENCE_START_JOINTS,    // homing.c internal usage
  HOME_SEQUENCE_WAIT_JOINTS,     // homing.c internal usage
} home_sequence_state_t;

//---------------------------------------------------------------------
// INTERFACE routines

// per-joint interface parameters (one-time setup)
extern void set_joint_homing_params(int    jno,
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

// one-time initialization:
extern void homing_init(void);
extern int  export_joint_home_pins(int njoints,int id);

// once-per-servo-period functions:
extern void read_homing_in_pins(int njoints);
extern void do_homing_sequence(void);
extern void do_homing(void);
extern void write_homing_out_pins(int njoints);

// overall sequence control:
extern void set_home_sequence_state(home_sequence_state_t);

// per-joint control of internal state machine:
extern void set_home_start(int jno);
extern void set_home_abort(int jno);
extern void set_home_idle( int jno);

// per-joint set status items:
extern void set_joint_homing( int jno, bool value);
extern void set_joint_homed(  int jno, bool value);
extern void set_joint_at_home(int jno, bool value);

//---------------------------------------------------------------------
// QUERIES

// overall status:
extern home_sequence_state_t get_home_sequence_state(void);
extern bool get_homing_is_active(void);

// per-joint information:
extern int  get_home_sequence(int jno);
extern bool get_homing(int jno);
extern bool get_homed(int jno);
extern bool get_index_enable(int jno);
extern bool get_home_is_volatile(int jno);
extern bool get_home_needs_unlock_first(int jno);
extern bool get_home_is_idle(int jno);
extern bool get_homing_at_index_search_wait(int jno);
extern bool get_home_is_synchronized(int jno);
//---------------------------------------------------------------------

#endif /* HOMING_H */
