
#ifndef AXIS_H
#define AXIS_H

#include "rtapi_bool.h"
#include "hal.h"

#ifdef __cplusplus
extern "C" {
#endif

void axis_init_all(void);
void axis_initialize_external_offsets(void);
int axis_init_hal_io(int mot_comp_id);

void axis_handle_jogwheels(bool motion_teleop_flag, bool motion_enable_flag, bool homing_is_active);
bool axis_plan_external_offsets(double servo_period, bool motion_enable_flag, bool all_homed);
void axis_check_constraints(double pos[], int failing_axes[]);

void axis_jog_cont(int axis_num, double vel, long servo_period);
void axis_jog_incr(int axis_num, double offset, double vel, long servo_period);
void axis_jog_abs(int axis_num, double offset, double vel);
bool axis_jog_abort_all(bool immediate);
bool axis_jog_abort(int axis_num, bool immediate);
bool axis_jog_is_active(void);

void axis_output_to_hal(double *pcmd_p[]);

void axis_set_max_pos_limit(int axis_num, double maxLimit);
void axis_set_min_pos_limit(int axis_num, double minLimit);
void axis_set_vel_limit(int axis_num, double vel);
void axis_set_acc_limit(int axis_num, double acc);
void axis_set_ext_offset_vel_limit(int axis_num, double ext_offset_vel);
void axis_set_ext_offset_acc_limit(int axis_num, double ext_offset_acc);
void axis_set_locking_joint(int axis_num, int joint);

double axis_get_min_pos_limit(int axis_num);
double axis_get_max_pos_limit(int axis_num);
double axis_get_vel_limit(int axis_num);
double axis_get_acc_limit(int axis_num);
int axis_get_locking_joint(int axis_num);
double axis_get_compound_velocity(void);
double axis_get_ext_offset_curr_pos(int axis_num);

double axis_get_teleop_vel_cmd(int axis_num);

void axis_sync_teleop_tp_to_carte_pos(int extfactor, double *pcmd_p[]);
void axis_sync_carte_pos_to_teleop_tp(int extfactor, double *pcmd_p[]);
void axis_apply_ext_offsets_to_carte_pos(int extfactor, double *pcmd_p[]);

int axis_update_coord_with_bound(double *pcmd_p[], double servo_period);

int axis_calc_motion(double servo_period);


#ifdef __cplusplus
}
#endif
#endif /* AXIS_H */
